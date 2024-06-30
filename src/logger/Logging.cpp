//
// Created by lq on 2024/4/29.
//
#include "Logging.h"
#include "../base/CurrentThread.h"

namespace ThreadInfo{
    __thread char t_errnobuf[512];
    __thread char t_time[64];
    __thread time_t t_lastSecond;
};

const char* getErrnoMsg(int savedErrno){
    return strerror_r(savedErrno, ThreadInfo::t_errnobuf, sizeof(ThreadInfo::t_errnobuf));
}

const char* getLevelName[Logger::LogLevel::NUM_LOG_LEVELS]{
    "TRACE",
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR",
    "FATAL",
};

Logger::LogLevel initLogLevel(){
    return Logger::INFO;
}

Logger::LogLevel g_logLevel = initLogLevel();

static void defaultOutput(const char * data, int len){
    fwrite(data, len, sizeof(char), stdout);
}

static void defaultFlush(){
    fflush(stdout);
}

Logger::OutputFunc g_output = defaultOutput;
Logger::FlushFunc g_flush = defaultFlush;

Logger::Impl::Impl(Logger::Impl::LogLevel level, int savedErrno, const char* file, int line)
: time_(Timestamp::now()), stream_(), level_(level), line_(line), basename_(file){
    formatTime();
    stream_ << GeneralTemplate(getLevelName[level], 6);
    if(savedErrno != 0){
        stream_ << getErrnoMsg(savedErrno) << " (errno=" << savedErrno << ")";
    }
}

void Logger::Impl::formatTime() {
    Timestamp now = Timestamp::now();
    time_t seconds = static_cast<time_t>(now.microSecondsSinceEpoch() / Timestamp::kMicrosecondsPerSecond);
    int microsecondes = static_cast<int>(now.microSecondsSinceEpoch() % Timestamp::kMicrosecondsPerSecond);

    struct tm *tm_time = localtime(&seconds);
    snprintf(ThreadInfo::t_time, sizeof(ThreadInfo::t_time), "%4d/%02d/%02d %02d:%02d:%02d",
             tm_time->tm_year,
             tm_time->tm_mon,
             tm_time->tm_mday,
             tm_time->tm_hour,
             tm_time->tm_min,
             tm_time->tm_sec);
    ThreadInfo::t_lastSecond = seconds;

    char buf[32] = {0};
    snprintf(buf, sizeof(buf), "%06d", microsecondes);

    stream_ << GeneralTemplate(ThreadInfo::t_time, 17) << GeneralTemplate(buf, 7);
}

void Logger::Impl::finish() {
    stream_ << "-" << GeneralTemplate(basename_.data_, basename_.size_) << ":" << line_ << "\n";
}

Logger::Logger(const char *file, int line) :
        impl_(INFO, 0, file, line){

}

Logger::Logger(const char *file, int line, Logger::LogLevel level) :
        impl_(level, 0, file, line){

}

Logger::Logger(const char *file, int line, Logger::LogLevel level, const char *func) :
        impl_(level, 0, file, line){
    impl_.stream_ << func << ' ';
}

Logger::~Logger(){
    impl_.finish();
    const LogStream::Buffer& buf(stream().buffer());
    g_output(buf.data(), buf.length());
    if(impl_.level_ == FATAL){
        g_flush();
        abort();
    }
}

void Logger::setLogLevel(Logger::LogLevel level) {
    g_logLevel = level;
}

void Logger::setOutput(OutputFunc out) {
    g_output = out;
}

void Logger::setFlush(FlushFunc flush) {
    g_flush = flush;
}