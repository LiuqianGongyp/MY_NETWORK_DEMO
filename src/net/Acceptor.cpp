//
// Created by lq on 2024/6/25.
//
#include "Acceptor.h"
#include "Logging.h"
#include "InetAddress.h"

#include <functional>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

static int createNonblocking(){
    //SOCK_STREAM表示面向连接的流套接字
    //SOCK_NONBLOCK表示套接字是非阻塞的
    //SOCK_CLOEXEC防止子进程继承不必要的文件描述符
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC , IPPROTO_TCP);
    if(sockfd < 0){
        LOG_FETAL << "listen socket create err " << errno;
    }
    return sockfd;
}


Acceptor::Acceptor(EventLoop *loop, const InetAddress &ListenAddr, bool reuseport) :
loop_(loop),
acceptSocket_(createNonblocking()),
acceptChannel_(loop, acceptSocket_.fd()),
listenning_(false){
    LOG_DEBUG << "Acceptor create nonblocking socket, [fd = " << acceptChannel_.fd() << "]";
    acceptSocket_.setReuseAddr(reuseport);//允许在同一个端口和地址重新绑定一个套接字
    acceptSocket_.setReusePort(true);//允许多个套接字绑定到同一个地址和端口
    acceptSocket_.bindAddress(ListenAddr);

    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor(){
    acceptChannel_.disableAll();
    acceptChannel_.remove();
    ::close(idleFd_);
}

void Acceptor::listen() {
    listenning_ = true;
    acceptSocket_.listen();
    //将acceptChannel的读事件注册到poller
    acceptChannel_.enableReading();
}

void Acceptor::handleRead() {
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    if(connfd >= 0){//确实有新连接到来
        if(newConnectionCallback_){
            newConnectionCallback_(connfd, peerAddr);
        }else{
            LOG_DEBUG << "no newConnectionCallback() function";
            ::close(connfd);
        }
    }else{
        LOG_ERROR << "accept() failed";
        //EMFILE表示当前进程打开的文件描述符已经到达系统或进程的上限，
        //可以调整单个服务器的上限，也可以分布式部署
        if(errno == EMFILE){
            LOG_INFO << "sofd reached limit";
            ::close(idleFd_);
            idleFd_ = ::accept(acceptSocket_.fd(), NULL, NULL);
            ::close(idleFd_);
            idleFd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
        }
    }
}