#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include "iomanager.h"
#include "macro.h"
#include "log.h"

namespace azure {

static azure::Logger::ptr g_logger = AZURE_LOG_NAME("system");

IOManager::FdContext::EventContext &IOManager::FdContext::getContext(IOManager::Event event) {
    switch(event) {
        case IOManager::READ:
            return read;
        case IOManager::WRITE:
            return write;
        default:
            AZURE_ASSERT2(false, "getContext");
    }
}

void IOManager::FdContext::resetContext(IOManager::FdContext::EventContext &ctx) {
    ctx.scheduler = nullptr;
    ctx.fiber.reset();
    ctx.cb = nullptr;
}

void IOManager::FdContext::triggerEvent(IOManager::Event event) {
    AZURE_ASSERT(events & event);
    events = (Event)(events & ~event);
    EventContext &ctx = getContext(event);
    if(ctx.cb) {
        ctx.scheduler->schedule(&ctx.cb);
    }
    else {
        ctx.scheduler->schedule(&ctx.fiber);
    }
    ctx.scheduler = nullptr;
}

// 使用信号量会阻塞线程，无法实现异步IO
IOManager::IOManager(size_t thread_num, bool use_caller, const std::string &name) 
    : Scheduler(thread_num, use_caller, name) {
    m_epollfd = epoll_create(5000);
    AZURE_ASSERT(m_epollfd > 0);

    int rt = pipe(m_tickleFds);         // rt, return
    AZURE_ASSERT(!rt);

    epoll_event event;
    memset(&event, 0, sizeof(event));
    event.events = EPOLLIN | EPOLLET;   // 读事件，边缘触发
    event.data.fd = m_tickleFds[0];     // 0读1写

    rt = fcntl(m_tickleFds[0], F_SETFL, O_NONBLOCK);
    AZURE_ASSERT(!rt);

    rt = epoll_ctl(m_epollfd, EPOLL_CTL_ADD, m_tickleFds[0], &event);   // 注册管道读事件
    AZURE_ASSERT(!rt);

    contextResize(32);                  // 默认大小32

    start();                            // 默认启动
}

IOManager::~IOManager() {
    stop();
    close(m_epollfd);
    close(m_tickleFds[0]);
    close(m_tickleFds[1]);

    for(size_t i = 0; i < m_fdContexts.size(); ++i) {
        if(m_fdContexts[i]) {
            delete m_fdContexts[i];
        }
    }
}

void IOManager::contextResize(size_t size) {
    m_fdContexts.resize(size);

    for(size_t i = 0; i < m_fdContexts.size(); ++i) {
        if(!m_fdContexts[i]) {
            m_fdContexts[i] = new FdContext;
            m_fdContexts[i]->fd = i;    // 自己指定文件描述符？
        }
    }
}

// 0 success, -1 error
int IOManager::addEvent(int fd, Event event, std::function<void()> cb) {
    FdContext *fd_ctx = nullptr;
    RWMutexType::ReadLock lock(m_mutex);
    if((int)m_fdContexts.size() > fd) {
        fd_ctx = m_fdContexts[fd];
        lock.unlock();
    }
    else {
        lock.unlock();
        RWMutexType::WriteLock lock2(m_mutex);
        contextResize(fd * 1.5);
        fd_ctx = m_fdContexts[fd];
    }

    FdContext::MutexType::Lock lock2(fd_ctx->mutex);
    if(fd_ctx->events & event) {
        AZURE_LOG_ERROR(g_logger) << "addEvent assert fd=" << fd << " event=" << event << " fd_ctx.event=" << fd_ctx->events;
        AZURE_ASSERT(!(fd_ctx->events &event));
    }

    // 已有注册事件就修改（EPOLL_CTL_MOD），否则就加（EPOLL_CTL_ADD）
    int op = fd_ctx->events ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
    epoll_event epevent;
    epevent.events = EPOLLET | fd_ctx->events | event;
    epevent.data.ptr = fd_ctx;

    int rt = epoll_ctl(m_epollfd, op, fd, &epevent);    // 向 epoll_table 注册 fd_ctx 上的事件
    if(rt) {
        AZURE_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epollfd << ", " << op << ", " << fd << ", " << epevent.events
                                    << "):" << rt << " (" << errno << ") (" << strerror(errno) << ")";
        return -1;
    }

    ++m_pendingEventCount;
    fd_ctx->events = (Event)(fd_ctx->events | event);

    // AZURE_LOG_INFO(g_logger) << "fd_ctx->events=" << fd_ctx->events;

    FdContext::EventContext &event_ctx = fd_ctx->getContext(event);
    AZURE_ASSERT(!event_ctx.scheduler && !event_ctx.fiber && !event_ctx.cb);
    
    event_ctx.scheduler = Scheduler::GetThis();
    AZURE_ASSERT(event_ctx.scheduler);

    if(cb) {
        event_ctx.cb.swap(cb);
    }
    else {
        event_ctx.fiber = Fiber::GetThis();
        // DEBUG
        // AZURE_ASSERT2(event_ctx.fiber->getState() == Fiber::EXEC, "state=" << event_ctx.fiber->getState());
    }
    return 0;
}

bool IOManager::delEvent(int fd, Event event) {
    RWMutexType::ReadLock lock(m_mutex);
    if((int)m_fdContexts.size() <= fd) {
        return false;
    }
    FdContext *fd_ctx = m_fdContexts[fd];
    lock.unlock();

    FdContext::MutexType::Lock lock2(fd_ctx->mutex);
    if(!(fd_ctx->events & event)) {
        return false;
    }

    Event new_events = (Event)(fd_ctx->events & ~event);
    int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
    epoll_event epevent;
    epevent.events = EPOLLET | new_events;
    epevent.data.ptr = fd_ctx;

    int rt = epoll_ctl(m_epollfd, op, fd, &epevent);
    if(rt) {
        AZURE_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epollfd << ", " << op << ", " << fd << ", " << epevent.events
                                    << "):" << rt << " (" << errno << ") (" << strerror(errno) << ")";
        return false;
    }

    --m_pendingEventCount;
    fd_ctx->events = new_events;
    FdContext::EventContext &event_ctx = fd_ctx->getContext(event);
    fd_ctx->resetContext(event_ctx);    // 清理 scheduler、fiber、cb
    return true;
}

// 强制触发执行
bool IOManager::cancelEvent(int fd, Event event) {
    RWMutexType::ReadLock lock(m_mutex);
    if((int)m_fdContexts.size() <= fd) {
        return false;
    }
    FdContext *fd_ctx = m_fdContexts[fd];
    lock.unlock();

    FdContext::MutexType::Lock lock2(fd_ctx->mutex);
    if(!(fd_ctx->events & event)) {
        return false;
    }

    Event new_events = (Event)(fd_ctx->events & ~event);
    int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
    epoll_event epevent;
    epevent.events = EPOLLET | new_events;
    epevent.data.ptr = fd_ctx;

    int rt = epoll_ctl(m_epollfd, op, fd, &epevent);
    if(rt) {
        AZURE_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epollfd << ", " << op << ", " << fd << ", " << epevent.events
                                    << "):" << rt << " (" << errno << ") (" << strerror(errno) << ")";
        return false;
    }

    // FdContext::EventContext &event_ctx = fd_ctx->getContext(event);
    fd_ctx->triggerEvent(event);
    --m_pendingEventCount;
    return true;
}

bool IOManager::cancelAll(int fd) {
    RWMutexType::ReadLock lock(m_mutex);
    if((int)m_fdContexts.size() <= fd) {
        return false;
    }
    FdContext *fd_ctx = m_fdContexts[fd];
    lock.unlock();

    FdContext::MutexType::Lock lock2(fd_ctx->mutex);
    if(!fd_ctx->events) {
        return false;
    }

    int op = EPOLL_CTL_DEL;
    epoll_event epevent;
    epevent.events = 0;
    epevent.data.ptr = fd_ctx;

    int rt = epoll_ctl(m_epollfd, op, fd, &epevent);
    if(rt) {
        AZURE_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epollfd << ", " << op << ", " << fd << ", " << epevent.events
                                    << "):" << rt << " (" << errno << ") (" << strerror(errno) << ")";
        return false;
    }

    if(fd_ctx->events & READ) {
        fd_ctx->triggerEvent(READ);
        --m_pendingEventCount;
    }

    if(fd_ctx->events & WRITE) {
        fd_ctx->triggerEvent(WRITE);
        --m_pendingEventCount;
    }
    AZURE_ASSERT(fd_ctx->events == 0);
    return true;
}

IOManager *IOManager::GetThis() {
    return dynamic_cast<IOManager*>(Scheduler::GetThis());
}

void IOManager::tickle() {
    if(!hasIdleThreads()) {
        return;
    }
    int rt = write(m_tickleFds[1], "T", 1);
    AZURE_ASSERT(rt == 1);
}

bool IOManager::stopping(uint64_t &timeout) {
    timeout = getNextTimer();
    return timeout == ~0ull && m_pendingEventCount == 0 && Scheduler::stopping();
}

bool IOManager::stopping() {
    uint64_t timeout = 0;
    return stopping(timeout);
}

// IOManager 继承自 Scheduler，所以逻辑是 IOManager::run --> 有任务 ？ 执行任务协程 ：执行 IOManager::idle
// 把触发的回调函数（超时事件和io事件）添加到协程任务队列里
// run负责执行事件，idle负责添加事件，每个线程都会这样做
void IOManager::idle() {
    AZURE_LOG_INFO(g_logger) << "iomanager idle";

    epoll_event *events = new epoll_event[64]();
    std::shared_ptr<epoll_event> shared_events(events, [](epoll_event *ptr){delete[] ptr;});
    
    int rt = 0;
    // 该idle协程永远不会终止，只会切来切去
    while(true) {
        uint64_t next_timeout = 0;

        if(stopping(next_timeout)) {
            if(next_timeout == ~0ull) {
                AZURE_LOG_INFO(g_logger) << "name=" << getName() << "idle stopping exit";
                break;
            }
        }
        do {
            static const int MAX_TIMEOUT = 3000;    // 毫秒级
            if(next_timeout != ~0ull) {
                next_timeout = (int)next_timeout > MAX_TIMEOUT ? MAX_TIMEOUT : next_timeout;
            }
            else {
                next_timeout = MAX_TIMEOUT;
            }
            rt = epoll_wait(m_epollfd, events, 64, (int)next_timeout);

            if(rt < 0 && errno == EINTR) {  // EINTER 中断了
                ;
            }   
            else {
                break;
            }
        } while(true);

        // 获取满足条件的超时事件
        std::vector<std::function<void()>> cbs;
        listExpiredCb(cbs);
        if(!cbs.empty()) {
            schedule(cbs.begin(), cbs.end());   // 添加超时任务
            cbs.clear();
        }

        for(int i = 0; i < rt; ++i) {
            epoll_event &event = events[i];
            if(event.data.fd == m_tickleFds[0]) {
                uint8_t dummy;
                while(read(m_tickleFds[0], &dummy, 1) == 1);
                continue;
            }

            FdContext *fd_ctx = (FdContext *)event.data.ptr;
            FdContext::MutexType::Lock lock(fd_ctx->mutex);
            if(event.events & (EPOLLERR | EPOLLHUP)) {  // 错误或中断
                event.events |= EPOLLIN | EPOLLOUT;     // 唤醒读和写事件
            }

            int real_events = NONE;
            if(event.events & EPOLLIN) {
                real_events |= READ;
            }
            if(event.events & EPOLLOUT) {
                real_events |= WRITE;
            }

            if((fd_ctx->events & real_events) == NONE) {
                continue;
            }

            int left_events = (fd_ctx->events & ~real_events);
            int op = left_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
            event.events = EPOLLET | left_events;

            int rt2 = epoll_ctl(m_epollfd, op, fd_ctx->fd, &event);
            if(rt2) {
                AZURE_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epollfd << ", " << op << ", " << fd_ctx->fd << ", " << event.events
                                        << "):" << rt2 << " (" << errno << ") (" << strerror(errno) << ")";
                continue;
            }

            if(real_events & READ) {
                fd_ctx->triggerEvent(READ);
                --m_pendingEventCount;
            }
            if(real_events & WRITE) {
                fd_ctx->triggerEvent(WRITE);
                --m_pendingEventCount;
            }
        }

        Fiber::ptr cur = Fiber::GetThis();
        auto raw_ptr = cur.get();
        cur.reset();

        raw_ptr->swapOut();
    }   
}

void IOManager::onTimerInsertedAtFront() {
    tickle();
}

}