//
// Created by lq on 2024/6/25.
//

#ifndef MY_NETWORK_DEMO_EPOLLPOLLER_H
#define MY_NETWORK_DEMO_EPOLLPOLLER_H

#include "noncopyable.h"
#include "Timestamp.h"
#include "Poller.h"

#include <sys/epoll.h>
#include <unistd.h>
#include <vector>

class Channel;
class EventLoop;

class EPollPoller : public Poller{
public:
    using ChannelList = std::vector<Channel*>;
    EPollPoller(EventLoop *Loop);
    ~EPollPoller() override;

    Timestamp poll(int timeoutMs, ChannelList * activeChannels) override;//将有事件发生的channel通过activeChannels返回

    void updateChannel(Channel * channel) override;//更新channel上感兴趣的事件
    void removeChannel(Channel * channel) override;//当连接销毁时，从EPoller移除channel
    //bool hasChannel(Channel * channel) const;//判断channel是否已经注册到EPoller
private:
    //using ChannelMap = std::unordered_map<int, Channel*>;
    using EventList = std::vector<epoll_event>;

    void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;//将有事件发生的channel添加到activeChannels中
    void update(int operation, Channel *channel);

    //默认监听事件的数量
    static const int kInitEventListSize = 16;

    //ChannelMap channels_;//存储channel的映射
    //EventLoop * ownerLoop_;//EPoller所属事件循环EventLoop
    int epollfd_;//每个EPollPoller都有一个epollfd_，epollfd_是epoll_create在内核创建空间返回的fid
    EventList events_; //用于存放epoll_wait返回的所有发生的事件
};


#endif //MY_NETWORK_DEMO_EPOLLPOLLER_H
