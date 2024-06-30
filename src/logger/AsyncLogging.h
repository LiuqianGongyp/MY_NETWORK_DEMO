//
// Created by lq on 2024/6/4.
//

#ifndef TINY_NET_WORK_ASYNCLOGGING_H
#define TINY_NET_WORK_ASYNCLOGGING_H
#include "noncopyable.h"
#include "Thread.h"
#include "Acceptor.h"
#include "FixedBuffer.h"
#include "LogStream.h"
#include "LogFile.h"

#include <vector>
#include <memory>
#include <mutex>
#include <condition_variable>
class AsyncLogging{
public:
    AsyncLogging(const std::string & basename,
                 off_t rollSize,
                 int flushInterval = 3);
    ~AsyncLogging(){
        if(running_){
            stop();
        }
    }

    void append(const char* logline, int len);

    void start(){
        running_ = true;
        thread_.start();
    }

    void stop(){
        running_ = false;
        cond_.notify_one();
        thread_.join();
    }
private:
    using Buffer = FixedBuffer<kLargeBuffer>;
    using BufferVector = std::vector<std::unique_ptr<Buffer>>;//独占指针
    using BufferPtr = BufferVector::value_type;

    void threadFunc();

    const int fulshINterval_;
    std::atomic<bool> running_;
    const std::string basename_;
    const off_t rollSize_;
    Thread thread_;
    std::mutex mutex_;
    std::condition_variable cond_;

    BufferPtr currentBuffer_;
    BufferPtr nextBuffer_;
    BufferVector buffers_;
};
#endif //TINY_NET_WORK_ASYNCLOGGING_H
