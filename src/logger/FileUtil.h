//
// Created by lq on 2024/6/5.
//

#ifndef TINY_NET_WORK_FILEUTIL_H
#define TINY_NET_WORK_FILEUTIL_H
#include <stdio.h>
#include <string>

class FileUtil{//把给定的数据写入到指定的文件中
public:
    explicit FileUtil(std::string & filename);
    ~FileUtil();
    void append(const char* data, size_t len);
    void flush();//当调用flush函数或者write时缓冲区写满，会立刻写入文件
    off_t writtenBytes() const { return writtenBytes_; }
private:
    size_t write(const char* data, size_t len);

    FILE* fp_;
    char buffer_[64 * 1024];//开辟64kb的缓冲区是为了避免系统提供的缓冲区小且日志多而密集时导致频繁向文件中写入数据
    off_t writtenBytes_;//off_t表示文件的偏移量，常为32位整数
};
#endif //TINY_NET_WORK_FILEUTIL_H
