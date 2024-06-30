//
// Created by lq on 2024/4/24.
//

#ifndef TINY_NET_WORK_FIXEDBUFFER_H
#define TINY_NET_WORK_FIXEDBUFFER_H
#include "noncopyable.h"
#include <assert.h>
#include <string.h>
#include <strings.h>
#include <string>

const int kSmallBuffer = 4000;
const int kLargeBuffer = 4000 * 1024;
template <int SIZE>
class FixedBuffer : noncopyable
{
public:
    FixedBuffer() : cur_(data_){}
    void append(const char * buf, size_t len)
    {
        if(static_cast<size_t>(avail()) > len) {
            memcpy(cur_, buf, len);
            cur_ += len;
        }
    }
    const char * data() const {return data_;}
    int length() const {return static_cast<int>(cur_ - data_);}
    char* current() {return  cur_;}
    int avail() const {return static_cast<int>(end() - cur_);}
    void add(size_t len) {cur_ += len;}
    void reset() { cur_ = data_;}
    void bzero() {::bzero(data_, sizeof(data_));}//将内存块清零
    std::string toString() const {return std::string(data_, length());}
private:
    const char* end() const {return data_ + sizeof(data_);}
    char data_[SIZE];
    char * cur_;
};
#endif //TINY_NET_WORK_FIXEDBUFFER_H
