//
// Created by lq on 2024/6/25.
//

#include "EPollPoller.h"
#include <string.h>

#include <assert.h>

const int kNew = -1;//某个channel还没添加至Poller
const int kAdded = 1;//某个channel添加至Poller
const int kDeleted = 2;//某个channel已经从Poller删除

EPollPoller::EPollPoller(EventLoop *Loop) :
        Poller(Loop),//传给基类
        //epoll_create1是epoll_create的升级版，可以接受一个flags参数，用来制定创建行为
        //比如：EPOLL_CLOEXEC，可以避免文件描述符泄漏
        epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
        events_(kInitEventListSize) {
    if (epollfd_ < 0) {
        LOG_FETAL << "epoll_create() error: " << errno;
    }else{
        LOG_DEBUG << "create a new epollfd, fd = " << epollfd_;
    }
}

EPollPoller::~EPollPoller(){
    ::close(epollfd_);
};

Timestamp EPollPoller::poll(int timeoutMs, EPollPoller::ChannelList *activeChannels) {
    //等待I/O事件发生，并将这些事件存储在一个预先分配的事件数组events_中
    size_t numEvents = ::epoll_wait(epollfd_, &(*events_.begin()),
                                    static_cast<int>(events_.size()), timeoutMs);
    int saveErrno = errno;
    Timestamp now(Timestamp::now());

    if(numEvents > 0){
        fillActiveChannels(numEvents, activeChannels);//填充活跃的channels
        if(numEvents == events_.size()){
            events_.resize(events_.size() * 2);
        }
    }else if(numEvents == 0){//超时
        LOG_DEBUG << "timeout!";
    }else{
        //EINTR表示系统调用被信号中断
        if(saveErrno != EINTR){
            errno = saveErrno;
            LOG_ERROR << "EPollPoller::poll() failed";
        }
    }
    return now;
}

void  EPollPoller::updateChannel(Channel *channel) {
    const int index = channel->index();//获取channel在epoll的状态
    if(index == kNew || index == kDeleted){//未添加和已删除状态都有可能会被再次添加到epoll中
        int fd = channel->fd();
        if(index == kNew){
			assert(channels_.find(fd) == channels_.end());
            channels_[fd] = channel;//添加到键值对
        }else{//index == kDeleted
            assert(channels_.find(fd) != channels_.end());
            assert(channels_[fd] == channel);
        }
        //修改为已添加状态
        channel->set_index(kAdded);
        //向epoll对象加入channel
        update(EPOLL_CTL_ADD, channel);
    }else{
        if(channel->isNoneEvent()){
            //没有感兴趣事件说明可以从epoll多项中删除channel
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        }else{
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

void EPollPoller::fillActiveChannels(int numEvents, EPollPoller::ChannelList *activeChannels) const {
    for(int i = 0; i < numEvents; ++i){
        Channel *channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        activeChannels->emplace_back(channel);
    }
}

void EPollPoller::removeChannel(Channel *channel) {
    int fd = channel->fd();
    channels_.erase(fd);

    int index = channel->index();
    if(index == kAdded){
        //如果fd已经被添加到Poller中了，还需要删除
        update(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(kNew);//状态设为未注册
}

void EPollPoller::update(int operation, Channel *channel) {
    epoll_event event;
    ::memset(&event, 0, sizeof(event));

    int fd = channel->fd();
    event.events = channel->events();
    event.data.fd = fd;
    event.data.ptr = channel;

    if(::epoll_ctl(epollfd_, operation, fd, &event) < 0){
        if(operation == EPOLL_CTL_DEL){
            LOG_ERROR << "epoll_ctl() del error:" << errno;
        }else{
            LOG_FETAL << "epoll_ctl add/mod error:" << errno;
        }
    }
}