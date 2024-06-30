//
// Created by lq on 2024/6/4.
//
#include "AsyncLogging.h"
#include "Timestamp.h"

#include <stdio.h>

AsyncLogging::AsyncLogging(const std::string &basename, off_t rollSize, int flushInterval)
: fulshINterval_(flushInterval),
running_(false),
basename_(basename),
rollSize_(rollSize),
thread_(std::bind(&AsyncLogging::threadFunc, this), "Logging"),
mutex_(),
cond_(),
currentBuffer_(new Buffer),//有4M大小，为了多积累一些日志信息，一并交给后端写入文件，可从append中发现这一点
nextBuffer_(new Buffer),
buffers_(){
    currentBuffer_->bzero();
    nextBuffer_->bzero();
    buffers_.reserve(16);
}

void AsyncLogging::append(const char *logline, int len) {//只是将信息写入缓冲区中，未写入文件
    //先将日志信息存储到用户自己开辟的大缓冲区，
    std::lock_guard<std::mutex> lock(mutex_);
    if(currentBuffer_->avail() > len){
        currentBuffer_->append(logline, len);
    }else{//当前缓冲区空间不够
        buffers_.push_back(std::move(currentBuffer_));//将写满的currentBuffer_放入buffers_
        //currentBuffer_是独占指针，使用std::move后将成为nullptr
        if(nextBuffer_){//nextBuffer_还没开始使用
            currentBuffer_ = std::move(nextBuffer_);//将nextBuffer_给currentBuffer_，nextBuffer_成为nullptr
        }else {
            //备用缓冲区不足，重新分配缓冲区
            currentBuffer_.reset(new Buffer);
        }
        currentBuffer_->append(logline, len);
        cond_.notify_one();//唤醒写入磁盘的后端程序
    }
}

void  AsyncLogging::threadFunc() {
    LogFile output(basename_, rollSize_, false);//output有写入磁盘的接口
    //异步日志后端的日志信息只来自 buffersToWrite，因此不用考虑线程安全
    //后端缓冲区
    BufferPtr newBuffer1(new Buffer);
    BufferPtr newBuffer2(new Buffer);
    newBuffer1->bzero();
    newBuffer2->bzero();

    BufferVector buffersToWrite;
    buffersToWrite.reserve(16);//缓冲区数组设为16个，用于和前端缓冲区数组进行交换

    while(running_){
        {
            //互斥锁保护
            std::unique_lock<std::mutex> lock(mutex_);
            if(!currentBuffer_) {//没找到currentBuffer_为空的原因，强行矫正
                currentBuffer_.reset(new Buffer);
            }
            if(buffers_.empty()){
                cond_.wait_for(lock, std::chrono::seconds(3));//等待三秒结束阻塞
            }
            buffers_.push_back(std::move(currentBuffer_));//将正在使用的缓冲区放入缓冲区数组（包括没写完的情况下的）
            currentBuffer_ = std::move(newBuffer1);//归还正在使用的缓冲区
            buffersToWrite.swap(buffers_);//前后端缓冲区交换
            if(!nextBuffer_){
                nextBuffer_ = std::move(newBuffer2);
            }
        }

        //遍历所有的buffer，将其写入文件
        for(const auto & buffer : buffersToWrite){
            output.append(buffer->data(), buffer->length());
        }

        //只保留两个缓冲区
        if(buffersToWrite.size() != 2){
            buffersToWrite.resize(2);
        }

        //归还缓冲区1
        if(!newBuffer1){
            newBuffer1 = std::move(buffersToWrite.back());
            buffersToWrite.pop_back();
            newBuffer1.reset();
        }
        //归还缓冲区2
        if(!newBuffer2){
            newBuffer2 = std::move(buffersToWrite.back());
            buffersToWrite.pop_back();
            newBuffer2.reset();
        }

        buffersToWrite.clear();//清空后端缓冲区队列
        output.flush();
    }
    output.flush();
}