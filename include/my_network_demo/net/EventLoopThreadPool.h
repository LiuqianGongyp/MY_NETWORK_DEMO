//
// Created by lq on 2024/4/24.
//

#ifndef TINY_NET_WORK_EVENTLOOPTHREADPOOL_H
#define TINY_NET_WORK_EVENTLOOPTHREADPOOL_H

#include "noncopyable.h"

#include <string>
#include <functional>
#include <memory>
#include <vector>

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool : noncopyable{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;
    EventLoopThreadPool(EventLoop *baseloop, const std::string &nameArg);
    ~EventLoopThreadPool();

    void setThreadnum(int numThreads) {
        numThreads_ = numThreads;
    }

    //启动线程池
    void start(const ThreadInitCallback &cb = ThreadInitCallback());

    //将新接受的连接分发给子Reactor时，通过轮询的方式获取应该分发给娜个子Reactor
    EventLoop* getNextLoop();

    std::vector<EventLoop*> getAllLoops();

    bool started() const { return started_; }

    const std::string name() const { return name_; }
private:
    EventLoop *baseLoop_;//主Reactor对应的EventLoop
    std::string name_;
    bool started_; //是否已经开启线程池
    int numThreads_;//线程的数目
    size_t next_;//轮询的下标
    std::vector<std::unique_ptr<EventLoopThread>> threads_;
    std::vector<EventLoop*> loops_;
};

#endif //TINY_NET_WORK_EVENTLOOPTHREADPOOL_H
