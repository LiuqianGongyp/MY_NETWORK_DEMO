//
// Created by lq on 2024/6/30.
//
/**
 * 避免频繁进行数据库连接和销毁
 */
#ifndef MY_NETWORK_DEMO_MYSQLCONNECTIONPOOL_H
#define MY_NETWORK_DEMO_MYSQLCONNECTIONPOOL_H

#include "MysqlConnection.h"
#include "json.hpp"
using json = nlohmann::json;

#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>

class MysqlConnectionPool {
public:
    static MysqlConnectionPool* getConnectionPool();
    std::shared_ptr<MysqlConnection> getConnection();
    ~MysqlConnectionPool();
private:
    MysqlConnectionPool();
    MysqlConnectionPool(const MysqlConnectionPool& obj) = delete;
    MysqlConnectionPool(const MysqlConnectionPool&& obj) = delete;
    MysqlConnectionPool& operator=(const MysqlConnectionPool& obj) = delete;

    bool parseJsonFile(std::string filePath_);
    void produceConnection();//产生数据库连接
    void recycleConnection();//销毁多余的数据库连接
    void addConnection();

    std::string filePath_ = "conf.json";
    std::string ip_;
    std::string user_;
    std::string passwd_;
    std::string dbname_;
    unsigned short port_;
    int minSize_;
    int maxSize_;
    int maxIdleTime_;
    int timeout_;
    int currentSize_; //connectionQueue_中连接的数目
    std::queue<MysqlConnection*> connectionQueue_;
    std::mutex mutex_;
    std::condition_variable cond_;
};


#endif //MY_NETWORK_DEMO_MYSQLCONNECTIONPOOL_H
