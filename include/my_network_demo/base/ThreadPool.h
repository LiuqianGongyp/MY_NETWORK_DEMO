//
// Created by lq on 2024/6/11.
//

#ifndef MY_NETWORK_DEMO_THREADPOOL_H
#define MY_NETWORK_DEMO_THREADPOOL_H

#include "noncopyable.h"
#include "Thread.h"
#include "Logging.h"

#include <deque>
#include <vector>
#include <mutex>
#include <condition_variable>
class ThreadPool : noncopyable {
public:
    using ThreadFunction = std::function<void()>;

    explicit ThreadPool(const std::string& name = std::string("ThreadPool"));
    ~ThreadPool();

    void setThreadInitCallback(const ThreadFunction & cb) { threadInitCallback_ = cb; }
    void setThreadSize(const int & num) { threadSize_ = num; }
    void start();
    void stop();

    const std::string & name() const { return name_; }
    size_t queueSize() const;

    void add(ThreadFunction ThreadFunction);
private:
    bool isFull() const;
    void runInThread();

    mutable std::mutex mutex_;//需要在const函数中修改一些与类状态无关的成员变量，那么这个成员变量需要用mutable来修饰
    std::condition_variable cond_;
    std::string name_;
    ThreadFunction threadInitCallback_;
    std::vector<std::unique_ptr<Thread>> threads_;
    std::deque<ThreadFunction> queue_;
    bool running_;
    size_t threadSize_;
};


#endif //MY_NETWORK_DEMO_THREADPOOL_H
