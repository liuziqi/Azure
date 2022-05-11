#ifndef __AZURE_FIBER_H__
#define __AZURE_FIBER_H__

#include <memory>
#include <functional>
#include <ucontext.h>   // 提供上下文切换的函数
#include "thread.h"

namespace azure {

// LEARN enable_shared_from_this
class Fiber : public std::enable_shared_from_this<Fiber> {
friend class Scheduler;

public:
    typedef std::shared_ptr<Fiber> ptr;

    enum State {
        INIT,
        HOLD,
        EXEC,
        TERM,
        READY,
        EXCEPT
    };

public:
    Fiber(std::function<void()> cb, size_t stacksize=0, bool use_caller=false);
    ~Fiber();

    void reset(std::function<void()> cb);   // 重置协程函数和状态，协程当前函数执行完了，可以重复利用它的内存去执行其他的函数
    void swapIn();                          // 切换到当前协程执行
    void swapOut();                         // 切换到后台执行

    uint64_t GetId() const {return m_id;}
    State getState() const {return m_state;}

public:
    static void SetThis(Fiber *f);          // 设置当前协程
    static Fiber::ptr GetThis();            // 获取当前协程
    static void YieldToReady();             // 协程切换到后台，并设置为Ready状态
    static void YieldToHold();              // 协程切换到后台，并设置为Hold状态
    static uint64_t TotalFibers();          // 总协程数
    static uint64_t GetFiberId();

    void call();                            // 把当前协程置换成目标协程
    void back();                            // 

    static void MainFunc();                 // 用来执行m_cb
    static void CallerMainFunc();           // 用来执行m_cb

private:
    uint64_t m_id = 0;                      // 协程id
    uint32_t m_stacksize = 0;               // 栈大小
    State m_state = INIT;                   // 协程状态
    ucontext_t m_ctx;                       // 协程上下文
    void *m_stack = nullptr;                // 栈基址
    std::function<void()> m_cb;             // 协程要执行的函数

private:
    Fiber();
};

}

#endif