//
// Created by lq on 2024/6/5.
//
#include "LogFile.h"

LogFile::LogFile(const std::string &basename, off_t rollSize, int flushInterval, int checkEveryN) :
basename_(basename),
rollSize_(rollSize),
flushINterval_(flushInterval),
checkEveryN_(checkEveryN),
count_(0),
mutex_(new std::mutex),
startOfPeriod_(0),
lastRoll_(0),
lastFlush_(0){
    rollFile();
}

LogFile::~LogFile() = default;

void LogFile::append(const char *data, int len) {
    if(mutex_){
        std::lock_guard<std::mutex> lock(*mutex_);
        appendUnlock(data, len);
    }else{
        appendUnlock(data, len);
    }

}

void LogFile::appendUnlock(const char *data, int len) {
    file_->append(data, len);
    if(file_->writtenBytes() > rollSize_){
        rollFile();
    }else{
        ++count_;//写入同一个日志文件的次数
        if(count_ >= checkEveryN_){
            count_ = 0;
            time_t now = ::time(NULL);
            time_t thisPeriod = now / kRollPerSeconds_ * kRollPerSeconds_;
            if(thisPeriod != startOfPeriod_){//如果今天没有创建日志
                rollFile();
            }else if(now - lastFlush_ > flushINterval_){//间隔时间超过flushINterval_才更新flush的时间
                lastFlush_ = now;//更新flush的时间
                file_->flush();
            }
        }
    }
}

void LogFile::flush() {
    if(mutex_){
        std::lock_guard<std::mutex> lock(*mutex_);
        file_->flush();
    }else{
        file_->flush();
    }
}

bool LogFile::rollFile() {
    time_t now = 0;
    std::string filename = getLogFilename(basename_, &now);
    time_t start = now / kRollPerSeconds_ * kRollPerSeconds_;//获得当天零点对应的秒数

    if(now > lastRoll_){
        lastRoll_ = now;
        lastFlush_ = now;
        startOfPeriod_ = start;
        file_.reset(new FileUtil(filename));
        return true;
    }
    return false;
}

std::string LogFile::getLogFilename(const std::string &basename, time_t *now) {
    std::string filename;
    filename.reserve(basename.size() + 64);
    filename = basename;

    char timebuff[32];
    struct tm tm;
    *now = time(NULL);
    localtime_r(now, &tm);
    strftime(timebuff, sizeof timebuff, ".%Y%m%d-%H%M%S", &tm);
    filename += timebuff;
    filename += ".log";
    return filename;
}