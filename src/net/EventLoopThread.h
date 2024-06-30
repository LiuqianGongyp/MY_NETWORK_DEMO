//
// Created by lq on 2024/6/25.
//

#ifndef MY_NETWORK_DEMO_EVENTLOOPTHREAD_H
#define MY_NETWORK_DEMO_EVENTLOOPTHREAD_H

#include "noncopyable.h"
#include "Thread.h"

#include <mutex>
#include <condition_variable>

class EventLoop;
/**
 * 此类的作用是将EventLoop和Thread联系起来
 */
class EventLoopThread :noncopyable {
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback(),
                    const std::string& name = std::string());

    ~EventLoopThread();

    //开启一个新线程
    EventLoop *startLoop();
private:
    //线程执行函数
    void threadFunc();
    EventLoop* loop_;
    bool exiting_;
    Thread thread_;
    std::mutex mutex_;
    std::condition_variable cond_;
    ThreadInitCallback callback_;
};


#endif //MY_NETWORK_DEMO_EVENTLOOPTHREAD_H
