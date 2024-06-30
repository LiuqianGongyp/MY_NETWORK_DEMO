//
// Created by lq on 2024/6/30.
//
/**
 * 测试：
 *      用时/纳秒 |   使用连接池    |    不使用连接池    |
 *     ---------+---------------+-----------------+-
 *       单线程  |   557603291   |   10065760115   |
 *     --------+---------------+-----------------+-
 *       多线程 |   347468596   |   10064923011   |
 *     -------+---------------+-----------------+-
 */
#include "MysqlConnectionPool.h"
#include "MysqlConnection.h"
#include <thread>

void op1(int begin, int end){
    for(int i = begin; i < end; ++i){
        MysqlConnection conn;
        conn.connect("root", "123456", "test", "127.0.0.1");
        char sql[1024] = {0};
        snprintf(sql, sizeof(sql), "insert into user values(%d, 'li si', 'lisi%d@163.com')", i, i);
        conn.update(sql);
    }
}
void op2(MysqlConnectionPool* pool, int begin, int end){
    for(int i = begin; i < end; ++i){
        std::shared_ptr<MysqlConnection> conn = pool->getConnection();
        char sql[1024] = {0};
        snprintf(sql, sizeof(sql), "insert into user values(%d, 'li si', 'lisi%d@163.com')", i, i);
        conn->update(sql);
    }
}
//单线程情况
void test1(){
#if 1
    //非连接池 单线程，用时：10065760115 纳秒，10065 毫秒
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    op1(0, 5000);
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    auto length = end - begin;
    std::cout << "非连接池，单线程，用时：" << length.count() << " 纳秒，" << length.count() / 1000000 << " 毫秒" << std::endl;
#else
    //连接池 连接池，单线程，用时：557603291 纳秒，557 毫秒
    MysqlConnectionPool* pool = MysqlConnectionPool::getConnectionPool();
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    op2(pool, 0, 5000);
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    auto length = end - begin;
    std::cout << "连接池，单线程，用时：" << length.count() << " 纳秒，" << length.count() / 1000000 << " 毫秒" << std::endl;

#endif
}

// 多线程
void test2(){
#if 1
    //非连接池，多线程，用时：10064923011 纳秒，10064 毫秒
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    std::thread t1(op1, 0, 1000);
    std::thread t2(op1, 1000, 2000);
    std::thread t3(op1, 2000, 3000);
    std::thread t4(op1, 3000, 4000);
    std::thread t5(op1, 4000, 5000);
    t1.join();
    t2.join();
    t3.join();
    t4.join();
    t5.join();
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    auto length = end - begin;
    std::cout << "非连接池，多线程，用时：" << length.count() << " 纳秒，" << length.count() / 1000000 << " 毫秒" << std::endl;
#else
    //连接池，多线程，用时：347468596 纳秒，347 毫秒
    MysqlConnectionPool* pool = MysqlConnectionPool::getConnectionPool();
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    std::thread t1(op2, pool, 0, 1000);
    std::thread t2(op2, pool, 1000, 2000);
    std::thread t3(op2, pool, 2000, 3000);
    std::thread t4(op2, pool, 3000, 4000);
    std::thread t5(op2, pool, 4000, 5000);
    t1.join();
    t2.join();
    t3.join();
    t4.join();
    t5.join();
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    auto length = end - begin;
    std::cout << "连接池，多线程，用时：" << length.count() << " 纳秒，" << length.count() / 1000000 << " 毫秒" << std::endl;

#endif
}

int query(){
    MysqlConnection conn;
    conn.connect("root", "123456", "test", "127.0.0.1");
    std::string sql = "insert into user (username, email) values ('zhang san', 'zs3546@163.com');";
    bool flag = conn.update(sql);
    std::cout << "flag value: " << flag << std::endl;
    sql = "select * from user";
    conn.query(sql);
    while(conn.next()){
        std::cout << conn.value(0) << ", "
        << conn.value(1) << ", "
        << conn.value(2) << ", "
        << conn.value(3) << std::endl;
    }
}
int main(){
    //query();

    //test1();

    test2();
    return 0;
}