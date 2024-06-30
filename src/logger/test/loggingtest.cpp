//
// Created by lq on 2024/6/4.
//
#include "AsyncLogging.h"
#include "Logging.h"
#include "Timestamp.h"

#include <stdio.h>
#include <unistd.h>

static const off_t kRollSize = 8 * 1024 * 1024;
AsyncLogging* g_asynclog = NULL;
std::unique_ptr<LogFile> g_logFile;

void dummyOutput(const char* msg, int len){
    if(g_logFile){
        g_logFile->append(msg, len);
    }
}

inline AsyncLogging* getAsyncLog(){
    return g_asynclog;
}

void test_Logging(){
    LOG_DEBUG << "debug";
    LOG_INFO << "info";
    LOG_WARN << "warn";
    LOG_ERROR << "error";

    const int n = 10;
    for(int i = 0; i < n; i++){
        LOG_INFO << "Hello, " << i << " abc...xyz";
    }
}

void test_AsyncLogging(){
    const int n = 1024;
    for(int i = 0; i < n; i++){
        LOG_INFO << "Hello, " << i << " abs...xyz";
    }
}

void asyncLog(const char* msg, int len){
    if(g_asynclog){
        g_asynclog->append(msg, len);
    }
}

int main(int argc, char* argv[]){
    printf("pid = %d\n", getpid());
    // 异步日志测试：
    /*AsyncLogging log(::basename(argv[0]), kRollSize);
    g_asynclog = &log;
    test_AsyncLogging();
    Logger::setOutput(asyncLog);//为Logger设置输出回调
    log.start();//开启日志后端线程
    test_AsyncLogging();
    sleep(1);
    log.stop();*/


    //同步日志测试：
    //test_Logging();//输出到stdout

    g_logFile.reset(new LogFile(argv[0], kRollSize, true));
    Logger::setOutput(dummyOutput);//为Logger设置输出回调
    test_Logging();//输出到文件

    return 0;
}