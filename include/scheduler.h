#ifndef __AZURE_SCHEDULER_H__
#define __AZURE_SCHEDULER_H__

#include <memory>
#include <list>
#include <vector>
#include "fiber.h"
#include "thread.h"
#include "mutex.h"

namespace azure {

// 协程调度器
// N线程 ： M协程
class Scheduler {
public:
    typedef std::shared_ptr<Scheduler> ptr;
    typedef Mutex MutexType;

    // thread_num 管理的线程数量
    // use_caller 是否使用当前调用线程（即主线程用来进行调度）
    // name 协程调度器名称
    Scheduler(size_t thread_num=1, bool use_caller=true, const std::string &name="");
    virtual ~Scheduler();

    const std::string &getName() const {return m_name;}

    static Scheduler *GetThis();    // 返回当前协程调度器
    static Fiber *GetMainFiber();   // 返回当前协程调度器的调度协程

    void start();   // 启动协程调度器
    void stop();    // 停止协程调度器

    // 调度一个协程
    // fc: fiber or callback
    // -1: 标识任意线程
    template<typename FiberOrCb>    
    void schedule(FiberOrCb fc, int thread=-1) {
        bool need_tickle = false;
        {
            MutexType::Lock lock(m_mutex);
            need_tickle = scheduleNoLock(fc, thread);
        }
        if(need_tickle) {
            tickle();
        }
    }

    // 批量调度协程
    template<typename InputIterator>
    void schedule(InputIterator begin, InputIterator end) {
        bool need_tickle = false;
        {
            MutexType::Lock lock(m_mutex);
            while(begin != end) {
                need_tickle = scheduleNoLock(&(*begin), -1) || need_tickle;
                ++begin;
            }
        }
        if(need_tickle) {
            tickle();
        }
    }

protected:
    virtual void tickle();      // 通知协程调度器有任务了
    void run();                 // 协程调度函数
    virtual bool stopping();    // 返回是否可以停止
    virtual void idle();        // 空闲协程，没有任务做时运行该协程
    void setThis();             // 设置当前的协程调度器

private:
    // 向协程队列添加一个协程
    template<class FiberOrCb>
    bool scheduleNoLock(FiberOrCb fc, int thread) {
        // 从无到有才进行通知
        bool need_tickle = m_fibers.empty();
        FiberAndThread ft(fc, thread);
        // 确保fc是fiber或者callback
        if(ft.fiber || ft.cb) {
            m_fibers.push_back(ft);
        }
        return need_tickle;
    }

private:
    struct FiberAndThread {
        Fiber::ptr fiber;
        std::function<void()> cb;
        int thread;

        FiberAndThread(Fiber::ptr f, int thr)
            : fiber(f), thread(thr) {
        }

        FiberAndThread(Fiber::ptr *f, int thr)
            : thread(thr) {
            fiber.swap(*f);  // 涉及到引用释放的问题
        }

        FiberAndThread(std::function<void()> f, int thr)
            : cb(f), thread(thr) {
        }

        FiberAndThread(std::function<void()> *f, int thr)
        : thread(thr) {
            cb.swap(*f);
        }

        FiberAndThread()
            : thread(-1) {
        }

        void reset() {
            fiber = nullptr;
            cb = nullptr;
            thread = -1;
        }
    };

private:
    MutexType m_mutex;
    std::vector<Thread::ptr> m_threads;             // 线程池
    std::list<FiberAndThread> m_fibers;             // 即将要执行的协程队列
    Fiber::ptr m_rootFiber;                         // use_caller为true时有效，调度协程
    std::string m_name;                             // 协程调度器名称

protected:
    std::vector<int> m_threadIds;                   // 协程下的线程id数组
    size_t m_threadCount = 0;                       // 线程数量
    std::atomic<size_t> m_activeThreadCount = {0};  // 工作线程数量
    std::atomic<size_t> m_idleThreadCount = {0};    // 空闲线程数量
    bool m_stopping = true;                         // 是否正在停止
    bool m_autostop = false;                        // 是否自动停止
    int m_rootThreadId = 0;                         // 主线程id（user_caller）
};

}

#endif