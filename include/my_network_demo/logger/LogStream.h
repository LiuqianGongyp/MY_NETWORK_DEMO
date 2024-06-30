//
// Created by lq on 2024/4/24.
//

#ifndef TINY_NET_WORK_LOGSTREAM_H
#define TINY_NET_WORK_LOGSTREAM_H
#include "FixedBuffer.h"
#include "noncopyable.h"
/*
 * 用于将LogStream<<时间，SourceFile
 */
class GeneralTemplate : noncopyable{
public:
    GeneralTemplate() : data_(nullptr), len_(0){}
    GeneralTemplate(const char* data, int len) : data_(data), len_(len){}
    const char * data_;
    int len_;
};
class LogStream : noncopyable
{
public:
    using Buffer = FixedBuffer<kSmallBuffer>;

    //向buffer_添加数据
    void append(const char* data, int len) {buffer_.append(data, len);}
    const Buffer& buffer() const {return buffer_;}
    void resetBuffer(){ buffer_.reset();}

    LogStream& operator<<(short);
    LogStream& operator<<(unsigned short);
    LogStream& operator<<(int);
    LogStream& operator<<(unsigned int);
    LogStream& operator<<(long);
    LogStream& operator<<(unsigned long);
    LogStream& operator<<(long long);
    LogStream& operator<<(unsigned long long);
    LogStream& operator<<(float v);
    LogStream& operator<<(double v);
    LogStream& operator<<(char c);
    LogStream& operator<<(const void* p);
    LogStream& operator<<(const char* str);
    LogStream& operator<<(const unsigned char* str);
    LogStream& operator<<(const std::string& str);
    LogStream& operator<<(const Buffer& buf);
    LogStream& operator<<(const GeneralTemplate& g);
private:
    static const int kMaxNumericSize = 48;//数字转换为字符串后的最大长度
    Buffer buffer_;
    //对于整型需要特殊处理成字符串，因为任意类型最终是要转化成字符串并添加到buffer_中
    template<typename T>
    void formatInteger(T);
};
#endif //TINY_NET_WORK_LOGSTREAM_H
