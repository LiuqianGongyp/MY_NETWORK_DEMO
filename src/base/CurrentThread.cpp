//
// Created by lq on 2024/4/29.
//
#include "CurrentThread.h"
namespace CurrentThread{
    __thread int t_cachedTid = 0;//__thread线程局部存储

    void cachedTid(){
        if(t_cachedTid == 0){
            t_cachedTid = static_cast<pid_t>(::syscall(SYS_gettid));
        }
    }
}