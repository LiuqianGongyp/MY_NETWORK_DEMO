//
// Created by lq on 2024/6/24.
//

#ifndef MY_NETWORK_DEMO_TIMER_H
#define MY_NETWORK_DEMO_TIMER_H
#include "noncopyable.h"
#include "Timestamp.h"
#include <functional>

/*
 * 定时器
 * 定时器回调函数，下一次超时时刻，重复定时器的时间间隔等
 */
class Timer : noncopyable{
public:
    using TimerCallback = std::function<void()>;
    Timer(TimerCallback cb, Timestamp when, double interval) :
    callback_(std::move(cb)),
    expiration_(when),
    interval_(interval),
    repeat_(interval > 0.0)//interval > 0.0则认为是重复定时器
    {}

    void run() const { callback_(); }

    Timestamp expiration() const { return expiration_; }//返回下一次超时时刻
    bool repeat() const { return repeat_; }

    //重启定时器
    void restart(Timestamp now);

private:
    const TimerCallback callback_;//定时器的回调函数
    Timestamp expiration_;//下一次超时时刻
    const double interval_;//超时时间间隔
    const bool repeat_;//是否重复（false表示是一次性定时器）
};


#endif //MY_NETWORK_DEMO_TIMER_H
