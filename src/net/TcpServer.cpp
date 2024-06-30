//
// Created by lq on 2024/6/25.
//
#include "TcpServer.h"

TcpServer::TcpServer(EventLoop *loop, const InetAddress &ListenAddr, const std::string &nameArg,
                     TcpServer::Option option) :
                     loop_(loop),
                     ipPort_(ListenAddr.toIpPort()),
                     name_(nameArg),
                     acceptor_(new Acceptor(loop, ListenAddr, option == kReusePort)),
                     threadPool_(new EventLoopThreadPool(loop, name_)),
                     connectionCallback_(defaultConnectionCallback),
                     messageCallback_(defaultMessageCallback),
                     writeCompleteCallback_(),
                     threadinitcallback_(),
                     started_(0),
                     nextConnId_(1){
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this, std::placeholders::_1, std::placeholders::_2));

}

TcpServer::~TcpServer(){
    for(auto & item : connections_){
        TcpConnectionPtr conn(item.second);
        //把智能指针复位，让栈空间的TcpConnectionPtr conn指向该对象，当TcpConnectionPtr conn出了其作用域，即可释放只能指针指向的对象
        item.second.reset();
        //销毁连接
        conn->getLoop()->runInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
    }
}

void TcpServer::setThreadNum(int numThreads) {
    threadPool_->setThreadnum(numThreads);
}

void TcpServer::start() {
    if(started_++ == 0){
        //启动底层的loop线程池
        threadPool_->start((threadinitcallback_));
        //acceptor_get()  绑定时候需要地址
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
    }
}

void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr) {
    //轮询算法 选择一个subLoop 来管理connfd对应的channel
    EventLoop *ioLoop = threadPool_->getNextLoop();
    //提示信息
    char buf[64] = {0};
    snprintf(buf, sizeof(buf), "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;//只在mainloop中执行，不涉及线程安全问题
    //新连接名字
    std::string connName = name_ + buf;
    LOG_INFO << "TcpServer::newConnection [" << name_.c_str() << "] - new connection ["
    << connName.c_str() << "] from" << peerAddr.toIpPort().c_str();

    //通过sockfd获取其绑定的本机的ip地址和端口信息
    sockaddr_in local;
    ::memset(&local, 0, sizeof(local));
    socklen_t addrlen = sizeof(local);
    if(::getsockname(sockfd, (sockaddr *) &local, &addrlen) < 0){
        LOG_ERROR << "sockets::getLocakAddr() failed";
    }

    InetAddress localAddr(local);
    TcpConnectionPtr conn(new TcpConnection(ioLoop, connName, sockfd, localAddr, peerAddr));
    connections_[connName] = conn;

    //下面三个回调时用户设置给TcpServer::TcpConnection的
    //Channel绑定的则是handleRead, handleWrite,...四个，
    //下面的几个回调用于hadneleXXX函数中
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    conn->setCloseCallback(std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));

    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}

void TcpServer::removeConnection(const TcpConnectionPtr &conn) {
    loop_->runInLoop((std::bind(&TcpServer::removeConnectionInLoop, this, conn)));
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn) {
    LOG_INFO << "TcpServer::removeConnectionInLoop [" << name_.c_str() << "] - connection " << conn->name().c_str();

    connections_.erase(conn->name());
    EventLoop *ioLoop = conn->getLoop();
    ioLoop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
}