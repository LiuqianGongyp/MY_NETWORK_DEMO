//
// Created by lq on 2024/4/29.
//
#include "Timestamp.h"
Timestamp Timestamp::now() {
    struct timeval tv;
    // 获取微秒和秒
    gettimeofday(&tv, NULL);
    int64_t seconds = tv.tv_sec;
    return Timestamp(seconds * kMicrosecondsPerSecond + tv.tv_usec);
}

std::string Timestamp::toFormattedString(bool showMicroseconds) const {
    char buf[64] = {0};
    time_t seconds = static_cast<time_t>(microSecondsSinceEpoch_ / kMicrosecondsPerSecond);
    //localtime函数将秒数格式化为日历时间
    tm *tm_time = localtime(&seconds);
    if(showMicroseconds){
        int microseconds = static_cast<int>(microSecondsSinceEpoch_ % kMicrosecondsPerSecond);
        snprintf(buf, sizeof(buf), "%4d/%02d/%02d %02d:%02d:%02d.%06d",
                tm_time->tm_year + 1900,
                tm_time->tm_mon + 1,
                tm_time->tm_mday,
                tm_time->tm_hour,
                tm_time->tm_min,
                tm_time->tm_sec,
                microseconds);
    } else{
        snprintf(buf, sizeof(buf), "%4d/%02d/%02d %02d:%02d:%02d",
                 tm_time->tm_year + 1900,
                 tm_time->tm_mon + 1,
                 tm_time->tm_mday,
                 tm_time->tm_hour,
                 tm_time->tm_min,
                 tm_time->tm_sec);
    }
    return buf;
}
