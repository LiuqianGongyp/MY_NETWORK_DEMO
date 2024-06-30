//
// Created by lq on 2024/6/24.
//

#ifndef MY_NETWORK_DEMO_TIMERQUEUE_H
#define MY_NETWORK_DEMO_TIMERQUEUE_H

#include "Timestamp.h"
#include "Channel.h"

#include <vector>
#include <set>

class EventLoop;//前向声明解决头文件的交叉引用
class Timer;

class TimerQueue {
public:
    using TimerCallback = std::function<void()>;
    explicit TimerQueue(EventLoop * loop);
    ~TimerQueue();
    //通过调用insert向TimerList中插入定时器
    void addTimer(TimerCallback cb, Timestamp when, double interval);
private:
    using Entry = std::pair<Timestamp, Timer*>;
    using TimerList = std::set<Entry>;//底层使用红黑树管理，自动按照时间戳进行排序

    //在本loop中添加定时器
    void addTimerInLoop(Timer *timer);

    //定时器读事件触发的函数
    void handleRead();

    //获取到期的定时器
    std::vector<Entry> getExpired(Timestamp now);

    //重置到期的定时器
    void reset(const std::vector<Entry>& expired, Timestamp now);

    //插入定时器的内部方法
    bool insert(Timer* timer);

    EventLoop* loop_; //所属的EventLoop
    const int timerfd_; //timerfd是Linux提供的定时器接口
    Channel timerfdChannel_;//封装timerfd_文件描述符
    TimerList timers_;//定时器队列
    bool callingExpiredTimers_;//是否正在获取超时定时器
};


#endif //MY_NETWORK_DEMO_TIMERQUEUE_H
