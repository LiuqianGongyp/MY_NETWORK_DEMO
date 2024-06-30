//
// Created by lq on 2024/6/30.
//

#include "MysqlConnectionPool.h"

#include <fstream>
#include <thread>
#include <assert.h>

MysqlConnectionPool* MysqlConnectionPool::getConnectionPool() {
    static MysqlConnectionPool pool;
    return &pool;
}

MysqlConnectionPool::MysqlConnectionPool() {
    parseJsonFile(filePath_);
    for(int i = 0; i < minSize_; ++i){
        addConnection();
        currentSize_++;
    }
    std::thread producer(&MysqlConnectionPool::produceConnection, this);
    std::thread recycler(&MysqlConnectionPool::recycleConnection, this);

    //设置线程分离
    producer.detach();
    recycler.detach();
}

MysqlConnectionPool::~MysqlConnectionPool(){
    //释放队列李管理的MySQL连接资源
    while(!connectionQueue_.empty()){
        MysqlConnection* conn = connectionQueue_.front();
        connectionQueue_.pop();
        delete conn;
        currentSize_--;
    }
}

//解析JSON配置文件
bool MysqlConnectionPool::parseJsonFile(std::string filePath_) {
    std::ifstream file(filePath_.c_str());
    json conf = json::parse(file);

    ip_ = conf["ip"];
    user_ = conf["userName"];
    passwd_ = conf["password"];
    dbname_ = conf["dbName"];
    port_ = conf["port"];
    minSize_ = conf["minSize"];
    maxSize_ = conf["maxSize"];
    maxIdleTime_ = conf["maxIdleTime"];
    timeout_ = conf["timeout"];
    return true;
}

void MysqlConnectionPool::produceConnection() {
    while(true){
        std::unique_lock<std::mutex> lock(mutex_);
        while(connectionQueue_.size() >= minSize_){
            //还有可用连接则不创建
            cond_.wait(lock);//1.释放锁，2.进入等待状态，3.重新获得锁
        }
        while (currentSize_ < maxSize_){
            addConnection();
            currentSize_++;
            cond_.notify_all();
        }
    }
}

void MysqlConnectionPool::recycleConnection() {
    while(true){
        //作周期性的执行，500ms检查一次
        std::this_thread::sleep_for((std::chrono::milliseconds(500)));

        std::lock_guard<std::mutex> lock(mutex_);
        while(connectionQueue_.size() > minSize_){
            MysqlConnection* conn = connectionQueue_.front();
            if(conn->getAliveTime() >= maxIdleTime_){
                connectionQueue_.pop();
                delete conn;
                currentSize_--;
            }else{
                break;
            }
        }
    }
}

void MysqlConnectionPool::addConnection() {
    MysqlConnection* conn = new MysqlConnection();
    conn->connect(user_, passwd_, dbname_, ip_, port_);
    conn->refreshAliveTime();
    connectionQueue_.push(conn);
}

std::shared_ptr<MysqlConnection> MysqlConnectionPool::getConnection() {
    std::unique_lock<std::mutex> lock(mutex_);
    if(connectionQueue_.empty()){
        while (connectionQueue_.empty()){
            if(std::cv_status::timeout == cond_.wait_for(lock, std::chrono::milliseconds(timeout_))){
                if(connectionQueue_.empty()){
                    continue;
                }
            }
        }
    }
    //有可用的连接

    //共享智能指针并规定删除器，实现放回连接池
    std::shared_ptr<MysqlConnection> connptr(connectionQueue_.front(), [this](MysqlConnection* conn){
        std::lock_guard<std::mutex> lock(mutex_);
        conn->refreshAliveTime();
        connectionQueue_.push(conn);
    });
    connectionQueue_.pop();
    cond_.notify_all();
    return connptr;
}