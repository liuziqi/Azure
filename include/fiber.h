#ifndef __AZURE_FIBER_H__
#define __AZURE_FIBER_H__

#include <memory>
#include <functional>
#include <ucontext.h>
#include "thread.h"

namespace azure {

// LEARN enable_shared_from_this
class Fiber : public std::enable_shared_from_this<Fiber> {
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
    Fiber(std::function<void()> cb, size_t stacksize=0);
    ~Fiber();

    void reset(std::function<void()> cb);   // 重置协程函数和状态，协程当前函数执行完了，可以重复利用它的内存去执行其他的函数
    void swapIn();                  // 切换到当前协程执行
    void swapOut();                 // 切换到后台执行

    uint64_t GetId() const {return m_id;}

public:
    static void SetThis(Fiber *f);  // 设置当前协程
    static Fiber::ptr GetThis();    // 获取当前协程
    static void YieldToReady();     // 协程切换到后台，并设置为Ready状态
    static void YieldToHold();      // 协程切换到后台，并设置为Hold状态
    static uint64_t TotalFibers();  // 总协程数

    static void MainFunc();         // 用来执行m_cb
    static uint64_t GetFiberId();

private:
    uint64_t m_id = 0;
    uint32_t m_stacksize = 0;
    State m_state = INIT;

    ucontext_t m_ctx;
    void *m_stack = nullptr;

    std::function<void()> m_cb;

private:
    Fiber();
};

}

#endif