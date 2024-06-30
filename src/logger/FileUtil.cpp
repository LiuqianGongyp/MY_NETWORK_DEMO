//
// Created by lq on 2024/6/5.
//
#include "FileUtil.h"
#include "Logging.h"

FileUtil::FileUtil(std::string &filename) : fp_(::fopen(filename.c_str(), "ae")),
    writtenBytes_(0){
    ::setbuffer(fp_, buffer_, sizeof(buffer_));//设置fd_的缓冲区为本地的buffer_
}

FileUtil::~FileUtil() {
    ::fclose(fp_);
}

void FileUtil::append(const char *data, size_t len) {
    size_t written = 0;//已经写入的数据大小
    while(written != len){
        size_t remain = len - written;//还需写入的大小
        size_t n = write(data + written, remain);
        if(n != remain){
            int err = ferror(fp_);
            if(err){
                fprintf(stderr, "FileUtile::append() failed %s\n", getErrnoMsg(err));
            }
        }
        written += n;
    }
    writtenBytes_ += written;
}

void FileUtil::flush() {
    ::fflush(fp_);
}

size_t FileUtil::write(const char *data, size_t len) {
    /*
     * size_t fwrite_unlocked(const void* buffer, size_t size, size_t count, FILE* stream);
     * buffer:指向数据块的指针
     * size:每块数据的大小，单位是Byte
     * count:数据的个数
     * stream:文件指针
     */
    return ::fwrite(data, 1, len, fp_);
}