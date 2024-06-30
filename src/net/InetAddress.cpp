//
// Created by lq on 2024/6/20.
//

#include "InetAddress.h"

InetAddress::InetAddress(uint16_t port, std::string ip) {
    ::bzero(&addr_, sizeof(addr_));
    addr_.sin_family = AF_INET;//IPv4
    addr_.sin_port = htons(port);
    addr_.sin_addr.s_addr = ::inet_addr(ip.c_str());
    //addr_.sin_addr.s_addr = htonl(INADDR_ANY);//表示绑定所有可用的网络接口
}

std::string InetAddress::toIp() const {
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf));
    return buf;
}

std::string InetAddress::toIpPort() const {
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf));
    size_t end = ::strlen(buf);
    uint16_t port = ::ntohs(addr_.sin_port);
    sprintf(buf + end, ":%u", port);
    return buf;
}

uint16_t InetAddress::toPort() const {
    return ::ntohs(addr_.sin_port);
}