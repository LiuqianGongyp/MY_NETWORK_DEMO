//
// Created by lq on 2024/6/25.
//

#include "EventLoopThread.h"
#include "EventLoop.h"

EventLoopThread::EventLoopThread(const EventLoopThread::ThreadInitCallback &cb, const std::string &name) :
loop_(nullptr),
exiting_(false),
thread_(std::bind(&EventLoopThread::threadFunc, this), name),
mutex_(),
cond_(),
callback_(cb){//传入的线程初始化回调函数，用户定义的

}

EventLoopThread::~EventLoopThread() {
    exiting_ = true;
    if(loop_ != nullptr){
        loop_->quit();
        thread_.join();
    }
}

EventLoop* EventLoopThread::startLoop() {
    //启动一个新线程
    //在构造函数中thread_已经绑定了回调函数threadFunc，绑定的是EventLoopThread::threadFunc()
    //此时，子线程开始执行threadFunc函数
    thread_.start();
    EventLoop *loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while(loop_ == nullptr){
            cond_.wait(lock);
        }
        loop = loop_;
    }
    return loop;
}

void EventLoopThread::threadFunc() {
    EventLoop loop;
    if(callback_){
        callback_(&loop);
    }
    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;//等到生成EventLoop对象后才唤醒
        cond_.notify_one();
    }
    //执行EventLoop::loop() 开启底层EPoller的poll()
    loop.loop();//这个是subLoop

    //loop是一个事件循环，如果往下执行，说明停止了事件循环，需要关闭EventLoop
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr;
}