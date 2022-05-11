#ifndef __AZURE_THREAD_H__
#define __AZURE_THREAD_H__

#include <thread>
#include <functional>
#include <memory>
#include "mutex.h"

namespace azure {

class Thread {
public:
    typedef std::shared_ptr<Thread> ptr;
    
    Thread(std::function<void()> cb, const std::string &name);
    ~Thread();

    pid_t getId() const {return m_id;}
    const std::string &getName() const {return m_name;}

    void join();

    static Thread *GetThis();                       // 拿到当前线程
    static const std::string GetName();             // 拿到当前线程的名称，为了方便使用
    static void SetName(const std::string &name);   // 这样也可以对主线程命名

private:
    Thread(const Thread&) = delete;             // 禁止拷贝构造
    Thread(const Thread&&) = delete;            // 禁止移动构造
    Thread &operator=(const Thread&) = delete;  // 禁止复制构造

    static void *run(void *arg);

private:
    pid_t m_id = -1;
    pthread_t m_thread = 0;
    std::function<void()> m_cb;     // 包装了一个返回类型为void，无参数的函数对象，可直接通过m_cb()进行调用
    std::string m_name;
    Semaphore m_semaphore;
};

}

#endif
