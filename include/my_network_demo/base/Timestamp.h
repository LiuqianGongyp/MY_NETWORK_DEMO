//
// Created by lq on 2024/4/24.
//

#ifndef TINY_NET_WORK_TIMESTAMP_H
#define TINY_NET_WORK_TIMESTAMP_H
#include <iostream>
#include <string>
#include <sys/time.h>

class Timestamp{
public:
    Timestamp() : microSecondsSinceEpoch_(0) {}
    explicit Timestamp(int64_t microSecondsSinceEpoch) : microSecondsSinceEpoch_(microSecondsSinceEpoch){}
    static Timestamp now();
    std::string toString() const;//用std：：string形式返回[millisec].[microsec]
    std::string toFormattedString(bool showMicroseconds = false) const;

    //判断时间戳是否有效
    bool valid() const { return microSecondsSinceEpoch_ > 0; }

    int64_t microSecondsSinceEpoch() const {return microSecondsSinceEpoch_;}
    time_t secondsSinceEpoch() const {
        return static_cast<time_t>(microSecondsSinceEpoch_ / kMicrosecondsPerSecond);
    }
    //失效的时间戳，返回一个值为0的时间戳
    static Timestamp invalid(){
        return Timestamp();
    }
    static const int kMicrosecondsPerSecond = 1000 * 1000;//1s = 1000 * 1000微秒
private:
    //表示时间戳的微秒数（自epoch开始经历的微秒数）
    int64_t microSecondsSinceEpoch_;
};
/**
 * 定时器需要比较时间戳，因此需要重载运算符
 */
 inline bool operator<(Timestamp lhs, Timestamp rhs){
    return lhs.microSecondsSinceEpoch() < rhs.microSecondsSinceEpoch();
 }
 inline bool operator==(Timestamp lhs, Timestamp rhs){
     return lhs.microSecondsSinceEpoch() == rhs.microSecondsSinceEpoch();
 }
 inline Timestamp addTime(Timestamp timestamp, double seconds){
     //将延时的秒数转换为微妙
     int64_t delta = static_cast<int64_t>(seconds * Timestamp::kMicrosecondsPerSecond);
     //返回新增后的时间戳
     return Timestamp(timestamp.microSecondsSinceEpoch() + delta);
 }
#endif //TINY_NET_WORK_TIMESTAMP_H
