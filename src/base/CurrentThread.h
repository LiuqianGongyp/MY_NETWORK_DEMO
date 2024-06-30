//
// Created by lq on 2024/4/29.
//

#ifndef TINY_NET_WORK_CURRENTTHREAD_H
#define TINY_NET_WORK_CURRENTTHREAD_H
#include <unistd.h>
#include <sys/syscall.h>
namespace CurrentThread{
    extern __thread int t_cachedTid;//这句话是声明，其中extern表示要在其他位置找t_cachedTid的值，__thread线程局部存储
    void cachedTid();
    inline int tid(){
        if(__builtin_expect(t_cachedTid == 0, 0)){//分支预测优化，期望表达式t_cachedTid == 0的结果为第二个参数常数0，__builtin_expect的返回值为第一个参数表达式
            //第二个参数为0表示执行if分支的可能性小
            cachedTid();
            return t_cachedTid;
        }
    }
}

#endif //TINY_NET_WORK_CURRENTTHREAD_H
