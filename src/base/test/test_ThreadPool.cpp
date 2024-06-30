//
// Created by lq on 2024/6/11.
//
#include "ThreadPool.h"
#include "Logging.h"
#include "CurrentThread.h"
#include <stdio.h>
#include <unistd.h>
#include <functional>

int count = 0;

void showInfo(){
    LOG_INFO << CurrentThread::tid();
}

void initFunc(){
    printf("Create thread %d\n", ++count);
}

void test1(){
    ThreadPool pool;
    pool.setThreadSize(4);
    pool.add(initFunc);
    for(int i = 0; i < 5000; i++){
        pool.add(showInfo);
    }
    pool.add([]{ sleep(5);});
    pool.start();
}

int main(){
    test1();
    return 0;
}