//
// Created by lq on 2024/6/24.
//
#include "EventLoop.h"
#include "Channel.h"
#include "EPollPoller.h"
#include "TimerQueue.h"

#include <sys/eventfd.h>

__thread EventLoop *t_loopInThisThread = nullptr;
//用来防止一个线程创建多个EventLoop
//当一个EventLoop被创建起来，t_loopInThisThread会指向这个EventLoop对象
//若线程想创建再一个EventLoop对象，但t_loopInThisThread非空，将无法创建

//定义默认的EPoller IO复用接口的超时时间
const int kPollTimeMs = 10000;

int createEventfd(){
    //eventfd是一种事件通知机制，
    int evfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if(evfd < 0) LOG_FETAL << "eventfd error: " << errno;
    else LOG_DEBUG << "create a new wakeupfd, fd = " << evfd;
    return evfd;
}

EventLoop::EventLoop() :
looping_(false),
quit_(false),
callingPendingFunctiors_(false),
threadId_(CurrentThread::tid()),
epoller_(new EPollPoller(this)),
timerQueue_(new TimerQueue(this)),
wakeupFd_(createEventfd()),
wakeupChannel_(new Channel(this, wakeupFd_)){
    LOG_DEBUG << "EventLoop created " << this <<  ", the threadId is " << threadId_;
    if(t_loopInThisThread){
        LOG_FETAL << "Another EventLoop " << t_loopInThisThread << " exists int this thread" << threadId_;
    }else{
        t_loopInThisThread = this;
    }
    //void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
    //using ReadEventCallback = std::function<void(Timestamp)>;
    //void EventLoop::handleRead()类型是否不一致
    //std::function<void()>可以传递给std::function<void(Timestamp)>
    // 只要保证函数运行时不需要这个参数
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));

    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop(){
    wakeupChannel_->disableAll();//移除感兴趣的事件
    wakeupChannel_->remove();//从EPollPoller中删除，同时让epollfd不再关注wakeupfd_上发生的任何事件
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

void EventLoop::loop() {
    looping_ = true;
    quit_ = false;
    LOG_INFO << "EventLoop " << this << " start looping";
    while(!quit_){
        activeChannels_.clear();
        epollReturnTime_ = epoller_->poll(kPollTimeMs, &activeChannels_);
        for(Channel *channel : activeChannels_){
            channel->handleEvent(epollReturnTime_);
        }
        doPendingFunctors();
    }
    looping_ = false;
}

void EventLoop::quit() {
    quit_ = true;
    //由void EventLoop::loop() 的代码可以看到，若当前loop没有任何事件发生，
    //会阻塞在epoller_->poll，因此需要向wakeupFd_写入数据，以解除阻塞
    if(!isInLoopThread()) wakeup();
}

void EventLoop::runInLoop(EventLoop::Functor cb) {
    //如果当前调用runInLoop的线程正好是EventLoop绑定的线程，则直接执行此函数
    //否则就将回调函数通过queueInLoop()存储到pendingFunctors_中
    if(isInLoopThread()){
        cb();
    }else{
        queueInLoop(cb);
    }
}

void EventLoop::queueInLoop(EventLoop::Functor cb) {//把回调函数放入pendingFunctors_并在必要的时候唤醒EventLoop绑定的线程
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }
    //在执行回调函数的过程中也可能会向pendingFunctors_中添加新的回调，
    // 若此时不进行唤醒，会发生有时间到来但是仍被阻塞的情况
    // 而callingPendingFunctiors_正在调用待执行的函数，即在执行回调函数，
    // 因此callingPendingFunctiors_为true时应进行唤醒
    if(!isInLoopThread() || callingPendingFunctiors_){
        wakeup();
    }
}

void EventLoop::wakeup() {
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof(one));
    if(n != sizeof(one)){
        LOG_ERROR << "EventLoop::wakeup wtires " << n << " bytes instead of 8";
    }
}

//wakeupChannel_可读事件的回调函数
void EventLoop::handleRead() {
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof(one));
    if(n != sizeof(one)){
        LOG_ERROR << "EventLoop::handleRead() reads " << n << " bytes instead of 8";
    }
}

void EventLoop::updateChannel(Channel *channel) {
    epoller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel) {
    epoller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel *channel) {
    return epoller_->hasChannel(channel);
}

void EventLoop::runAt(Timestamp timestamp, EventLoop::Functor &&cb) {
    timerQueue_->addTimer(std::move(cb), timestamp, 0.0);
}

void EventLoop::runAfter(double waitTime, EventLoop::Functor &&cb) {
    Timestamp time(addTime(Timestamp::now(), waitTime));
    runAt(time, std::move(cb));
}

void EventLoop::runEvery(double interval, EventLoop::Functor &&cb) {
    Timestamp timestamp(addTime(Timestamp::now(), interval));
    timerQueue_->addTimer(std::move(cb), timestamp, interval);
}

void EventLoop::doPendingFunctors() {
    std::vector<Functor> functors;
    callingPendingFunctiors_ = true;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);//交换的方式减少了锁的临界区范围，提升效率、避免死锁
    }
    for(const Functor &functor : functors){
        functor();
    }
    callingPendingFunctiors_ = false;
}