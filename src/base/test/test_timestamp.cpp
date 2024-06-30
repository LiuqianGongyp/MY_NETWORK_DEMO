//
// Created by lq on 2024/4/29.
//
#include "Timestamp.h"
int main(){
    Timestamp time;
    std::cout << time.now().toFormattedString() << std::endl;
    std::cout << time.now().toFormattedString(true) << std::endl;

    return 0;
}