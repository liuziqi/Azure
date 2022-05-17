#include <dlfcn.h>
#include "hook.h"
#include "fiber.h"
#include "iomanager.h"
#include "fdmanager.h"
#include "log.h"
#include "config.h"

static azure::Logger::ptr g_logger = AZURE_LOG_NAME("system");

namespace azure {

static azure::ConfigVar<int>::ptr g_tcp_connect_timeout = azure::Config::Lookup("tcp.connect.timeout", 5000, "tcp connect timeout");

static thread_local bool t_hook_enable = false;     // 标记当前线程是否被hook

#define HOOK_FUN(XX) \
    XX(sleep) \
    XX(usleep) \
    XX(nanosleep) \
    XX(socket) \
    XX(connect) \
    XX(accept) \
    XX(read) \
    XX(readv) \
    XX(recv) \
    XX(recvfrom) \
    XX(recvmsg) \
    XX(write) \
    XX(writev) \
    XX(send) \
    XX(sendto) \
    XX(sendmsg) \
    XX(close) \
    XX(fcntl) \
    XX(ioctl) \
    XX(getsockopt) \
    XX(setsockopt)

void hook_init() {
    static bool is_inited = false;
    if(is_inited) {
        return;
    }
// 从函数库里取出原始的函数赋值给函数指针
#define XX(name) name ## _f = (name ## _fun)dlsym(RTLD_NEXT, #name);
    HOOK_FUN(XX)
#undef XX
}

static uint64_t s_connect_timeout = -1;

struct _HOOKIniter {
    _HOOKIniter() {
        hook_init();
        s_connect_timeout = g_tcp_connect_timeout->getValue();
        g_tcp_connect_timeout->addListener([](const int &old_value, const int &new_value){
            AZURE_LOG_INFO(g_logger) << "tcp connect timeout changed from " << old_value << "to " << new_value;
            s_connect_timeout = new_value;
        });
    }
};

static _HOOKIniter s_hook_initer;   // 进入main函数之前初始化hook

bool is_hook_enable() {
    return t_hook_enable;
}

void set_hook_enable(bool flag) {
    t_hook_enable = flag;
}

}

struct timer_info {
    int cancelled = 0;
};

// 一般来说，是一个协程在执行该函数
template<typename OriginFun, typename ... Args>
static ssize_t do_io(int fd, OriginFun fun, const char *hook_fun_name, uint32_t event, int timeout_so, Args &&... args) {
    if(!azure::t_hook_enable) {
        // NOTE std::forward通常是用于完美转发
        // 完美转发主要目的一般都是为了避免拷贝，同时调用正确的函数版本
        // 它会将输入的参数传递到下一个函数中
        // 如果输入的参数是左值，那么传递给下一个函数的参数的也是左值
        // 如果输入的参数是右值，那么传递给下一个函数的参数的也是右值
        // NOTE && 是引用折叠，其规则是：
        // && && -> &&, & && -> &, & & -> &, && & -> &
        return fun(fd, std::forward<Args>(args)...);
    }

    // AZURE_LOG_DEBUG(g_logger) << "do_io<" << hook_fun_name << ">";

    // 假设不存在就不是socket
    azure::FdCtx::ptr ctx = azure::FdMgr::GetInstance()->get(fd);
    if(!ctx) {
        return fun(fd, std::forward<Args>(args)...);
    }

    if(ctx->isClosed()) {
        errno = EBADF;
        return -1;
    }

    // 不是socket或者用户设置了nonblock则执行原来的函数
    if(!ctx->isSocket() || ctx->getUserNonblock()) {
        return fun(fd, std::forward<Args>(args)...);
    }

    uint64_t to = ctx->getTimeout(timeout_so);
    std::shared_ptr<timer_info> tinfo(new timer_info);

retry:
    ssize_t n = fun(fd, std::forward<Args>(args)...);   // read 或 accept 等的返回值
    while(-1 == n && errno == EINTR) {                  // 中断重试
        n = fun(fd, std::forward<Args>(args)...);
    }
    if(-1 == n && errno == EAGAIN) {                    // 非阻塞立即返回，需要做异步操作

        // AZURE_LOG_DEBUG(g_logger) << "do_io<" << hook_fun_name << ">";

        azure::IOManager *iom = azure::IOManager::GetThis();
        azure::Timer::ptr timer;
        std::weak_ptr<timer_info> winfo(tinfo);         // weak info

        if((uint64_t)-1 != to) {                        // m_recvTimeout 和 m_sendTimeout 的初始值是 (uint64_t)-1
            timer = iom->addConditionTimer(to, [winfo, fd, iom, event](){
                auto t = winfo.lock();                  // 超时事件放入定时器，当定时器到达时间，强制cancel
                if(!t || t->cancelled) {
                    return;
                }
                t->cancelled = ETIMEDOUT;
                iom->cancelEvent(fd, (azure::IOManager::Event)(event));
            }, winfo);
        }

        int rt = iom->addEvent(fd, (azure::IOManager::Event)(event));
        if(rt) {                                       // 执行失败，取消定时器
            AZURE_LOG_ERROR(g_logger) << hook_fun_name << " addEvent(" << fd << ", " << event << ")";
            if(timer) {
                timer->cancel();
            }
            return - 1;
        }
        else {
            AZURE_LOG_DEBUG(g_logger) << "do_io<" << hook_fun_name << "> YieldToHold";

            azure::Fiber:: YieldToHold();               // 执行成功，让出当前协程执行时间，被唤醒有两种条件：1. 定时器超时；2. 完成了io

            AZURE_LOG_DEBUG(g_logger) << "do_io<" << hook_fun_name << "> Exec";

            if(timer) {
                timer->cancel();
            }
            if(tinfo->cancelled) {
                errno = tinfo->cancelled;
                return -1;
            }

            goto retry;
        }
    }
    // io执行成功，返回n
    return n;
}

extern "C" {
#define XX(name) name ## _fun name ##_f = nullptr;
    HOOK_FUN(XX)
#undef XX

// sleep
unsigned int sleep(unsigned int seconds) {
    if(!azure::t_hook_enable) {
        return sleep_f(seconds);
    }
    azure::Fiber::ptr fiber = azure::Fiber::GetThis();
    azure::IOManager *iom = azure::IOManager::GetThis();
    // iom->addTimer(seconds * 1000, std::bind(&azure::IOManager::schedule, iom, fiber));
    // iom->addTimer(seconds * 1000, [&](){iom->schedule(fiber);});
    // NOTE C::* 是指向数据成员的指针类型
    iom->addTimer(seconds * 1000, std::bind((void(azure::Scheduler::*)(azure::Fiber::ptr, int thread))
                    &azure::IOManager::schedule, iom, fiber, -1));
    azure::Fiber::YieldToHold();
    return 0;
}

int usleep(useconds_t usec) {
    if(!azure::t_hook_enable) {
        return usleep_f(usec);
    }
    azure::Fiber::ptr fiber = azure::Fiber::GetThis();
    azure::IOManager *iom = azure::IOManager::GetThis();
    // iom->addTimer(usec / 1000, std::bind(&azure::IOManager::schedule, iom, fiber));
    // iom->addTimer(usec / 1000, [&](){iom->schedule(fiber);});
    iom->addTimer(usec / 1000, std::bind((void(azure::Scheduler::*)(azure::Fiber::ptr, int thread))
                    &azure::IOManager::schedule, iom, fiber, -1));
    azure::Fiber::YieldToHold();
    return 0;
}

int nanosleep(const struct timespec *req, struct timespec *rem) {
    if(!azure::t_hook_enable) {
        return nanosleep_f(req, rem);
    }
    int timeout_ms = req->tv_sec * 1000 + req->tv_nsec / 1000 / 1000;
    azure::Fiber::ptr fiber = azure::Fiber::GetThis();
    azure::IOManager *iom = azure::IOManager::GetThis();
    // iom->addTimer(usec / 1000, std::bind(&azure::IOManager::schedule, iom, fiber));
    iom->addTimer(timeout_ms, [&](){iom->schedule(fiber);});
    azure::Fiber::YieldToHold();
    return 0;
}

// socket
int socket(int domain, int type, int protocol) {
    if(!azure::t_hook_enable) {
        return socket_f(domain, type, protocol);
    }
    int fd = socket_f(domain, type, protocol);
    if(fd == -1) {
        return fd;
    }
    azure::FdMgr::GetInstance()->get(fd, true);
    return fd;
}

int connect_with_timeout(int fd, const struct sockaddr *addr, socklen_t addrlen, uint64_t timeout_ms) {
    if(!azure::t_hook_enable) {
        return connect_f(fd, addr, addrlen);
    }
    azure::FdCtx::ptr ctx = azure::FdMgr::GetInstance()->get(fd);
    if(!ctx || ctx->isClosed()) {
        errno = EBADF;
        return -1;
    }

    if(!ctx->isSocket()) {
        return connect_f(fd, addr, addrlen);
    }

    if(ctx->getUserNonblock()) {
        return connect_f(fd, addr, addrlen);
    }

    int n = connect_f(fd, addr, addrlen);
    if(n == 0) {
        return 0;
    }
    else if(n != -1 || errno != EINPROGRESS) {
        return n;
    }

    azure::IOManager *iom = azure::IOManager::GetThis();
    azure::Timer::ptr timer;
    std::shared_ptr<timer_info> tinfo(new timer_info);
    std::weak_ptr<timer_info> winfo(tinfo);

    if(timeout_ms != (uint64_t)-1) {
        timer = iom->addConditionTimer(timeout_ms, [winfo, fd, iom](){
            auto t = winfo.lock();
            if(!t || t->cancelled) {
                return;
            }
            t->cancelled = ETIMEDOUT;
            iom->cancelEvent(fd, azure::IOManager::WRITE);
        }, winfo);
    }

    int rt = iom->addEvent(fd, azure::IOManager::WRITE);
    if(rt == 0) {   // 添加成功
        azure::Fiber::YieldToHold();
        // LEARN 什么情况下返回来继续执行
        if(timer) {
            timer->cancel();
        }
        if(tinfo->cancelled) {
            errno = tinfo->cancelled;
            return -1;
        }
    }
    else {          // 添加失败
        if(timer) {
            timer->cancel();
        }
        AZURE_LOG_ERROR(g_logger) << "connect addEvent(" << fd << ", WRITE) error";
    }

    int error = 0;
    socklen_t len = sizeof(int);
    if(-1 == getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len)) {
        return -1;
    }
    if(!error) {
        return 0;
    }
    else {
        errno = error;
        return -1;
    }
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    return connect_with_timeout(sockfd, addr, addrlen, azure::s_connect_timeout);
}

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    int fd = do_io(sockfd, accept_f, "accept", azure::IOManager::READ, SO_RCVTIMEO, addr, addrlen);
    if(fd >= 0) {
        azure::FdMgr::GetInstance()->get(fd, true); // 初始化fd
    }
    return fd;
}

// read
ssize_t read(int fd, void *buf, size_t count) {
    return do_io(fd, read_f, "read", azure::IOManager::READ, SO_RCVTIMEO, buf, count);
}

ssize_t readv(int fd, const struct iovec *iov, int iovcnt) {
    return do_io(fd, readv_f, "readv", azure::IOManager::READ, SO_RCVTIMEO, iov, iovcnt);
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
    return do_io(sockfd, recv_f, "recv", azure::IOManager::READ, SO_RCVTIMEO, buf, len, flags);
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen) {
    return do_io(sockfd, recvfrom_f, "recvfrom", azure::IOManager::READ, SO_RCVTIMEO, buf, len, flags, src_addr, addrlen);
}

ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags) {
    return do_io(sockfd, recvmsg_f, "recvmsg", azure::IOManager::READ, SO_RCVTIMEO, msg, flags);
}

//write
ssize_t write(int fd, const void *buf, size_t count) {
    return do_io(fd, write_f, "write", azure::IOManager::WRITE, SO_SNDTIMEO, buf, count);
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt) {
    return do_io(fd, writev_f, "writev", azure::IOManager::WRITE, SO_SNDTIMEO, iov, iovcnt);
}

ssize_t send(int sockfd, const void *buf, size_t len, int flags) {
    return do_io(sockfd, send_f, "send", azure::IOManager::WRITE, SO_SNDTIMEO, buf, len, flags);
}

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen) {
    return do_io(sockfd, sendto_f, "sendto", azure::IOManager::WRITE, SO_SNDTIMEO, buf, len, flags, dest_addr, addrlen);
}

ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags) {
    return do_io(sockfd, sendmsg_f, "sendmsg", azure::IOManager::WRITE, SO_SNDTIMEO, msg, flags);
}

// close
int close(int fd) {
    if(!azure::t_hook_enable) {
        return close_f(fd);
    }
    azure::FdCtx::ptr ctx = azure::FdMgr::GetInstance()->get(fd);
    if(ctx) {
        auto iom = azure::IOManager::GetThis();
        if(iom) {
            iom->cancelAll(fd);
        }
        azure::FdMgr::GetInstance()->del(fd);
    }
    return close_f(fd);
}

// socket option
int fcntl(int fd, int cmd, ... /* arg */ ) {
    // 针对其中几种cmd进行hook
    va_list va;
    va_start(va, cmd);
    switch(cmd) {
        case F_SETFL:
            {
                int arg = va_arg(va, int);
                va_end(va);
                azure::FdCtx::ptr ctx = azure::FdMgr::GetInstance()->get(fd);
                if(!ctx || ctx->isClosed() || !ctx->isSocket()) {
                    return fcntl_f(fd, cmd, arg);
                }
                ctx->setUserNonblock(arg & O_NONBLOCK);
                if(ctx->getSysNonblock()) {
                    arg |= O_NONBLOCK;
                }
                else {
                    arg &= ~O_NONBLOCK;
                }
                return fcntl_f(fd, cmd, arg);
            }
            break;
        case F_GETFL:
            {
                va_end(va);
                int arg = fcntl_f(fd, cmd);
                azure::FdCtx::ptr ctx = azure::FdMgr::GetInstance()->get(fd);
                if(!ctx || ctx->isClosed() || !ctx->isSocket()) {
                    return arg;
                }
                if(ctx->getUserNonblock()) {
                    return arg | O_NONBLOCK;
                }
                else {
                    return arg & ~O_NONBLOCK;
                }
            }
            break;
        // int
        case F_DUPFD:
        case F_DUPFD_CLOEXEC:
        case F_SETFD:
        case F_SETOWN:
        case F_SETSIG:
        case F_SETLEASE:
        case F_NOTIFY:
        case F_SETPIPE_SZ:
            {
                int arg = va_arg(va, int);
                va_end(va);
                return fcntl_f(fd, cmd, arg);
            }
            break;

        // void
        case F_GETFD:
        case F_GETOWN:
        case F_GETSIG:
        case F_GETLEASE:
        case F_GETPIPE_SZ:
            {
                va_end(va);
                return fcntl_f(fd, cmd);
            }
            break;

        // flock*
        case F_SETLK:
        case F_SETLKW:
        case F_GETLK:
            {
                struct flock *arg = va_arg(va, struct flock*);
                va_end(va);
                return fcntl_f(fd, cmd, arg);
            }
            break;

        // f_owner_ex *
        case F_GETOWN_EX:
        case F_SETOWN_EX:
            {
                struct f_owner_ex *arg = va_arg(va, struct f_owner_ex*);
                va_end(va);
                return fcntl_f(fd, cmd, arg);
            }
            break;
        
        default:
            va_end(va);
            return fcntl_f(fd, cmd);
    }
}

int ioctl(int fd, unsigned long request, ...) {
    va_list va;
    va_start(va, request);
    void *arg = va_arg(va, void*);
    va_end(va);

    if(FIONBIO == request) {
        bool user_nonblock = !!*(int*)arg;
        azure::FdCtx::ptr ctx = azure::FdMgr::GetInstance()->get(fd);
        if(!ctx || ctx->isClosed() || !ctx->isSocket()) {
            return ioctl_f(fd, request, arg);
        }
        ctx->setUserNonblock(user_nonblock);
    }
    return ioctl_f(fd, request, arg);
}

int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen) {
    return getsockopt_f(sockfd, level, optname, optval, optlen);
}

int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) {
    if(!azure::t_hook_enable) {
        return setsockopt(sockfd, level, optname, optval, optlen);
    }
    if(level == SOL_SOCKET) {
        if(optname == SO_RCVTIMEO || optname == SO_SNDTIMEO) {
            azure::FdCtx::ptr ctx = azure::FdMgr::GetInstance()->get(sockfd);
            if(ctx) {
                const timeval *tv = (const timeval*)optval;
                ctx->setTimeout(optname, tv->tv_sec * 1000 + tv->tv_usec / 1000);
            }
        }
    }
    return setsockopt_f(sockfd, level, optname, optval, optlen);
}

}