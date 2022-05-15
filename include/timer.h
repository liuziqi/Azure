#ifndef __AZURE_TIMER_H__
#define __AZURE_TIMER_H__

#include <memory>
#include <functional>
#include <set>
#include <vector>
#include "mutex.h"

namespace azure {

class TimerManager;

class Timer : public std::enable_shared_from_this<Timer> {
friend class TimerManager;

public:
    typedef std::shared_ptr<Timer> ptr;

    bool cancel();
    bool refresh();
    bool reset(uint64_t ms, bool from_now);

private:
    Timer(uint64_t ms, std::function<void()> cb, bool recurring, TimerManager *manager);
    Timer(uint64_t next);

private:
    bool m_recurring = false;               // 是否定义循环定时器
    uint64_t m_ms = 0;                      // 执行周期
    uint64_t m_next = 0;                    // 精确的执行时间
    std::function<void()> m_cb;             // 超时回调
    TimerManager *m_manager = nullptr;      // 时间管理大师

private:
    struct Comparator {
        bool operator()(const Timer::ptr &lhs, const Timer::ptr &rhs) const;
    };
};

class TimerManager {
friend class Timer;

public:
    typedef RWMutex RWMutexType;

    TimerManager();
    virtual ~TimerManager();

    Timer::ptr addTimer(uint64_t ms, std::function<void()> cb, bool recurring=false);
    // 条件定时器，weak_ptr当作执行条件，当智能指针计数为0时说明条件不成立了，不触发定时事件
    Timer::ptr addConditionTimer(uint64_t ms, std::function<void()> cb, std::weak_ptr<void> weak_cond, bool recurring=false);

    uint64_t getNextTimer();    // 获取下一个定时器的执行时间

    void listExpiredCb(std::vector<std::function<void()>> &cbs);

    bool hasTimer();

protected:
    virtual void onTimerInsertedAtFront() = 0;  // 重新设置之前 epoll_wait 设置的超时时间
    void addTimer(Timer::ptr timer, RWMutexType::WriteLock &lock);

private:
    bool detectClockRollover(uint64_t now_ms);  // 处理服务器时间被修改的情况

private:
    RWMutexType m_mutex;
    std::set<Timer::ptr, Timer::Comparator> m_timers;
    bool m_tickled = false;
    uint64_t m_previousTime = 0;
};

}


#endif