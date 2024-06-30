//
// Created by lq on 2024/6/28.
//

#include "Buffer.h"
#include "HttpContext.h"

bool HttpContext::processRequestLine(const char* begin, const char* end)
{
    // 一个典型的 HTTP GET 请求头部:
    // GET /index.html?id=acer HTTP/1.1 // 请求行(本函数只解析请求行):
    //   // GET: HTTP 方法，表示请求服务器获取资源。
    //   // /index.html?id=acer: 请求的资源路径和查询参数，这里请求的是服务器上的 index.html 文件，并附带一个查询参数 id=acer。
    //   // HTTP/1.1: 使用的 HTTP 协议版本。
    // Host: 127.0.0.1:8002 // 指定请求的主机和端口号。这里是 127.0.0.1 表示本地服务器，端口号是 8002。
    // User-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:102.0) Gecko/20100101 Firefox/102.0 //提供有关客户端浏览器和操作系统的信息。这里表示客户端使用的是 Firefox 102.0 浏览器，运行在 Ubuntu Linux x86_64 系统上。
    //   // Mozilla/5.0: Mozilla 是早期 Netscape 浏览器的代号。它成为了用户代理字符串的标准开头部分，以确保兼容性。5.0 表示浏览器的版本号，但在现代浏览器中，这个数字通常没有实际意义。最初的 Mozilla/5.0 是为了与 Netscape 5.0 兼容。
    //   // (X11; Ubuntu; Linux x86_64): X11 表示浏览器运行在一个 X11 窗口系统中，这通常是类 Unix 系统（如 Linux）使用的窗口系统。Ubuntu 指示操作系统是 Ubuntu Linux。Linux x86_64 表示操作系统是 64 位的 Linux。
    //   // rv:102.0: rv 表示 Gecko 引擎的版本号，102.0 是该版本的标识。Gecko 是 Firefox 浏览器使用的排版引擎。
    //   // Gecko/20100101: Gecko 是 Firefox 使用的排版引擎的名称。20100101 是一个固定的日期字符串，常用于表示 Gecko 引擎的一个特定版本。
    //   // Firefox/102.0:Firefox 是浏览器的名称。102.0 是 Firefox 浏览器的版本号。
    // Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,*/*;q=0.8 // 指定客户端可以接收的内容类型。text/html 表示 HTML 文档，application/xhtml+xml 表示 XHTML 文档，application/xml;q=0.9 表示 XML 文档，image/avif 表示 AVIF 图像，image/webp 表示 WebP 图像，*/*;q=0.8 表示所有其他类型的内容，
    // Accept-Language: en-US,en;q=0.5 // 指定客户端接受的语言。en-US 表示美国英语，en;q=0.5 表示任何英语变体，优先级较低
    // Accept-Encoding: gzip, deflate, br // 指定客户端支持的内容编码。gzip 和 deflate 是常见的压缩编码，br 是 Brotli 压缩编码。
    //   // 质量因子 q 指定了优先级。 质量因子的作用是在服务器响应时，允许服务器根据客户端的优先级偏好选择最佳的内容类型、语言或编码。这样可以提高用户体验，确保客户端接收到最合适的内容。
    // Connection: keep-alive // 指定是否保持连接。keep-alive 表示客户端希望保持与服务器的连接，以便后续请求可以重用同一连接，提高性能。

    bool succeed = false;
    const char *start = begin;
    const char *space = std::find(start, end, ' ');         // 返回空格所在位置
    if (space != end && request_.setMethod(start, space))   // 判断请求方法是否有效
    {
        start = space + 1;                                  // 跳过空格
        space = std::find(start, end, ' ');                 // 继续寻找下一个空格
        if (space != end)
        {
            const char* question = std::find(start, space, '?');    // path和query是用"？"隔开的
            if (question != space)
            {
                request_.setPath(start, question);
                request_.setQuery(question, space);
            }
            else
            {
                request_.setPath(start, space);
            }
            start = space + 1;
            succeed = end-start == 8 && std::equal(start, end-1, "HTTP/1.");
            if (succeed)
            {
                if (*(end-1) == '1')
                {
                    request_.setVersion(HttpRequest::kHttp11);
                }
                else if (*(end-1) == '0')
                {
                    request_.setVersion(HttpRequest::kHttp10);
                }
                else
                {
                    succeed = false;
                }
            }
        }
    }
    return succeed;
}

bool HttpContext::parseRequest(Buffer *buf, Timestamp receiveTime)
{
    bool ok = true;                                 // 表示解析是否成功
    bool hasMore = true;                            // 表示是否还有更多的信息需要我解析,
    while (hasMore)
    {
        if (state_ == kExpectRequestLine)
        {
            const char *crlf = buf->findCRLF();     // CRLF表示“\r\n", 如果找到了就返回"\r\n"所在位置
            if (crlf)                               // 如果buffer的可读缓冲区有"\r\n"
            {
                ok = processRequestLine(buf->peek(), crlf); // peek()返回readerIndex_
                if (ok)
                {
                    request_.setReceiveTime(receiveTime);
                    buf->retrieveUntil(crlf + 2);   // 从buffer中取出一行，crlf+2的原因是，crlf自生存储的\r\n占用两个字符，所以+2才能移动到请求行的尾部
                    state_ = kExpectHeaders;        // 接下来希望解析请求头
                }
                else
                {
                    hasMore = false;
                }
            }
            else
            {
                hasMore = false;
            }
        }
        else if (state_ == kExpectHeaders)
        {
            const char *crlf = buf->findCRLF();
            if (crlf)
            {
                // 找到“：”位置
                const char *colon = std::find(buf->peek(), crlf, ':');
                if (colon != crlf)
                {
                    request_.addHeader(buf->peek(), colon, crlf);
                }
                else
                {
                    // 头部结束后的空行
                    state_ = kGotAll;
                    hasMore = false;
                }
                buf->retrieveUntil(crlf + 2);
            }
            else
            {
                hasMore = false;
            }
        }
        else if (state_ == kExpectBody)
        {

        }
    }
    return ok;
}