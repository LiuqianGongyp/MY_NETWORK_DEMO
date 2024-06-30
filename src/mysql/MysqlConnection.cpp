//
// Created by lq on 2024/6/29.
//

#include "MysqlConnection.h"

//初始化数据库连接

MysqlConnection::MysqlConnection() {
    //分配或初始化与mysql_real_connect()相适应的MYSQL对象
    //如果mysql是NULL指针，该函数将分配、初始化、并返回新对象
    conn_ = mysql_init(nullptr);
    //设置字符编码，可以存储中文
    mysql_set_character_set(conn_, "utf8");
}

MysqlConnection::~MysqlConnection(){
    if(conn_ != nullptr){
        mysql_close(conn_);
    }
    //释放结果集
    freeResult();
}

bool MysqlConnection::connect(const std::string &user, const std::string passwd, const std::string dbName,
                              const std::string &ip, const unsigned int &port) {
    MYSQL* ptr = mysql_real_connect(conn_, ip.c_str(), user.c_str(), passwd.c_str(), dbName.c_str(), port, nullptr, 0);
    return ptr != nullptr;
}

bool MysqlConnection::update(const std::string &sql) {
    if(mysql_query(conn_, sql.c_str())){
        return false;
    }
    return true;
}

bool MysqlConnection::query(const std::string &sql) {
    freeResult();
    if(mysql_query(conn_, sql.c_str())){
        return false;
    }
    result_ = mysql_store_result(conn_);
    return true;
}

//
bool MysqlConnection::next() {
    if(result_ != nullptr){
        row_ = mysql_fetch_row(result_);
        if(row_ != nullptr){//读完一行还有下一行才返回true
            return true;
        }
    }
    return false;
}

std::string MysqlConnection::value(int index) {
    int colCount = mysql_num_fields(result_);//结果集中的字段数量
    if(index >= colCount || index < 0){
        //索引非法
        return std::string();
    }
    //若存储的是二进制字符串（其中含有‘\0’），那么我们无法获得完整字符串，需要获取字符串头指针和字符串长度
    char* val = row_[index];
    unsigned long length = mysql_fetch_lengths(result_)[index];
    return std::string(val, length);
}

bool MysqlConnection::transaction() {
    //true 自动提交
    //false 手动提交
    return mysql_autocommit(conn_, false);
}

bool MysqlConnection::commit() {
    return mysql_commit(conn_);
}

bool MysqlConnection::rollbock() {
    return mysql_rollback(conn_);
}

void MysqlConnection::refreshAliveTime() {
    m_alivetime = std::chrono::steady_clock::now();
}

//计算连接存活的总时长
long long MysqlConnection::getAliveTime() {
    std::chrono::nanoseconds res = std::chrono::steady_clock::now() - m_alivetime;
    //纳秒转毫秒，高精度转低精度转换需要duration_cast
    std::chrono::milliseconds millsec = std::chrono::duration_cast<std::chrono::milliseconds>(res);
    return millsec.count();
}

void MysqlConnection::freeResult() {
    if(result_){
        mysql_free_result(result_);
        result_ = nullptr;
    }
}