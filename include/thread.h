#ifndef __AZURE_THREAD_H__
#define __AZURE_THREAD_H__

#include <thread>
#include <pthread.h>
#include <functional>
#include <memory>
#include <semaphore.h>

namespace azure {

class Semaphore {
public:
    Semaphore(uint32_t count = 0);
    ~Semaphore();

    void wait();
    void notify();

private:
    Semaphore(const Semaphore&) = delete;
    Semaphore(const Semaphore&&) = delete;
    Semaphore &operator=(const Semaphore&) = delete;

private:
    sem_t m_semaphore;
};

// 构造加锁，析构解锁
template<typename T>
struct ScopedLockImpl {
public:
    ScopedLockImpl(T &mutex)
    : m_mutex(mutex) {
        m_mutex.lock();
        m_locked = true;
    }

    ~ScopedLockImpl() {
        unlock();
    }

    void lock() {
        if(!m_locked) {
            m_mutex.lock();
            m_locked = true;
        }
    }

    void unlock() {
        if(m_locked) {
            m_mutex.unlock();
            m_locked = false;
        }
    }

private:
    T &m_mutex;
    bool m_locked;
};

class Mutex {
public:
    typedef ScopedLockImpl<Mutex> Lock;

    Mutex() {
        pthread_mutex_init(&m_mutex, nullptr);
    }

    ~Mutex() {
        pthread_mutex_destroy(&m_mutex);
    }

    void lock() {
        pthread_mutex_lock(&m_mutex);
    }

    void unlock() {
        pthread_mutex_unlock(&m_mutex);
    }

private:
    pthread_mutex_t m_mutex;
};

// 测试用的，测试把锁换成空锁会出现什么情况
class NullMutex {
public:
    typedef ScopedLockImpl<NullMutex> Lock;
    NullMutex() {}
    ~NullMutex() {}
    void lock() {}
    void unlock() {}
};

template<typename T>
struct ReadScopedLockImpl {
public:
    ReadScopedLockImpl(T &mutex)
    : m_mutex(mutex) {
        m_mutex.rdlock();
        m_locked = true;
    }

    ~ReadScopedLockImpl() {
        unlock();
    }

    void lock() {
        if(!m_locked) {
            m_mutex.rdlock();
            m_locked = true;
        }
    }

    void unlock() {
        if(m_locked) {
            m_mutex.unlock();
            m_locked = false;
        }
    }

private:
    T &m_mutex;
    bool m_locked;
};

template<typename T>
struct WriteScopedLockImpl {
public:
    WriteScopedLockImpl(T &mutex)
    : m_mutex(mutex) {
        m_mutex.wrlock();
        m_locked = true;
    }

    ~WriteScopedLockImpl() {
        unlock();
    }

    void lock() {
        if(!m_locked) {
            m_mutex.wrlock();
            m_locked = true;
        }
    }

    void unlock() {
        if(m_locked) {
            m_mutex.unlock();
            m_locked = false;
        }
    }

private:
    T &m_mutex;
    bool m_locked;
};

// LEARN 各种锁的特点和应用场景
// 读写锁
class RWMutex {
public:
    typedef ReadScopedLockImpl<RWMutex> ReadLock;
    typedef WriteScopedLockImpl<RWMutex> WriteLock;

    RWMutex() {
        pthread_rwlock_init(&m_lock, nullptr);
    }

    ~RWMutex() {
        pthread_rwlock_destroy(&m_lock);
    }

    void rdlock() {
        pthread_rwlock_rdlock(&m_lock);
    }

    void wrlock() {
        pthread_rwlock_wrlock(&m_lock);
    }

    void unlock() {
        pthread_rwlock_unlock(&m_lock);
    }

private:
    pthread_rwlock_t m_lock;
};

// 测试用的，测试把锁换成空锁会出现什么情况
class NullRWMutex {
public:
    typedef ReadScopedLockImpl<RWMutex> ReadLock;
    typedef WriteScopedLockImpl<RWMutex> WriteLock;
    NullRWMutex() {}
    ~NullRWMutex() {}
    void rdlock() {}
    void wrlock() {}
    void unlock() {}
};

class Thread {
public:
    typedef std::shared_ptr<Thread> ptr;
    Thread(std::function<void()> cb, const std::string &name);
    ~Thread();

    pid_t getId() const {return m_id;}
    const std::string &getName() const {return m_name;}

    void join();

    static Thread *GetThis();               // 拿到当前线程
    static const std::string GetName();     // 拿到当前线程的名称，为了方便使用
    static void SetName(const std::string &name);   // 这样也可以对主线程命名

private:
    Thread(const Thread&) = delete;     // 禁止拷贝构造
    Thread(const Thread&&) = delete;    // 禁止移动构造
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
