#include "scheduler.h"
#include "log.h"
#include "macro.h"
#include "hook.h"

namespace azure {

static azure::Logger::ptr g_logger = AZURE_LOG_NAME("system");

static thread_local Scheduler *t_scheduler = nullptr;       // 当前使用中的协程调度器（一般只存在一个协程调度器）
static thread_local Fiber *t_scheduler_fiber = nullptr;     // 调度协程，每个线程的主协程会被设置为调度线程，用于协程切换

// 这里的 use_caller 就是说把 Scheduler 即主线程也当作工作线程之一
// 主线程作为工作线程，与其他工作线程的区别在于，主线程的 root 协程负责调度，
// 而工作线程则由线程本身负责调度，这一点从对 run 方法的调用看出
// 主线程当作工作线程的好处是性能可以发挥到极致，协程切换顺序是：
// idle --> t_scheduler_fiber(m_rootFiber) --> idle 或者 
// t_threadFiber --> t_scheduler_fiber(m_rootFiber) --> idle --> t_scheduler_fiber(m_rootFiber) （在没有工作线程的情况下）
// 如果不把主线程当作工作线程，那么主线程负责 start，schedule，stop，工作线程负责负责调度，协程切换顺序是：
// t_threadFiber（MainFunc为空，执行线程逻辑）--> idle --> t_threadFiber --> idle
Scheduler::Scheduler(size_t thread_num, bool use_caller, const std::string &name) 
    : m_name(name) {
    AZURE_ASSERT(thread_num > 0);

    if(use_caller) {
        azure::Fiber::GetThis();                // 线程没有协程的话就会初始化一个主协程，用于call方法
        --thread_num;

        AZURE_ASSERT(GetThis() == nullptr);     // 构造Scheduler时必须保证不存在已构造的协程调度器
        t_scheduler = this;

        // std::bind本身是一种延迟计算的思想，将函数和参数绑定成一个对象
        // 非静态成员函数则需要传递this指针作为第一个参数
        // std::function不能封装成员函数
        m_rootFiber.reset(new Fiber(std::bind(&Scheduler::run, this), 0, true));     // 将调度协程的执行函数设置为run（调度函数）
        azure::Thread::SetName(m_name);

        t_scheduler_fiber = m_rootFiber.get();
        m_rootThreadId = azure::GetThreadId();
        m_threadIds.push_back(m_rootThreadId);
    }
    else {
        // t_scheduler = this;  // 不加这个主线程无法获取调度器
        m_rootThreadId = -1;
    }
    m_threadCount = thread_num;
}

Scheduler::~Scheduler() {
    AZURE_ASSERT(m_stopping);
    if(GetThis() == this) {
        t_scheduler = nullptr;
    }
}

Scheduler *Scheduler::GetThis() {
    // AZURE_LOG_INFO(g_logger) << "Scheduler::GetThis()=" << t_scheduler;
    return t_scheduler;
}

Fiber *Scheduler::GetMainFiber() {
    return t_scheduler_fiber;
}

void Scheduler::start() {
    MutexType::Lock lock(m_mutex);
    if(!m_stopping) {   
        return;
    }
    m_stopping = false;

    AZURE_ASSERT(m_threads.empty());

    m_threads.resize(m_threadCount);
    for(size_t i = 0; i < m_threadCount; ++i) {
        // Thread::run里存在信号量，保证构造函数返回之前线程已经运行起来，可以安全地获取到线程id
        m_threads[i].reset(new Thread(std::bind(&Scheduler::run, this), m_name + "_" + std::to_string(i)));
        m_threadIds.push_back(m_threads[i]->getId());
    }
}

void Scheduler::stop() {
    m_autostop = true;
    if(m_rootFiber && m_threadCount == 0 && ((m_rootFiber->getState() == Fiber::TERM) || m_rootFiber->getState() == Fiber::INIT)) {
        AZURE_LOG_INFO(g_logger) << this << " stopped";
        m_stopping = true;

        if(stopping()) {
            return;
        }
    }

    if(m_rootThreadId != -1) {  // 说明主线程不用来调度
        AZURE_ASSERT(GetThis() == this);
    }
    else {
        AZURE_ASSERT(GetThis() != this);
    }

    m_stopping = true;
    for(size_t i = 0; i < m_threadCount; ++i) {
        tickle();
    }

    if(m_rootFiber) {
        tickle();
    }

    if(m_rootFiber) {
        if(!stopping()) {
            m_rootFiber->call();
        }
    }

    std::vector<Thread::ptr> thrs;
    {
        MutexType::Lock lock(m_mutex);
        thrs.swap(m_threads);
    }

    for(auto &i : thrs) {
        i->join();
    }
}

void Scheduler::setThis() {
    // AZURE_LOG_INFO(g_logger) << "Scheduler::setThis()=" << this;
    t_scheduler = this;
}

// 由线程执行的函数
void Scheduler::run() {
    AZURE_LOG_INFO(g_logger) << m_name << " run";

    set_hook_enable(true);

    setThis();  // 设置当前Scheduler
    
    // 如果不是主线程，初始化主协程，赋值给当前调度协程
    // 有点蠢，按现在（p35）的写法只需要初始化一次t_scheduler_fiber就够了，切换时用到的协程
    // if(azure::GetThreadId() != m_rootThreadId) {
    //     t_scheduler_fiber = Fiber::GetThis().get(); 
    // }

    // 只初始化一次
    if(!t_scheduler_fiber) {
        t_scheduler_fiber = Fiber::GetThis().get();
    }
    
    Fiber::ptr idle_fiber(new Fiber(std::bind(&Scheduler::idle, this)));    // 所有协程任务都完成就运行idle_fiber
    Fiber::ptr cb_fiber;    // 如果取得的是callback，就用这个协程执行

    FiberAndThread ft;

    while(true) {
        ft.reset();
        bool tickle_me = false;
        bool is_active = false;
        // 从协程队列里取出一个要执行的协程
        {
            MutexType::Lock lock(m_mutex);
            auto it = m_fibers.begin();
            while(it != m_fibers.end()) {
                // 协程指定的线程不是当前线程
                if(it->thread != -1 && it->thread != azure::GetThreadId()) {
                    ++it;
                    tickle_me = true;   // 通知其他线程处理
                    continue;
                }

                AZURE_ASSERT(it->fiber || it->cb);
                if(it->fiber && it->fiber->getState() == Fiber::EXEC) {
                    ++it;
                    continue;
                }

                ft = *it;
                m_fibers.erase(it++);
                ++m_activeThreadCount;
                is_active = true;
                break;
            }
        }

        if(tickle_me) {
            tickle();
        }

        if(ft.fiber && ft.fiber->getState() != Fiber::TERM && ft.fiber->getState() != Fiber::EXCEPT) {
            ft.fiber->swapIn();
            --m_activeThreadCount;  // 已经从协程里返回了

            if(ft.fiber->getState() == Fiber::READY) {  // 如果执行了YieldToReady，再次入队等待调度
                schedule(ft.fiber); // DEBUG 可以指定运行该协程的线程
            }
            else if(ft.fiber->getState() != Fiber::TERM && ft.fiber->getState() != Fiber::EXCEPT) {     // 让出了执行时间
                ft.fiber->m_state = Fiber::HOLD;    // DEBUG 不用入队吗？
            }
            ft.reset();             // 析构Fiber
        }
        else if(ft.cb) {
            if(cb_fiber) {
                cb_fiber->reset(ft.cb);             // 对象的reset方法
            }
            else {
                cb_fiber.reset(new Fiber(ft.cb));   // 智能指针reset
            }
            ft.reset();
            cb_fiber->swapIn();
            --m_activeThreadCount;
            if(cb_fiber->getState() == Fiber::READY) {
                schedule(cb_fiber);                 // YieldToReady 再入队
                cb_fiber.reset();
            }
            else if(cb_fiber->getState() == Fiber::EXCEPT || cb_fiber->getState() == Fiber::TERM) {
                cb_fiber->reset(nullptr);           // callback reset
            }
            else {
                cb_fiber->m_state = Fiber::HOLD;
                cb_fiber.reset();
            }
        }
        else {
            if(is_active) {
                --m_activeThreadCount;
            }
            if(idle_fiber->getState() == Fiber::TERM) {
                AZURE_LOG_INFO(g_logger) << "idle fiber term";
                tickle();
                break;  // 空闲协程执行完就结束线程
            }

            ++m_idleThreadCount;
            idle_fiber->swapIn();
            --m_idleThreadCount;

            if(idle_fiber->getState() != Fiber::TERM && idle_fiber->getState() != Fiber::EXCEPT) {
                idle_fiber->m_state = Fiber::HOLD;
            }
        }
    }
}

void Scheduler::tickle() {
    AZURE_LOG_INFO(g_logger) << "tickle";
}

bool Scheduler::stopping() {
    MutexType::Lock lock(m_mutex);
    return m_autostop && m_stopping && m_fibers.empty() && m_activeThreadCount == 0;
}

void Scheduler::idle() {
    AZURE_LOG_INFO(g_logger) << "idle";
    // 一般情况下，调用了stop方法才退出 while 循环
    // idle协程 --> GetMainFiber协程：继续执行while循环，直到 idle_fiber->swapIn()
    // idle_fiber->swapIn()：返回到上一步
    // GetMainFiber协程不包含执行函数，会直接返回到线程逻辑里
    while(!stopping()) {
        azure::Fiber::YieldToHold();
    }
}   

}