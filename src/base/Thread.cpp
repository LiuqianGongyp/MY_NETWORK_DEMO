//
// Created by lq on 2024/6/5.
//
#include <semaphore.h>
#include "Thread.h"
#include "CurrentThread.h"

std::atomic_int32_t  Thread::numCreated_(0);//atomic_int32_t是原子变量，提供线程安全
/**
 * TODO: error
 * 默认参数在定义和声明中只能出现一次，不能声明和定义都有默认参数
 *
 */
 Thread::Thread(ThreadFunc func, const std::string &name):
     started_(false), //最初未开始运行
     joined_(false),//未设置等待线程
     tid_(0),
     func_(std::move(func)),
     name_(name)
 {
     setDefaultName();//设置默认姓名和编号
 }

 Thread::~Thread(){
     if(started_ && !joined_){
         thread_->detach();//分离线程
     }
 }

 void Thread::start(){
     started_ = true;
     sem_t sem;
     sem_init(&sem, false, 0);

     thread_ = std::shared_ptr<std::thread>(new std::thread([&](){
         tid_ = CurrentThread::tid();//获取线程pid
         sem_post(&sem);//V操作（PV操作，P操作——申请资源，V操作释放资源）
         func_();
     }));
     /**
      * 必须等待获取上面新创建的线程的tid
      * 未获取到信息则不会执行sem_post，所以会被阻塞住
      * 如果不使用信号量操作，则别的线程访问tid时候，可能上面的线程还没有获取到tid
      */
     sem_wait(&sem);
 }

 void Thread::join(){
     joined_ = true;
     //等待线程执行完毕
     thread_->join();
 }

 void Thread::setDefaultName() {
     int num = ++numCreated_;
     if(name_.empty()){//
         char buf[32] = {0};
         snprintf(buf, sizeof(buf), "Thread%d", num);
         name_ = buf;
     }
 }