//
// Created by lq on 2024/6/20.
//

#include "Channel.h"
#include "EventLoop.h"

#include <sys/epoll.h>

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;//同时监视两个事件，EPOLLIN表示对应的文件描述符可以读取数据，
//EPOLLPRI表示对应的文件描述符有紧急数据可读
const int Channel::kWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop *loop, int fd) :
loop_(loop),
fd_(fd),
events_(0),
revents_(0),
index_(-1),
tied_(false){

}

Channel::~Channel(){
    if(loop_->isInLoopThread()){
        assert(!loop_->hasChannel(this));
    }
}

void Channel::tie(const std::shared_ptr<void> &obj) {
    //tie_指向obj，可以用tie_来判断TcpConnection是否还活着
    tie_ = obj;
    tied_ = true;
}

/**
 * 更新fd感兴趣的事件，比如Channel刚创建时，需要让Channel管理的fd关注可读事件，此时需要调用Channel::enableReading()
 * 该函数内部再调用update()。update()的本质时在EPoller类中，调用了epoll_ctl，来实现对感兴趣事件的修改
 */
void Channel::update() {
    //通过该Channel所属的EvenLoop，调用EPoller对应的方法，注册fd的events事件
    loop_->updateChannel(this);
}

void Channel::remove() {
    //把channel从EPollPoller的channels_中移除掉，同时把这个channel的状态标记为kNew
    loop_->removeChannel(this);
}

void Channel::handleEvent(Timestamp receiveTime) {
    /**
     * 调用Channel::tie，会将tid_设置为true
     * 而TcpConnection::connectEstablished会调用channel_->tie(shared_from_this());
     * 所以对于TcpConnection::channel_需要多一分强引用的保证以免用户误删TcpConnection对象
     */
    if(tied_){
        //使用.lock()获取一个指向TcpConnection对象的shared_ptr指针，前提是TcpConnection对象未被销毁
        std::shared_ptr<void> guard = tie_.lock();//若guard为空，则说明TcpConnection对象已经不存在
        if(guard){
            handleEventWithGuard(receiveTime);
        }
    }else{
        handleEventWithGuard(receiveTime);
    }
}

void Channel::handleEventWithGuard(Timestamp receiveTime) {
    //EPOLLHUP表示对方关闭连接
    if((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)){//对方挂断连接且没有可读消息
        if(closeCallback_) closeCallback_();
    }
    if(revents_ & EPOLLERR){
        LOG_ERROR << "the fd = " << this->fd();
        if(errorCallback_) errorCallback_();
    }
    if(revents_ & (EPOLLIN | EPOLLPRI)){
        LOG_DEBUG << "channel have read events, the fd = " << this->fd();
        if(readCallback_){
            LOG_DEBUG << "channel call the readCallback(), the fd = " << this->fd();
            readCallback_(receiveTime);
        }
    }
    if(revents_ & EPOLLOUT){
        if(writeCallback_) writeCallback_();
    }
}