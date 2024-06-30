//
// Created by lq on 2024/6/4.
//

#ifndef TINY_NET_WORK_THREAD_H
#define TINY_NET_WORK_THREAD_H
#include <thread>
#include <functional>
#include <memory>
#include <string>
#include <atomic>
#include "noncopyable.h"
class Thread : noncopyable{
public:
    using ThreadFunc = std::function<void()>;
    explicit Thread(ThreadFunc, const std::string &name = std::string());
    ~Thread();

    void start();
    void join();//等待线程

    bool started() const { return started_; }
    pid_t tid() const { return tid_; }
    const std::string& name() const { return name_; }

    static int numCreated() { return numCreated_; }
private:
    void setDefaultName();//设置线程名

    bool started_;//是否启动线程
    bool joined_;//是否等待该线程
    std::shared_ptr<std::thread> thread_;
    pid_t tid_;//线程tid
    ThreadFunc func_;
    std::string name_;//线程名
    static  std::atomic_int32_t numCreated_;//线程索引
};
#endif //TINY_NET_WORK_THREAD_H
