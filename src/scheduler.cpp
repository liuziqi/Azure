#include "scheduler.h"
#include "log.h"
#include "macro.h"

namespace azure {

static azure::Logger::ptr g_logger = AZURE_LOG_NAME("system");

static thread_local Scheduler *t_scheduler = nullptr;       // 当前使用中的协程调度器（一般只存在一个协程调度器）
static thread_local Fiber *t_scheduler_fiber = nullptr;     // 调度协程，每个线程的主协程会被设置为调度线程，用于协程切换

Scheduler::Scheduler(size_t thread_num, bool use_caller, const std::string &name) 
    : m_name(name) {
    AZURE_ASSERT(thread_num > 0);

    if(use_caller) {
        azure::Fiber::GetThis();                // 线程没有协程的话就会初始化一个主协程（主协程只用来切换协程）
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

    // bool exit_on_this_fiber = false;
    if(m_rootThreadId != -1) {  // 说明是use_caller（调度线程）
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
        // while(!stopping()) {
        //     if(m_rootFiber->getState() == Fiber::TERM || m_rootFiber->getState() == Fiber::EXCEPT) {
        //         m_rootFiber.reset(new Fiber(std::bind(&Scheduler::run, this), 0, true));
        //         AZURE_LOG_INFO(g_logger) << " root fiber is term reset";
        //         t_scheduler_fiber = m_rootFiber.get();
        //     }
        //     m_rootFiber->call();
        // }
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
    t_scheduler = this;
}

// 由线程执行的函数
void Scheduler::run() {
    AZURE_LOG_INFO(g_logger) << m_name << " run";

    setThis();  // 设置当前Scheduler
    if(azure::GetThreadId() != m_rootThreadId) {    // 如果不是主线程，初始化主协程，赋值给当前调度协程
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
            ft.reset();     // 智能指针reset
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
                // cb_fiber->reset(nullptr);           // callback reset
                cb_fiber.reset();
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
                break;
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
    while(!stopping()) {
        azure::Fiber::YieldToHold();
    }
}   

}