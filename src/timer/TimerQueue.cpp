//
// Created by lq on 2024/6/24.
//

#include "TimerQueue.h"
#include "EventLoop.h"
#include "Channel.h"
#include "Logging.h"
#include "Timer.h"
#include "TimerQueue.h"

#include <sys/timerfd.h>
#include <unistd.h>
#include <string.h>

int createTimefd(){
    //创建timerfd
    // CLOCK_MONOTONIC表示绝对时间（最近一次重启到现在的时间）
    // CLOCK_REALTIME表示从1970.1.1到目前的时间
    //TFD_NONBLOCK表示文件描述符以非阻塞的模式工作，在没有数据可读取时，读操作会立即返回
    //TFD_CLOEXEC表示在执行exec系列函数时，自动关闭文件描述符，通过exec执行一个新程序时，新程序不会继承这个文件描述符
    int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    LOG_DEBUG << "create a timerfd, fd = " << timerfd;
    if(timerfd < 0) LOG_ERROR << "Failed in timerfd_create";
    return timerfd;
}

//重置timerfd的超时时刻，重置后，如果超时时刻不为0，则内核会启动定时器，否则内核会停止定时器
void resetTimerfd(int timerfd_, Timestamp expiration){
    /**
     * struct itimerspec
     * {
     *   struct timespec it_interval;//此值为0表示定时器是一次性的
     *   struct timespec it_value;//
     * };
     */
    struct itimerspec newValue;
    struct itimerspec oldValue;
    memset(&newValue, '\0', sizeof(newValue));
    memset(&oldValue, '\0', sizeof(oldValue));
    //计算多久后计时器超时
    int64_t microSecondDif = expiration.microSecondsSinceEpoch() - Timestamp::now().microSecondsSinceEpoch();
    if(microSecondDif < 100){
        microSecondDif = 100;
    }

    struct timespec ts;
    ts.tv_sec = static_cast<time_t>(microSecondDif / Timestamp::kMicrosecondsPerSecond);
    ts.tv_nsec = static_cast<long>((microSecondDif % Timestamp::kMicrosecondsPerSecond) * 1000);
    newValue.it_value = ts;
    //调用timerfd_settime会在内核启动定时器
    //&oldValue用于存储之前的定时器，&newValue用于指定新的定时器，
    if(::timerfd_settime(timerfd_, 0, &newValue, &oldValue)){
        LOG_ERROR << "timerfd_settime failed";
    }
}

void readTimerfd(int timerfd){
    uint64_t howmany;
    //sszie_t的范围为从-1到SSIZE_MAX，SSIZE_MAX是与size_t的最大值相对应的最大正值
    ssize_t n = ::read(timerfd, &howmany, sizeof(howmany));
    if(n != sizeof(howmany)){
        LOG_ERROR << "TimerQueue::handleRead() reads " << n << " bytes insted of 8";
    }
}

TimerQueue::TimerQueue(EventLoop *loop) :
loop_(loop),
timerfd_(createTimefd()),
timerfdChannel_(loop_, timerfd_),
timers_(){
    timerfdChannel_.setReadCallback(std::bind(&TimerQueue::handleRead, this));//为timerfd的可读事件设置回调函数
    timerfdChannel_.enableReading();//向epoll中注册timerfd的可读事件
}

TimerQueue::~TimerQueue() {
    timerfdChannel_.disableAll();
    timerfdChannel_.remove();
    ::close(timerfd_);
    for(const Entry& timer : timers_){
        delete timer.second;
    }
}

void TimerQueue::addTimer(TimerQueue::TimerCallback cb, Timestamp when, double interval) {
    Timer *timer = new Timer(std::move(cb), when, interval);
    loop_->runInLoop(std::bind(&TimerQueue::addTimerInLoop, this, timer));
}

void TimerQueue::addTimerInLoop(Timer *timer) {
    //将timer添加到TimerList时，判断其超时时刻是否是最早的
    bool eraliestChanged = insert(timer);
    if(eraliestChanged){
        resetTimerfd(timerfd_, timer->expiration());
    }
}

//返回删除的定时器节点
std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now) {
    std::vector<Entry> expired;
    //reinterpret_cast是C++中的一种类型转换操作符，他可以将一个指针或引用转换为另一种不同类型的指针或引用
    //UINTPTR_MAX表示无符号整数类型的最大值
    Entry sentry(now, reinterpret_cast<Timer*>(UINTPTR_MAX));
    //lower_bound返回第一个大于等于sentry的迭代器
    //end是第一个没有超时的计时器
    TimerList ::iterator end = timers_.lower_bound(sentry);
    //back_inserter表示每个元素都插入到expired的尾部
    std::copy(timers_.begin(), end, back_inserter(expired));
    //把超时的元素从TimerList中删掉
    timers_.erase(timers_.begin(), end);
    return expired;
}

void TimerQueue::handleRead() {
    Timestamp now = Timestamp::now();
    readTimerfd(timerfd_);

    //获取超时的定时器并挨个调用定时器的回调函数
    std::vector<Entry> expired = getExpired(now);
    callingExpiredTimers_ = true;
    for(const Entry& it : expired){
        it.second->run();//执行该定时器超时后的回调函数
    }

    reset(expired, now);
}

//处理超时定时器
void TimerQueue::reset(const std::vector<Entry> &expired, Timestamp now) {
    Timestamp nextExpire;//记录timerfd下一次的超时时刻
    for(const Entry & it : expired){
        if(it.second->repeat()){
            auto timer = it.second;
            timer->restart(Timestamp::now());
            insert(timer);
        }else{
            delete it.second;
        }
    }

    if(!timers_.empty()){
        nextExpire = timers_.begin()->second->expiration();
    }
    if(nextExpire.valid()){
        resetTimerfd(timerfd_, nextExpire);
    }
}

bool TimerQueue::insert(Timer *timer) {
    bool earliestChanged = false;
    Timestamp when = timer->expiration();
    TimerList::iterator it = timers_.begin();
    if(it == timers_.end() || when < it->first){
        earliestChanged = true;//说明最早的超时的定时器已经被替换了
    }
    timers_.insert(Entry(when, timer));
    return earliestChanged;
}