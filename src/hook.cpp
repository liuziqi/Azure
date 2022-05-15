#include <dlfcn.h>
#include "hook.h"
#include "fiber.h"
#include "iomanager.h"
#include "fdmanager.h"
#include "log.h"

namespace azure {

static azure::Logger::ptr g_logger = AZURE_LOG_NAME("system");

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

struct _HOOKIniter {
    _HOOKIniter() {
        hook_init();
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

extern "C" {
#define XX(name) name ## _fun name ##_f = nullptr;
    HOOK_FUN(XX)
#undef XX

struct timer_info {
    int cancelled = 0;
};

template<typename OriginFun, typename ... Args>
static ssize_t do_io(int fd, OriginFun fun, const char *hook_fun_name, uint32_t event, int timeout_so, Args &&... args) {
    if(!azure::t_hook_enable) {
        return fun(fd, std::forward<Args>(args)...);

    }

    // 假设不存在就不是socket
    azure::FdCtx::ptr ctx = azure::FdMgr::GetInstance()->get(fd);
    if(!ctx) {
        return fun(fd, std::forward<Args>(args)...);
    }

    if(ctx->isClose()) {
        errno = EBADF;
        return -1;
    }

    if(!ctx->isSocket() || ctx->getUserNonblock()) {
        return fun(fd, std::forward<Args>(args)...);
    }

    uint64_t to = ctx->getTimeout(timeout_so);
    std::shared_ptr<timer_info> tinfo(new timer_info);

retry:
    ssize_t n = fun(fd, std::forward<Args>(args)...);
    while(-1 == n && errno == EINTR) {  // 中断重试
        n = fun(fd, std::forward<Args>(args)...);
    }
    if(-1 == n && errno == EAGAIN) {    // 阻塞
        azure::IOManager *iom = azure::IOManager::GetThis();
        azure::Timer::ptr timer;
        std::weak_ptr<timer_info> winfo(tinfo); // weak info

        // 当定时器到达时间，强制cancel
        if((uint64_t)-1 != to) {
            timer = iom->addConditionTimer(to, [winfo, fd, iom, event](){
                auto t = winfo.lock();
                if(!t || t->cancelled) {
                    return;
                }
                t->cancelled = ETIMEDOUT;
                iom->cancelEvent(fd, (azure::IOManager::Event)(event));
            }, winfo);
        }

        int rt = iom->addEvent(fd, (azure::IOManager::Event)(event));
        if(rt) {
            AZURE_LOG_ERROR(g_logger) << hook_fun_name << " addEvent(" << fd << ", " << event << ")";
            if(timer) {
                timer->cancel();
            }
            return - 1;
        }
        else {
            azure::Fiber:: YieldToHold();
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

unsigned int sleep(unsigned int seconds) {
    if(!azure::t_hook_enable) {
        return sleep_f(seconds);
    }
    azure::Fiber::ptr fiber = azure::Fiber::GetThis();
    azure::IOManager *iom = azure::IOManager::GetThis();
    // iom->addTimer(seconds * 1000, std::bind(&azure::IOManager::schedule, iom, fiber));
    iom->addTimer(seconds * 1000, [&](){iom->schedule(fiber);});
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
    iom->addTimer(usec / 1000, [&](){iom->schedule(fiber);});
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

}