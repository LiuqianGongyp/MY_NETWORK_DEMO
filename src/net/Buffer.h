//
// Created by lq on 2024/6/24.
//

#ifndef MY_NETWORK_DEMO_BUFFER_H
#define MY_NETWORK_DEMO_BUFFER_H

#include <vector>
#include <string>
#include <algorithm>
#include <stddef.h>
#include <assert.h>
/**
 * +---------------+------------------+----------------------+
 * |  preoendable  |  readable bytes  |    writable bytes    |
 * |               |    (CONTENT)     |                      |
 * +---------------+------------------+----------------------+
 * |               |  服务端要发送的数据  | 从socket读来数据存放于此|
 * 0     <=   readerIndex   <=    writerIndex      <=      size
 */
//socket处于可读时，需要向buffer中写数据

//prependable 初始大小， readIndex初始位置
//writable 初始大小， writerIndex初始位置
//刚开始 readerIndex和writerIndex位于同一位置

class Buffer {
public:
    static const size_t kCheapPrepend = 8;
    static const size_t kInitialSize = 1024;
    explicit Buffer(size_t initialSize = kInitialSize) :
    buffer_(kCheapPrepend + initialSize), readerIndex_(kCheapPrepend), writerIndex_(kCheapPrepend){

    }
    /*
     * | kCheapPrepend | reder | writer |
     */
    size_t readableBytes() const { return writerIndex_ - readerIndex_; }//可读的大小
    size_t writableBytes() const { return buffer_.size() - writerIndex_; };//可写的大小
    size_t prependableBytes() const { return readerIndex_; }//已读的大小
    const char* peek() const { return begin() + readerIndex_; }//返回缓冲区中可读数据的起始地址

    // 查找buffer中是否有"\r\n", 解析http请求行用到
    const char* findCRLF() const
    {
        const char* crlf = std::search(peek(), beginWrite(), kCRLF, kCRLF+2);
        return crlf == beginWrite() ? NULL : crlf;
    }

    void retrieveUntil(const char* end){
        assert(peek() <= end);
        assert(end <= beginWrite());
        retrieve(end - peek());
    }
    void retrieve(size_t len){
        if(len < readableBytes()){
            readerIndex_ += len;
        }else{
            retrieveAll();
        }
    }
    void retrieveAll(){
        readerIndex_ = kCheapPrepend;
        writerIndex_ = kCheapPrepend;
    }

    //把onMessage函数上报的buffer转化成string类型返回
    std::string retrieveAllAsString(){ return retrieveAllAsString(readableBytes()); }
    std::string retrieveAllAsString(size_t len){
        std::string result(peek(), len);
        retrieve(len);
        return result;
    }
    void enseureWritableBytes(size_t len){
        if(writableBytes() < len){
            makeSpace(len);
        }
    }
    void append(const std::string &str){
        append(str.data(), str.size());
    }
    void append(const char* data, size_t len){
        //添加数据到writable缓冲区中
        enseureWritableBytes(len);
        std::copy(data, data + len, beginWrite());
        writerIndex_ += len;
    }
    char *beginWrite() { return begin() + writerIndex_; }
    const char* beginWrite() const { return begin() + writerIndex_; }

    ssize_t readfd(int fd, int* saveErrno);
    ssize_t writeFd(int fd, int* saveErrno);
private:
    /**
     * 借助vector实现自动分配内存
     */
     char* begin(){
        return &(*buffer_.begin());//获取buffer_的起始地址
     }
     const char* begin() const{
         return &(*buffer_.begin());//先调用begin()获得首个元素的迭代器，然后再解引用得到这个变量，再取地址
     }
     void makeSpace(int len){//调整可写的空间，即存放socket可读数据的空间
         //扩容
         /*     | kCheapPrepend | xxx | reder | writer |
          * 变为 | kCheapPrepend | reder |         len        |
          */
         if(writableBytes() + prependableBytes() < len + kCheapPrepend){
             buffer_.resize(writerIndex_ + len);
         }else{//调整buffer的两个游标
             /**
             * +-----------------------+------------------+----------------------+
             * |       preoendable     |  readable bytes  |    writable bytes    |
             * | kCheapPrepend | p-kC  |    (CONTENT)     |                      |
             * +---------------+-------+------------------+----------------------+
             * |               |       |  服务端要发送的数据  | 从socket读来数据存放于此|
             * 0               8 <= readerIndex   <=    writerIndex      <=      size
             */
             //调整为
             /**
             * +---------------+------------------+------------------------------+
             * |  preoendable  |  readable bytes  |      新的writable bytes       |
             * | kCheapPrepend |    (CONTENT)     | = p-kC + 之前的writable bytes |
             * +---------------+------------------+-----------------------------+
             * |               |  服务端要发送的数据  |    从socket读来数据存放于此     |
             * 0          readerIndex   <=    writerIndex          <=          size
             */
             size_t readable = readableBytes();
             std::copy(begin() + readerIndex_,
                       begin() + writerIndex_,
                       begin() + kCheapPrepend);
             readerIndex_ = kCheapPrepend;
             writerIndex_ = readerIndex_ + readable;
         }
     }
     std::vector<char> buffer_;
     size_t readerIndex_;
     size_t writerIndex_;
     static const char kCRLF[];
};


#endif //MY_NETWORK_DEMO_BUFFER_H
