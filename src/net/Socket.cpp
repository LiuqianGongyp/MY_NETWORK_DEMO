//
// Created by lq on 2024/6/20.
//
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/tcp.h>

#include "Socket.h"
#include "Logging.h"
#include "InetAddress.h"
Socket::~Socket(){
    ::close(sockfd_);
}

void Socket::bindAddress(const InetAddress &lockaddr) {
    if(0 != ::bind(sockfd_, (sockaddr *)lockaddr.getSockAddr(), sizeof(sockaddr_in))){
        LOG_FETAL << "bind sockfd:" << sockfd_ << " fail";
    }
}

void Socket::listen() {
    if(0 != ::listen(sockfd_, 1024)){
        LOG_FETAL << "listen sockfd:" << sockfd_ << " fail";
    }
}

int Socket::accept(InetAddress *peeraddr) {
    /**
     * Reactor模型 one loop per thread
     * poller + non-blocking IO
     */
    sockaddr_in addr;
    socklen_t len = sizeof(addr);
    ::memset(&addr, 0, sizeof(addr));

    int connfd = ::accept4(sockfd_, (sockaddr *)&addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);

    if(connfd >= 0){
        peeraddr->setSockAddr(addr);
    }else{
        LOG_ERROR << "accept4() failed";
    }
    return connfd;
}

void Socket::shutdownWrite() {
    if(::shutdown(sockfd_, SHUT_WR) < 0){//关闭写端，但可以接收客户端数据
        LOG_ERROR << "shutdownWrite error";
    }
}

void Socket::setTcpNoDelay(bool on) {//不启用Nagle算法
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
}

void Socket::setReuseAddr(bool on) {//设置地址复用，使可以使用处于Time-wait的端口
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
}

void Socket::setReusePort(bool on) {//多个进程绑定同一个地址，多个服务的ip+port相同
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
}

void Socket::setKeepAlive(bool on) {
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
}