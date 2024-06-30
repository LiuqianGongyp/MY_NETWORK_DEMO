//
// Created by lq on 2024/6/25.
//
#include "EventLoopThreadPool.h"
#include "EventLoop.h"
#include "EventLoopThread.h"

#include <assert.h>

EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseloop, const std::string &nameArg) :
baseLoop_(baseloop),
name_(nameArg),
started_(false),
numThreads_(0),
next_(0){

}

EventLoopThreadPool::~EventLoopThreadPool(){
    //不要删除loop，他是一个栈变量
}

void EventLoopThreadPool::start(const EventLoopThreadPool::ThreadInitCallback &cb) {
    started_ = true;
    for(int i = 0; i < numThreads_; ++i){
        char buf[name_.size() + 32];
        snprintf(buf, sizeof(buf), "%s%d", name_.c_str(), i);
        EventLoopThread *t = new EventLoopThread(cb, buf);
        threads_.push_back(std::unique_ptr<EventLoopThread>(t));
        //t->startLoop()就开始执行新线程了
        //底层 创建线程-绑定一个新的EvenLoop-返回该loop的地址
        loops_.push_back(t->startLoop());
    }

    if(numThreads_ == 0 && cb){
        cb(baseLoop_);
    }
}

EventLoop* EventLoopThreadPool::getNextLoop() {
    assert(started_);

    EventLoop *loop = baseLoop_;//若只设置一个线程，也就是只有一个mainReactor，则每次都会返回baseloop_

    //通过轮询获取下一个处理事件的loop
    if(!loops_.empty()){
        loop = loops_[next_];
        ++next_;
        if(next_ > loops_.size()){
            next_ = 0;
        }
    }

    return loop;
}

std::vector<EventLoop *> EventLoopThreadPool::getAllLoops() {
    if(loops_.empty()){
        return std::vector<EventLoop*> (1, baseLoop_);
    }else{
        return loops_;
    }
}