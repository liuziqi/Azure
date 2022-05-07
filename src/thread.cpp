#include "thread.h"
#include "log.h"

namespace azure {

// thread_local 是 C++ 11 新引入的一种存储类型，它会影响变量的存储周期
// 有且只有 thread_local 关键字修饰的变量具有线程（thread）周期，
// 这些变量在线程开始的时候被生成，在线程结束的时候被销毁，并且每一个线程都拥有一个独立的变量实例
// thread_local 一般用于需要保证线程安全的函数中
// 需要注意的一点是，如果类的成员函数内定义了 thread_local 变量，
// 则对于同一个线程内的该类的多个对象都会共享一个变量实例，
// 并且只会在第一次执行这个成员函数时初始化这个变量实例，这一点是跟类的静态成员变量类似的
static thread_local Thread *t_thread = nullptr;
static thread_local std::string t_thread_name = "UNKNOWN";

static azure::Logger::ptr g_logger = AZURE_LOG_NAME("system");

Semaphore::Semaphore(uint32_t count) {
    if(sem_init(&m_semaphore, 0, count)) {
        throw std::logic_error("sem_init error");
    }
}

Semaphore::~Semaphore() {
    sem_destroy(&m_semaphore);
}

void Semaphore::wait() {
    if(sem_wait(&m_semaphore)) {   // sem_wait 成功返回 0，
        throw std::logic_error("sem_wait error");
    }
}

void Semaphore::notify() {
    if(sem_post(&m_semaphore)) {
        throw std::logic_error("sem_post error");
    }
}

Thread *Thread::GetThis() {
    return t_thread;
}

const std::string Thread::GetName() {
    return t_thread_name;
}

void Thread::SetName(const std::string& name) {
    if(t_thread) {
        t_thread->m_name = name;
    }
    t_thread_name = name;
}

Thread::Thread(std::function<void()> cb, const std::string &name) 
    : m_cb(cb)
    , m_name(name) {
    if(name.empty()) {
        m_name == "UNKNOWN";
    }
    // 第二个参数用来设置线程属性
    // 第三个参数指定线程需要执行的函数，该函数的参数最多有1个（可以省略不写），形参和返回值的类型都必须为 void* 类型
    // 第四个参数指定传递给第三个参数函数的实参，nullptr表示不需要传递任何数据
    int rt = pthread_create(&m_thread, nullptr, &Thread::run, this);
    if(rt) {
        AZURE_LOG_ERROR(g_logger) << "pthread_create thread fail, rt=" << rt << ", name=" << name;
        throw std::logic_error("pthread_create error");
    }

    // 可能在构造函数返回的时候线程还没有开始执行
    // m_semaphore.wait();
}

Thread::~Thread() {
    if(m_thread) {
        // 从状态上实现线程分离
        // 线程主动与主控线程断开关系。线程结束后（不会产生僵尸线程），其退出状态不由其他线程获取，
        // 直接自己自动释放（自己清理掉PCB的残留资源）。网络、多线程服务器常用
        pthread_detach(m_thread);
    }
}

void Thread::join() {
    if(m_thread) {
        // pthread_join() 函数会一直阻塞调用它的线程，直至目标线程执行结束（接收到目标线程的返回值）
        int rt = pthread_join(m_thread, nullptr);
        if(rt) {
            AZURE_LOG_ERROR(g_logger) << "pthread_join thread fail, rt=" << rt << ", name=" << m_name;
            throw std::logic_error("pthread_join error");
        }
        m_thread = 0;
    }
}

void *Thread::run(void *arg) {
    Thread *thread = (Thread *) arg;
    t_thread = thread;
    t_thread_name = thread->getName();
    thread->m_id = azure::GetThreadId();
    // 给线程设置name
    pthread_setname_np(pthread_self(), thread->m_name.substr(0, 15).c_str());

    // 防止Thread对象被释放掉
    std::function<void()> cb;
    cb.swap(thread->m_cb);

    // 保证构造函数退出的时候线程已经运行起来了
    // t_thread->m_semaphore.notify();

    cb();   // 线程真正执行的函数
    return 0;
}

}