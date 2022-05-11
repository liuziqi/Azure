#include <atomic>
#include "config.h"
#include "macro.h"
#include "fiber.h"
#include "scheduler.h"

namespace azure {

static Logger::ptr g_logger = AZURE_LOG_NAME("system");

static std::atomic<uint64_t> s_fiber_id{0};
static std::atomic<uint64_t> s_fiber_count{0};

static thread_local Fiber *t_fiber = nullptr;               // 指向当前协程
static thread_local Fiber::ptr t_threadFiber = nullptr;     // 主协程的智能指针

// 协程栈大小
static ConfigVar<uint32_t>::ptr g_fiber_stack_size = Config::Lookup<uint32_t>("fiber.stack_size", 1024 * 1024, "fiber stack size");

class MallocStackAllocator {
public:
    static void *Alloc(size_t size) {
        return malloc(size);
    }

    static void Dealloc(void *vp, size_t size) {
        return free(vp);
    }
};

using StackAllocator = MallocStackAllocator;

uint64_t Fiber::GetFiberId() {
    return t_fiber ? t_fiber->GetId() : 0;
}

// 只有主协程用这个私有构造函数
Fiber::Fiber() {
    m_state = EXEC;
    SetThis(this);

    // 初始化ucp结构体，将当前的上下文保存到m_ctx中
    // 主协程没有MainFunc，不干任何事，就会返回主函数
    if(getcontext(&m_ctx)) {    
        AZURE_ASSERT2(false, "getcontext");
    }
    ++s_fiber_count;

    AZURE_LOG_DEBUG(g_logger) << "Fiber::Fiber id=" << m_id;
}

Fiber::Fiber(std::function<void()> cb, size_t stacksize, bool use_caller)
    : m_id(++s_fiber_id)
    , m_cb(cb) {
    ++s_fiber_count;
    m_stacksize = stacksize ? stacksize : g_fiber_stack_size->getValue();

    m_stack = StackAllocator::Alloc(m_stacksize);
    if(getcontext(&m_ctx)) {
        AZURE_ASSERT2(false, "getcontext");
    }
    m_ctx.uc_link = nullptr;                // 后继上下文
    m_ctx.uc_stack.ss_sp = m_stack;         // 栈空间基址
    m_ctx.uc_stack.ss_size = m_stacksize;   // 栈大小

    if(!use_caller) {
        // 修改通过getcontext取得的上下文m_ctx
        makecontext(&m_ctx, &Fiber::MainFunc, 0);
    }
    else {
        makecontext(&m_ctx, &Fiber::CallerMainFunc, 0);
    }
    
    // m_state = INIT;
    AZURE_LOG_DEBUG(g_logger) << "Fiber::Fiber id=" << m_id;
}

Fiber::~Fiber() {
    --s_fiber_count;
    if(m_stack) {
        AZURE_ASSERT(m_state == TERM || m_state == INIT || m_state == EXCEPT);
        StackAllocator::Dealloc(m_stack, m_stacksize);
    }
    // 主协程
    else {
        AZURE_ASSERT(!m_cb);
        AZURE_ASSERT(m_state == EXEC);

        Fiber *cur = t_fiber;
        if(cur == this) {
            SetThis(nullptr);
        }
    }

    AZURE_LOG_DEBUG(g_logger) << "Fiber::~Fiber id=" << m_id;
}

void Fiber::reset(std::function<void()> cb) {
    AZURE_ASSERT(m_stack);
    AZURE_ASSERT(m_state == TERM || m_state == INIT);
    m_cb = cb;
    if(getcontext(&m_ctx)) {
        AZURE_ASSERT(m_state == TERM || m_state == INIT);
    }
    m_ctx.uc_link = nullptr;                // Pointer to the context that is resumed
    m_ctx.uc_stack.ss_sp = m_stack;
    m_ctx.uc_stack.ss_size = m_stacksize;

    makecontext(&m_ctx, &Fiber::MainFunc, 0);
    m_state = INIT;
}

void Fiber::swapIn() {
    SetThis(this);
    AZURE_ASSERT(m_state != EXEC);
    m_state = EXEC;

    // 第一个参数是被换出去的协程，第二个是换进来的协程
    if(swapcontext(&(Scheduler::GetMainFiber()->m_ctx), &m_ctx)) {
        AZURE_ASSERT2(false, "swapcontext");
    }
}

// 把主协程切换进来
void Fiber::swapOut() {
    // AZURE_LOG_INFO(g_logger) << "fiber swapout, out: " << m_id << ", in:" << Scheduler::GetMainFiber()->m_id;
    SetThis(Scheduler::GetMainFiber());
    if(swapcontext(&m_ctx, &(Scheduler::GetMainFiber()->m_ctx))) {
        AZURE_ASSERT2(false, "swapcontext");
    }
}

void Fiber::SetThis(Fiber *f) {
    t_fiber = f;
}

Fiber::ptr Fiber::GetThis() {
    if(t_fiber) {
        return t_fiber->shared_from_this();
    }
    // 静态成员函数调用私有默认构造
    Fiber::ptr main_fiber(new Fiber);
    AZURE_ASSERT(t_fiber == main_fiber.get());
    t_threadFiber = main_fiber;
    return t_fiber->shared_from_this();
}

void Fiber::YieldToReady() {
    Fiber::ptr cur = GetThis();
    if(cur != t_threadFiber) {
        cur->m_state = READY;
        cur->swapOut();
    }
}

void Fiber::YieldToHold() {
    Fiber::ptr cur = GetThis();
    if(cur != t_threadFiber) {
        cur->m_state = HOLD;
        cur->swapOut();
    }
}

uint64_t Fiber::TotalFibers() {
    return s_fiber_count;
}

void Fiber::call() {
    SetThis(this);
    m_state = EXEC;
    if(swapcontext(&(t_threadFiber->m_ctx), &m_ctx)) {
        AZURE_ASSERT2(false, "swapcontext");
    }
}

void Fiber::back() {
    SetThis(t_threadFiber.get());
    if(swapcontext(&m_ctx, &(t_threadFiber->m_ctx))) {
        AZURE_ASSERT2(false, "swapcontext");
    }
}

void Fiber::MainFunc() {
    Fiber::ptr cur = GetThis();
    AZURE_ASSERT(cur);
    try {
        cur->m_cb();
        cur->m_cb = nullptr;
        cur->m_state = TERM;
    }
    catch(std::exception &e) {
        cur->m_state = EXCEPT;
        AZURE_LOG_ERROR(g_logger) << "Fiber Except: " << e.what() << " fiber_id=" << cur->m_id << std::endl << BacktraceToString();
    }
    catch(...) {
        cur->m_state = EXCEPT;
        AZURE_LOG_ERROR(g_logger) << "Fiber Except: " << " fiber_id=" << cur->m_id << std::endl << BacktraceToString();
    }

    // 让引用计数可以减到0
    auto raw_ptr = cur.get();
    cur.reset();

    // 协程的切换需要手动管理，执行完切回主协程
    raw_ptr->swapOut();

    AZURE_ASSERT2(false, "never reach fiber_id = " + std::to_string(raw_ptr->m_id));
}

void Fiber::CallerMainFunc() {
    Fiber::ptr cur = GetThis();
    AZURE_ASSERT(cur);
    try {
        cur->m_cb();
        cur->m_cb = nullptr;
        cur->m_state = TERM;
    }
    catch(std::exception &e) {
        cur->m_state = EXCEPT;
        AZURE_LOG_ERROR(g_logger) << "Fiber Except: " << e.what() << " fiber_id=" << cur->m_id << std::endl << BacktraceToString();
    }
    catch(...) {
        cur->m_state = EXCEPT;
        AZURE_LOG_ERROR(g_logger) << "Fiber Except: " << " fiber_id=" << cur->m_id << std::endl << BacktraceToString();
    }

    // 让引用计数可以减到0
    auto raw_ptr = cur.get();
    cur.reset();

    // 协程的切换需要手动管理，执行完切回主协程
    raw_ptr->back();

    AZURE_ASSERT2(false, "never reach fiber_id = " + std::to_string(raw_ptr->m_id));
}

}