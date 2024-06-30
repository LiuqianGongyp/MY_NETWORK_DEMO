//
// Created by lq on 2024/4/24.
//

#ifndef TINY_NET_WORK_ACCEPTOR_H
#define TINY_NET_WORK_ACCEPTOR_H
#include "noncopyable.h"
#include "Socket.h"
#include "Channel.h"

class EventLoop;
class InetAddress;

/**
 * Acceptor运行在baseloop中
 * TcpServer发现Acceptor有一个新连接，则将此channel分发给一个subLoop
 */

class Acceptor : noncopyable{
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress&)>;
    Acceptor(EventLoop *loop, const InetAddress &ListenAddr, bool reuseport);
    ~Acceptor();

    void setNewConnectionCallback(const NewConnectionCallback &cb){
        newConnectionCallback_ = cb;
    }

    bool listenning() const { return listenning_; }
    void listen();
private:
    void handleRead();
    EventLoop *loop_;
    Socket acceptSocket_;
    Channel acceptChannel_;
    NewConnectionCallback newConnectionCallback_;
    bool listenning_;//是否正在监听
    int idleFd_; //防止fd数量超过上限，用于占位的fd
};

#endif //TINY_NET_WORK_ACCEPTOR_H
