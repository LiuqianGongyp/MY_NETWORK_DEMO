//
// Created by lq on 2024/4/24.
//

#ifndef TINY_NET_WORK_EVENTLOOP_H
#define TINY_NET_WORK_EVENTLOOP_H
#include "noncopyable.h"
#include "Timestamp.h"
#include "CurrentThread.h"

#include <vector>
#include <atomic>
#include <memory>
#include <mutex>
#include <functional>

class Channel;
class EPollPoller;
class TimerQueue;
class EventLoop : noncopyable{
public:
    using Functor = std::function<void()>;
    EventLoop();
    ~EventLoop();

    void loop();
    void quit();

    Timestamp epollReturnTime() const { return epollReturnTime_; }

    void runInLoop(Functor cb);//让回调函数在EventLoop绑定的线程中执行
    void queueInLoop(Functor cb);//让回调函数添加到EventLoop的pendingFunctors_中，
    // 以便以后在EventLoop绑定的线程中执行

    //唤醒loop所在的线程
    void wakeup();

    //EventLoop的方法 => Poller
    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);//EPoller以后不再关注channel上的事件
    bool hasChannel(Channel* channel);

    //判断EventLoop是否在自己的线程
    bool isInLoopThread() const { return  threadId_ == CurrentThread::tid(); }

    /**
     * 定时任务相关函数
     */
    void runAt(Timestamp timestamp, Functor&& cb);//在time时刻执行cb
    void runAfter(double waitTime, Functor&& cb);//在waitTime秒后执行cb
    void runEvery(double interval, Functor&& cb);//每隔interval秒执行一次cb
private:
    using ChannelList = std::vector<Channel*>;

    //wakeupChannel_可读事件的回调函数
    void handleRead();
    void doPendingFunctors();

    std::atomic_bool looping_; //是否正在事件循环中
    std::atomic_bool quit_;//是否退出事件循环
    std::atomic_bool callingPendingFunctiors_;//是否正在调用待执行的函数
    const pid_t threadId_;//当前loop所属线程的id
    Timestamp epollReturnTime_;//EPoller管理的fd有事件发生时的时间
    std::unique_ptr<EPollPoller> epoller_;
    std::unique_ptr<TimerQueue> timerQueue_;//管理当前loop所有定时器的容器

    //wakeupFd_用于唤醒EPoller，以免EPoller阻塞了无法执行PendingFunctiors_中的待处理的函数
    int wakeupFd_;
    std::unique_ptr<Channel> wakeupChannel_;//wakeuoFd_对应的Channel
    ChannelList activeChannels_;//有事件发生的Channel集合
    std::mutex mutex_;//用于保护pendingFunctors_线程安全操作
    std::vector<Functor> pendingFunctors_;//存储loop跨线程需要执行的所有回调操作
};
#endif //TINY_NET_WORK_EVENTLOOP_H
