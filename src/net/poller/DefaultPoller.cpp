//
// Created by lq on 2024/6/26.
//
#include "Poller.h"
#include "EPollPoller.h"

#include <stdlib.h>

Poller* Poller::newDefultPoller(EventLoop *Loop) {
    if(::getenv("MUDUO_USE_POLL")){//获取名为 MUDUO_USE_POLL 的环境变量的值
        return nullptr;//生成poll实例
    }else{
        return new EPollPoller(Loop);//生成epoll实例
    }
}

