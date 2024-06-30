//
// Created by lq on 2024/6/20.
//

#ifndef MY_NETWORK_DEMO_CHANNEL_H
#define MY_NETWORK_DEMO_CHANNEL_H
#include <functional>
#include <memory>
#include <sys/epoll.h>

#include "noncopyable.h"
//#include "Timestamp.h"
#include "Logging.h"

class EventLoop;
class Timestamp;

/**
 * 封装了sockfd和其感兴趣的事件，如EPOLLIN,EPOLLOUT
 * 还绑定了poller返回的具体事件
 */
class Channel : noncopyable {
public:
    using EventCallback = std::function<void()>;//void()表示不接受任何参数且没有返回值的函数
    //()中写接受的参数类型

    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop *loop, int fd);
    ~Channel();

    void handleEvent(Timestamp receiveTime);//fd接收到poller通知以后，处理事件的回调函数

    //设置回调函数对象
    //使用右值引用，避免拷贝操作
    void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

    //将TcpConnection的共享指针和Channel的成员弱指针tie_绑定，便于在Channel处理事件时
    //防止TcpConnection已经被析构了
    void tie(const std::shared_ptr<void>&);

    int fd() const { return fd_; }//返回封装的fd_
    int events() const { return events_; }//返回感兴趣的事件
    void set_revents(int revt) { revents_ = revt; }//设置Poller返回的发生事件

    //向epoll中注册、删除fd感兴趣的事件
    void enableReading(){ events_ |= kReadEvent; update(); }
    void disableReading(){ events_ &= ~kReadEvent; update(); }
    void enableWriting(){ events_ |= kWriteEvent; update(); }
    void disableWriting(){ events_ &= ~kWriteEvent; update(); }
    void disableAll(){ events_ &= kNoneEvent; update(); }

    //返回fd当前的事件状态
    bool isNoneEvent() const { return events_ == kNoneEvent; }
    bool isWriting() const { return events_ == kWriteEvent; }//是否注册了可写事件
    bool isReading() const { return events_ == kReadEvent; }//是否注册了可读事件

    /**
     * 返回fd在EPoller中的状态
     * for EPoller
     * const int kNew = -1; //fd还未被Epoller监视
     * const int kAdded = 1; //fd =正被Epoller监视中
     * const int kDeleted = 2; //fd被从Epoller中移除
     */
    int index() { return index_; }
    void set_index(int idx) { index_ = idx; }

    //返回Channel自己所属的loop
    EventLoop* ownerLoop() { return loop_; }
    //从EPoller中移除自己，
    //这不是销毁Channel，Channel的生命周期和TcpConnection一样长，因为Channel是TcpConnection的成员
    void remove();
private:
    void update();
    void handleEventWithGuard(Timestamp receiveTime);

    /**
     * const int Channel::KNoneEvent = 0;
     * const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
     * const int Channel::kWriteEvent = EPOLLOUT;
     */
     static const int kNoneEvent;
     static const int kReadEvent;
     static const int kWriteEvent;

     EventLoop *loop_; //当前Channel属于的EventLoop
     const int fd_;//fd, Poller的监听对象
     int events_; //注册fd感兴趣的事件
     int revents_; //poller返回的具体发生的事件
     int index_; //在Poller上注册的情况

     //std::weak_ptr<void>是指向任何类型的弱指针
     std::weak_ptr<void> tie_;//弱指针指向TcpConnection
     bool tied_;//标记此Channel是否被调用过Channel::tie方法

     //因为channel通道里面能够获知fd最终发生的具体的事件
     //保存事件到来时的回调函数
     ReadEventCallback readCallback_;
     EventCallback writeCallback_;
     EventCallback closeCallback_;
     EventCallback errorCallback_;
};


#endif //MY_NETWORK_DEMO_CHANNEL_H
