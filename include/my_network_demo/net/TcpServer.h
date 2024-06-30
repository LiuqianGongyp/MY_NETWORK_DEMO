//
// Created by lq on 2024/4/24.
//

#ifndef TINY_NET_WORK_TCPSERVER_H
#define TINY_NET_WORK_TCPSERVER_H

#include <unordered_map>

#include "EventLoop.h"
#include "EventLoopThreadPool.h"
#include "Acceptor.h"
#include "InetAddress.h"
#include "noncopyable.h"
#include "Callback.h"
#include "TcpConnection.h"

class TcpServer : noncopyable {
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    enum Option{
        kNoReusePort,
        kReusePort,
    };

    TcpServer(EventLoop * loop, const InetAddress &ListenAddr, const std::string &nameArg, Option option = kNoReusePort);
    ~TcpServer();

    void setThreadInitCallback(const ThreadInitCallback &cb) { threadinitcallback_ = cb; }
    void setConnectionCallback(const ConnectionCallback &cb) { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback &cb) { writeCompleteCallback_ = cb; }

    //设置底层subLoop的个数
    void setThreadNum(int numThreads);

    void start();//开启服务器

    EventLoop* getLoop() const { return loop_; }

    const std::string name() const { return name_; }

    const std::string ipPort() const { return ipPort_; }
private:
    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

    void newConnection(int sockfd, const InetAddress &peerAddr);

    void removeConnection(const TcpConnectionPtr &conn);

    void removeConnectionInLoop(const TcpConnectionPtr &conn);

    EventLoop *loop_; //用户定义的mainloop
    const std::string ipPort_; //传入的Ip地址和端口号
    const std::string name_; //TcpServer的名字
    std::unique_ptr<Acceptor> acceptor_; //用于监听和接收新连接的Acceptor
    std::shared_ptr<EventLoopThreadPool> threadPool_;

    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_; // 有读写消息时的回调函数
    ThreadInitCallback threadinitcallback_; //loop线程初始化的回调函数

    std::atomic_int started_;

    int nextConnId_;
    ConnectionMap connections_; //保存所有的连接

};

#endif //TINY_NET_WORK_TCPSERVER_H
