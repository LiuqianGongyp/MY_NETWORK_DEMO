//
// Created by lq on 2024/6/28.
//


#include "HttpResponse.h"
#include "Buffer.h"

#include <stdio.h>

void HttpResponse::appendToBuffer(Buffer * output) const{
    /*典型的响应消息：
     *   HTTP/1.1 200 OK // 状态行   HTTP/1.1: 指示服务器使用的 HTTP 版本。
                                    200: 状态码，表示请求成功。200 是表示成功的标准状态码。
                                    OK: 状态码的文本描述，说明请求成功。
        //以下均为响应头部
     *   Date:Mon,31Dec200104:25:57GMT // 服务器生成响应的日期和时间，使用 GMT（格林尼治标准时间）格式。
     *   Server:Apache/1.3.14(Unix) //服务器软件的信息，包括名称和版本。这里表示使用的是 Apache 1.3.14 版本的服务器软件，并且运行在 Unix 系统上。
     *   Content-type:text/html //响应内容的媒体类型。text/html 表示内容是 HTML 文档。
     *   Last-modified:Tue,17Apr200106:46:28GMT // 响应内容的最后修改时间。
     *   Etag:"a030f020ac7c01:1e9f" // 实体标签（Entity Tag），用于标识资源的特定版本，便于缓存验证和协商。
     *   Content-length:39725426 // 响应消息体的长度（以字节为单位）。这里表示消息体的长度为 39,725,426 字节。
     *   Content-range:bytes554554-40279979/40279980 // 指定响应的字节范围，用于分块传输。这里表示响应包含的字节范围是从 554,554 到 40,279,979，总大小是 40,279,980 字节。
    */
    char buf[32];
    // 先把"HTTP/1.0 200 OK\r\n"添加到buffer中
    snprintf(buf, sizeof(buf), "HTTP/1.1 %d ", statusCode_);
    // muduo重载了append函数，我这里没重载如果出错请检查这里
    output->append(buf);
    output->append(statusMessage_.data());
    output->append("\r\n");

    if (closeConnection_)
    {
        output->append("Connection: close\r\n");
    }
    else
    {
        snprintf(buf, sizeof(buf), "Content-Length: %zd\r\n", body_.size());
        output->append(buf);
        output->append("Connection: Keep-Alive\r\n");
    }

    for (const auto& header : headers_)
    {
        output->append(header.first);
        output->append(": ");
        output->append(header.second);
        output->append("\r\n");
    }

    output->append("\r\n");
    output->append(body_);
}
