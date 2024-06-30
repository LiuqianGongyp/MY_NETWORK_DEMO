//
// Created by lq on 2024/6/24.
//
#include "TcpConnection.h"
#include "Logging.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"

#include <functional>
#include <string.h>
#include <string>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

void defaultConnectionCallback(const TcpConnectionPtr& conn){
    LOG_TRACE << conn->localAddress().toIpPort() << " -> "
            << conn->peerAddress().toIpPort() << " is "
            << (conn->connected() ? "UP" : "DOWN");
}

void defaultMessageCallback(const TcpConnectionPtr&, Buffer* buf, Timestamp){
    LOG_TRACE << "receive " << buf->readableBytes() << " bytes: " << buf->retrieveAllAsString();
}

static EventLoop* CheckLoopNotNull(EventLoop *loop){
    //TcpServer::start会生成新线程和对应的Eventloop
    if(loop == nullptr){
        LOG_FETAL << "mainLoop is null";
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop *loop, const std::string &nameArg, int sockfd, const InetAddress &localAddr,
                             const InetAddress &peerAddr) :
                             loop_(loop),
                             name_(nameArg),
                             state_(kConnecting),
                             reading_(true),
                             socket_(new Socket(sockfd)),
                             channel_(new Channel(loop, sockfd)),
                             localAddr_(localAddr),
                             peerAddr_(peerAddr),
                             hightWaterMark_(64 * 1024 * 1024){//64M
    //给channel设置回调函数
    //std::placeholders::_1是占位符
    channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(std::bind(&TcpConnection::handleError, this));

    LOG_INFO << "TcpConnection::dtor[" <<name_.c_str() <<"] at fd = " << sockfd;
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection(){
    LOG_INFO << "TcpConnection::deletor[" << name_.c_str() << "] at fd = " << channel_->fd() << "state = " << static_cast<int>(state_);
}

void TcpConnection::send(const std::string &buf){
    if(state_ == kConnected){
        if(loop_->isInLoopThread()){
            sendInLoop(buf.c_str(), buf.size());
        }else{
            //void(TcpConnection::*)(const std::string&)是一个成员函数指针
            //表示TcpConnection中的一个成员函数，这个成员函数的返回类型是void，并且接受一个const std::string&类型的参数
            loop_->runInLoop(std::bind((void(TcpConnection::*)(const std::string&))&TcpConnection::sendInLoop, this, buf));
        }
    }
}

void TcpConnection::send(Buffer *buf) {
    if(state_ == kConnected){
        if(loop_->isInLoopThread()){
            sendInLoop(buf->retrieveAllAsString());
        }else{
            std::string msg = buf->retrieveAllAsString();
            loop_->runInLoop(std::bind((void(TcpConnection::*)(const std::string&))&TcpConnection::sendInLoop, this, msg));
        }
    }
}

void TcpConnection::sendInLoop(const std::string &message) {
    sendInLoop(message.data(), message.size());
}

void TcpConnection::sendInLoop(const void *message, size_t len) {
    ssize_t nwrote = 0; //已经发送的数据长度
    size_t remaining = len; //还剩下多少数据需要发送
    bool faultError = false; //记录是否产生错误

    if(state_ == kDisconnected){//已经断开连接
        LOG_ERROR << "disconnected, give up writing";
        return;
    }
    //刚初始化的channel和数据发送完毕的channel都是没有可写事件在epoll上的，即isWriting返回false
    //channel第一次写数据，且缓冲区没有待发送数据
    if(!channel_->isWriting() && outputBuffer_.readableBytes() == 0){
        nwrote = ::write(channel_->fd(), message, len);
        if(nwrote >= 0){
            remaining = len - nwrote;
            if(remaining == 0 && writeCompleteCallback_){
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }else{
            nwrote = 0;
            if(errno != EWOULDBLOCK) {//EWOULDBLOCK表示非阻塞情况下没有数据后的正常返回 等同于EAGAIN
                LOG_ERROR << "TcpConnection::sendInLoop";
                if(errno == EPIPE || errno == ECONNRESET){//EPIPE表示管道破裂，ECONNRESET表示连接被对端重置
                    faultError = true;
                }
            }
        }
    }
    //一次性没有发送完数据，剩余数据需要保存到缓冲区中，且需要channel注册写事件
    if(!faultError && remaining > 0){
        size_t oldlen = outputBuffer_.readableBytes();//目前发送缓冲区剩余的待发送的数据的长度
        //判断待写数据是否会超过设置的高标志位
        if(oldlen + remaining >= hightWaterMark_ && oldlen < hightWaterMark_ && highWaterMarkCallback_){
            loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldlen + remaining));
        }
        outputBuffer_.append((char *)message + nwrote, remaining);
        if(!channel_->isWriting()){
            channel_->enableWriting();//若不注册，poller不会给channel通知epollout
        }
    }
}

void TcpConnection::shutdown() {
    if(state_ == kConnected){
        setState(kDisconnecting);
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
    }
}

void TcpConnection::shutdownInLoop() {
    if(!channel_->isWriting()){//说明当前outputBuffe_的数据全部向外发送完成
        socket_->shutdownWrite();
    }
}

void TcpConnection::connectEstablished() {
    setState(kConnected);

    channel_->tie(shared_from_this());
    channel_->enableReading();//向EPoller注册channel的EPOLLIN读事件，
    // EPOLLIN在网络编程中通常表示可以从套接字中读取数据或者文件可以读取
    // 可以想成服务器接收数据

    //新连接建立 执行回调
    connectionCallback_(shared_from_this());
}

void TcpConnection::connectDestroyed() {
    if(state_ == kConnected){
        setState(kDisconnected);
        channel_->disableAll();
        connectionCallback_(shared_from_this());
    }
    channel_->remove();
}

void TcpConnection::handleRead(Timestamp receiveTime) {
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readfd(channel_->fd(), &savedErrno);
    if(n > 0){
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }else if(n == 0) {//对方关闭了连接
        handleClose();
    }else{
        errno = savedErrno;//errno是一个全局变量
        LOG_ERROR << "TcpConnection::handleRead() failed";
        handleError();
    }
}

void TcpConnection::handleWrite() {
    if(channel_->isWriting()){
        int savedErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &savedErrno);
        if(n > 0){
            outputBuffer_.retrieve(n);//把outputBuffer_的readerIndex往前移动n个字节，
            // 因为outputBuffer_中的readableBytes已经发送出去了n字节
            if(outputBuffer_.readableBytes() == 0){
                channel_->disableWriting();//数据发送完毕以后，注销写事件
                if(writeCompleteCallback_){
                    loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
                }
                if(state_ == kDisconnecting){
                    shutdownInLoop();
                }
            }
        }else{
            LOG_ERROR << "TcpConnection::handleWrite() failed";
        }
    }else{
        LOG_ERROR << "TcpConnection fd = " << channel_->fd() << " is down, no more writing";
    }
}

void TcpConnection::handleClose() {
    setState(kDisconnected);
    channel_->disableAll();
    TcpConnectionPtr  connPtr(shared_from_this());
    connectionCallback_(connPtr);//
    closeCallback_(connPtr);
}

void TcpConnection::handleError() {
    int optval;
    socklen_t  optlen = sizeof(optval);
    int err = 0;
    //获取并清除socket的错误状态
    if(::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0){
        err = errno;
    }else{
        err = optval;
    }
    LOG_ERROR << "TcpConnection::handleError name:" << name_.c_str() << " - SO_ERROR:" << err;
}