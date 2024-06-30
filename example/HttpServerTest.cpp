//
// Created by lq on 2024/6/28.
//
#include "HttpServer.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "HttpContext.h"
#include "Timestamp.h"

#include "AsyncLogging.h"

extern char favicon[555];
bool benchmark = false;

void onRequest(const HttpRequest& req, HttpResponse* resp){
    LOG_DEBUG << "Headers " << req.methodString() << " " << req.path();

    if(!benchmark){
        const std::unordered_map<std::string, std::string>& headers = req.headers();
        for(const auto & header : headers){
            LOG_DEBUG << header.first << ": " << header.second;
        }
    }

    if(req.path() == "/"){
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setStatusMessage("OK");
        resp->setContentType("text/html");
        resp->addHeader("Server", "my_network_demo");
        std::string now = Timestamp::now().toFormattedString();
        resp->setBody("<html><head><title>This is title</title></head>"
                      "<body><h1>Hello</h1> Now is " + now + "</body></html>");
    }else if(req.path() == "/favicon.ico"){
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setStatusMessage("OK");
        resp->setContentType("image/png");
        resp->setBody(std::string(favicon, sizeof(favicon)));
    }else if(req.path() == "/hello"){
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setStatusMessage("OK");
        resp->setContentType("text/plain");
        resp->addHeader("Server", "my_network_demo");
        resp->setBody("hello, world!\n");
    }else{
        resp->setStatusCode(HttpResponse::k404NotFound);
        resp->setStatusMessage("Not Found");
        resp->setCloseConnection(true);
    }
}

AsyncLogging* g_asyncLog = nullptr;
void asyncOutput(const char* msg, int len){
    g_asyncLog->append(msg, len);
}

int main(){
    //Logger::setLogLevel(Logger::DEBUG);//设置日志等级，默认是INFO
    /**
     * 开启异步日志
     */
    off_t kRollSize = 500 * 1000 * 1000;
    AsyncLogging log("httpserver_log_file", kRollSize);
    g_asyncLog = &log;
    log.start();
    //Logger::setOutput(asyncOutput);

    EventLoop loop;
    HttpServer server(&loop, InetAddress(8004, "192.168.142.142"), "http-server");
    server.setHttpCallback(onRequest);
    server.setThreadNum(3);
    server.start();
    loop.loop();
    //log.stop();
}

char favicon[555] = {0};