#!/bin/bash

set -e

SOURCE_DIR=`pwd`
SRC_BASE=${SOURCE_DIR}/src/base

# 如果没有build目录 创建该目录
if [ ! -d `pwd`/build ]; then
  mkdir `pwd`/build
fi

# 如果没有build目录 创建该目录
if [ ! -d `pwd`/include ]; then
  mkdir `pwd`/include
fi

# 如果没有build目录 创建该目录
if [ ! -d `pwd`/lib ]; then
  mkdir `pwd`/lib
fi

# 删除存在build目录生成文件并执行cmake命令
rm -fr $`pwd`/build/*
cd `pwd`/build &&
  cmake .. &&
  make install

cd ..

#告诉系统libmy_network_demo.so的目录
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/lq/CLionProjects/my_network_demo/lib

#将头文件拷贝到 /usr/include
sudo cp ${SOURCE_DIR}/include/my_network_demo -r /usr/local/include

#将动态库文件复制到/usr/lib
sudo cp ${SOURCE_DIR}/lib/libmy_network_demo.so /usr/local/lib

#使操作生效
sudo ldconfig