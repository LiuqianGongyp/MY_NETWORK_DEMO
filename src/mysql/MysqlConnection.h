//
// Created by lq on 2024/6/29.
//

#ifndef MY_NETWORK_DEMO_MYSQLCONNECTION_H
#define MY_NETWORK_DEMO_MYSQLCONNECTION_H

#include <iostream>
#include <mysql/mysql.h>
#include <chrono>


class MysqlConnection {
public:
    MysqlConnection();
    ~MysqlConnection();
    bool connect(const std::string &user, const std::string passwd, const std::string dbName, const std::string& ip, const unsigned int & port = 3306);
    //更新数据库：包括insert update delete操作
    bool update(const std::string & sql);
    bool query(const std::string & sql);
    //遍历查询得到的结果集
    bool next();
    //得到row_中的字段值
    std::string value(int index);
    //事务操作
    bool transaction();
    //提交事物
    bool commit();
    //事务回滚
    bool rollbock();
    //刷新起始的空闲时间点
    void refreshAliveTime();
    //计算连接存活的总时长
    long long getAliveTime();
private:
    void freeResult();

    MYSQL* conn_ = nullptr;
    MYSQL_RES* result_ = nullptr;
    MYSQL_ROW row_ = nullptr;

    std::chrono::steady_clock::time_point m_alivetime;
};


#endif //MY_NETWORK_DEMO_MYSQLCONNECTION_H
