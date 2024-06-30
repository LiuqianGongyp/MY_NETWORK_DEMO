//
// Created by lq on 2024/6/25.
//

#ifndef MY_NETWORK_DEMO_POLLER_H
#define MY_NETWORK_DEMO_POLLER_H
#include "noncopyable.h"
#include "Channel.h"
#include "Timestamp.h"

#include <vector>
#include <unordered_map>

class Poller :noncopyable {
public:
    using ChannelList = std::vector<Channel*>;
    Poller(EventLoop * Loop):ownerLoop_(Loop){}
    virtual ~Poller() = default;
    virtual Timestamp poll(int timeoutMs, ChannelList * activeChannels) = 0;
    virtual void updateChannel(Channel *channel) = 0;
    virtual void removeChannel(Channel *channel) = 0;

    //判断channel是否注册到poller当中
    bool hasChannel(Channel *channel) const;

    //EventLoop可以通过该接口获取默认的IO复用实现方式（默认epoll）
    static Poller* newDefultPoller(EventLoop * Loop);
protected:
    using ChannelMap = std::unordered_map<int, Channel*>;
    ChannelMap channels_;//存储channel的映射
private:
    EventLoop *ownerLoop_;//定义Poller所属事件循环
};


#endif //MY_NETWORK_DEMO_POLLER_H
