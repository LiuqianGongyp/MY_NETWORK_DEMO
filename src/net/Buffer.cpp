//
// Created by lq on 2024/6/24.
//

#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

#include "Buffer.h"
#include "Logging.h"

const char Buffer::kCRLF[] = "\r\n";
/**
 * 从fd上读取数据 Poller工作在LT模式
 * 由于缓冲区大小是一定的，但从fd上读数据的时候不知道tcp数据的大小
 * 从socket读到缓冲区的方法是使用readv先读至buffer_，如果空间不够，会读到栈上65536个字节大小的空间，
 * 然后以append的方式追加入buffer_
 */
ssize_t Buffer::readfd(int fd, int *saveErrno) {
    char extrabuf[65536] = {0};//栈额外空间，用于从套接字往出读，空间不够时，暂存数据
    struct iovec vec[2];
    const size_t writable = writableBytes();

    //第一块缓冲区，指向可写空间
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;

    //第二块缓冲区，指向栈空间
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf);

    //如果缓冲区比额外空间还大，就只用缓冲区，不然则都用
    const int iovcnt = (writable < sizeof(extrabuf)) ? 2 : 1;

    /**
     * ssize_t readv(int fd, const iovec *iov, int iovcnt);
     * 散布读
     * iovcnt 表示缓冲区的个数
     */
    const ssize_t n = ::readv(fd, vec, iovcnt);

    if(n < 0){
        *saveErrno = errno;
    }else if(n <= writable){
        writerIndex_ += n;
    }else{
        writerIndex_ = buffer_.size();
        append(extrabuf, n - writable);
    }
    return n;
}

ssize_t Buffer::writeFd(int fd, int *saveErrno) {//将缓冲区中的可读数据写入fd
    ssize_t n = ::write(fd, peek(), readableBytes());
    if(n < 0){
        *saveErrno = errno;
    }
    return n;
}