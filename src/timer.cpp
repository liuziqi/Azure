#include "timer.h"
#include "util.h"

namespace azure {

bool Timer::Comparator::operator()(const Timer::ptr &lhs, const Timer::ptr &rhs) const {
    if(!lhs && !rhs) {
        return false;
    }
    if(!lhs) {
        return true;
    }
    if(!rhs) {
        return false;
    }
    if(lhs->m_next == rhs->m_next) {
        return lhs.get() < rhs.get();
    }
    return lhs->m_next < rhs->m_next;
}

Timer::Timer(uint64_t ms, std::function<void()> cb, bool recurring, TimerManager *manager) 
    : m_recurring(recurring)
    , m_ms(ms)
    , m_cb(cb)
    , m_manager(manager) {
    m_next = azure::GetCurrentMS() + m_ms;
}

// 只用来进行比较
Timer::Timer(uint64_t next)
    : m_next(next) {
}

// 取消定时器
bool Timer::cancel() {
    TimerManager::RWMutexType::WriteLock lock(m_manager->m_mutex);
    if(m_cb) {
        m_cb = nullptr;
        auto it = m_manager->m_timers.find(shared_from_this());
        m_manager->m_timers.erase(it);
        return true;
    }
    return false;
}

// 刷新定时器 m_next = azure::GetCurrentMS() + m_ms;
bool Timer::refresh() {
    TimerManager::RWMutexType::WriteLock lock(m_manager->m_mutex);
    if(!m_cb) {
        return false;
    }
    auto it = m_manager->m_timers.find(shared_from_this());
    if(it == m_manager->m_timers.end()) {
        return false;
    }
    m_manager->m_timers.erase(it);
    m_next = azure::GetCurrentMS() + m_ms;
    m_manager->m_timers.insert(shared_from_this());
    return true;
}

// 修改超时时间
bool Timer::reset(uint64_t ms, bool from_now) {
    if(ms == m_ms && !from_now) {
        return true;
    }
    TimerManager::RWMutexType::WriteLock lock(m_manager->m_mutex);
    if(!m_cb) {
        return false;
    }
    auto it = m_manager->m_timers.find(shared_from_this());
    if(it == m_manager->m_timers.end()) {
        return false;
    }
    m_manager->m_timers.erase(it);
    uint64_t start = 0;
    if(from_now) {
        start = azure::GetCurrentMS();
    }
    else {
        start = m_next - m_ms;
    }
    m_ms = ms;
    m_next = start + m_ms;
    m_manager->addTimer(shared_from_this(), lock);
    return true;
}

TimerManager::TimerManager() {
    m_previousTime = azure::GetCurrentMS();
}

TimerManager::~TimerManager() {

}

Timer::ptr TimerManager::addTimer(uint64_t ms, std::function<void()> cb, bool recurring) {
    Timer::ptr timer(new Timer(ms, cb, recurring, this));
    RWMutexType::WriteLock lock(m_mutex);
    addTimer(timer, lock);
    return timer;
}

static void OnTimer(std::weak_ptr<void> weak_cond, std::function<void()> cb) {
    std::shared_ptr<void> tmp = weak_cond.lock();   // 获取智能指针
    if(tmp) {
        cb();
    }
}

Timer::ptr TimerManager::addConditionTimer(uint64_t ms, std::function<void()> cb, std::weak_ptr<void> weak_cond, bool recurring) {
    return addTimer(ms, std::bind(&OnTimer, weak_cond, cb), recurring);
}

// 获取距离下一次触发定时器所需要的时间
uint64_t TimerManager::getNextTimer() {
    RWMutexType::ReadLock lock(m_mutex);
    m_tickled = false;
    if(m_timers.empty()) {
        return ~0ull;
    }

    const Timer::ptr &next = *m_timers.begin();
    uint64_t now_ms = azure::GetCurrentMS();
    if(now_ms >= next->m_next) {
        return 0;
    }
    else {
        return next->m_next - now_ms;
    }
}

// 获取所有已超时的定时器的回调函数
void TimerManager::listExpiredCb(std::vector<std::function<void()>> &cbs) {
    uint64_t now_ms = azure::GetCurrentMS();
    std::vector<Timer::ptr> expired;
    {
        RWMutexType::ReadLock lock(m_mutex);
        if(m_timers.empty()) {
            return;
        }
    }

    RWMutexType::WriteLock lock(m_mutex);
    if(m_timers.empty()) {
        return;
    }
    bool rollover = detectClockRollover(now_ms);
    if(!rollover && ((*m_timers.begin())->m_next > now_ms)) {
        return;
    }

    Timer::ptr now_timer(new Timer(now_ms));
    auto it = rollover ? m_timers.end() : m_timers.upper_bound(now_timer);
    expired.insert(expired.begin(), m_timers.begin(), it);
    m_timers.erase(m_timers.begin(), it);
    cbs.reserve(expired.size());

    for(auto &timer : expired) {
        cbs.push_back(timer->m_cb);
        if(timer->m_recurring) {
            timer->m_next = now_ms + timer->m_ms;
            m_timers.insert(timer);
        }
        else {
            timer->m_cb = nullptr;  // 主要是释放智能指针
        }
    }
}

void TimerManager::addTimer(Timer::ptr timer, RWMutexType::WriteLock &lock) {
    auto it = m_timers.insert(timer).first;
    bool at_front = (it == m_timers.begin() && !m_tickled);
    if(at_front) {
        m_tickled = true;
    }
    lock.unlock();

    if(at_front) {
        // 重新设置之前 epoll_wait 设置的超时时间
        onTimerInsertedAtFront();
    }
}

// FIXME 逻辑有问题
// 处理服务器调整过时间的情况
bool TimerManager::detectClockRollover(uint64_t now_ms) {
    bool rollover = false;
    // 当前时间小于m_previousTime，且小于1小时
    if(now_ms < m_previousTime && now_ms < (m_previousTime - 60 * 60 * 1000)) {
        rollover = true;
    }
    m_previousTime = now_ms;
    return rollover;
}

bool TimerManager::hasTimer() {
    RWMutexType::ReadLock lock(m_mutex);
    return !m_timers.empty();
}

}