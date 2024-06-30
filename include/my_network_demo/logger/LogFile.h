//
// Created by lq on 2024/6/4.
//

#ifndef TINY_NET_WORK_LOGFILE_H
#define TINY_NET_WORK_LOGFILE_H
#include "FileUtil.h"

#include <mutex>
#include <memory>

class LogFile{
public:
    LogFile(const std::string& basename, off_t rollSize, int flushInterval = 3, int checkEveryN = 1024);
    ~LogFile();

    void append(const char* data, int len);//同步加锁，异步不用
    void flush();//同步加锁，异步不用
    //具体来说，产生日志的线程有很多，他们在写入用户开辟的大缓冲区的时候需要加锁，但这是写入缓冲区，不是写入文件，
    //也就是说，异步日志开启的情况下，写文件的永远是异步日志的后台线程，不存在竞争不需要加锁

    bool rollFile();//滚动日志
private:
    static std::string getLogFilename(const std::string& basename, time_t* now);
    void appendUnlock(const char* data, int len);
    const std::string basename_;
    const off_t rollSize_; //滚动日志的阈值
    const int flushINterval_; //刷新时间间隔
    const int checkEveryN_; //记录往buffer_添加数据的次数，

    int count_;//

    std::unique_ptr<std::mutex> mutex_;
    time_t startOfPeriod_; //记录最后一个日志的创建日期
    time_t lastRoll_;//最后一次滚动的时间
    time_t lastFlush_;//最后一次刷新的时间
    std::unique_ptr<FileUtil> file_;

    const static int kRollPerSeconds_ = 60 * 60 * 24;
};
#endif //TINY_NET_WORK_LOGFILE_H
