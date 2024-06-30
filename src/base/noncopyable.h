//
// Created by lq on 2024/4/24.
//

#ifndef TINY_NET_WORK_NONCOPYABLE_H
#define TINY_NET_WORK_NONCOPYABLE_H
//

class noncopyable
{
public:
    noncopyable(const noncopyable &) = delete;
    noncopyable &operator=(const noncopyable &) = delete;
protected:
    noncopyable() = default;
    ~noncopyable() = default;
};
#endif //TINY_NET_WORK_NONCOPYABLE_H
