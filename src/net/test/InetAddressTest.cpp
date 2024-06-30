//
// Created by lq on 2024/6/20.
//
#include <iostream>
#include "InetAddress.h"

int main(){
    InetAddress addr(8080);
    std::cout << addr.toIpPort() << std::endl;
}