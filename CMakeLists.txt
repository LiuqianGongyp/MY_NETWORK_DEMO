cmake_minimum_required(VERSION 3.10)

project(my_network_demo)

set(CMAKE_CXX_STANDARD 11)

include_directories(
        ${PROJECT_SOURCE_DIR}/src/base
        ${PROJECT_SOURCE_DIR}/src/logger
        ${PROJECT_SOURCE_DIR}/src/net
        ${PROJECT_SOURCE_DIR}/src/net/poller
        ${PROJECT_SOURCE_DIR}/src/timer
        ${PROJECT_SOURCE_DIR}/src/http
        ${PROJECT_SOURCE_DIR}/src/mysql
)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/base SRC_BASE)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/logger SRC_LOG)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/net SRC_NET)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/net/poller SRC_POLLER)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/timer SRC_TIMER)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/http SRC_HTTP)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/mysql SRC_MYSQL)

set(CXX_FLAGS
        -g
        -Wall
        -std=c++11
)
add_library(my_network_demo SHARED
        ${SRC_BASE}
        ${SRC_LOG}
        ${SRC_NET}
        ${SRC_POLLER}
        ${SRC_TIMER}
        ${SRC_HTTP}
        ${SRC_MYSQL}
)
target_link_libraries(my_network_demo pthread mysqlclient)

# 设置生成动态库的路径
set(LIBRARY_OUTPUT_PATH  ${PROJECT_SOURCE_DIR}/lib)

#设置安装的默认路径
set(CMAKE_INSTALL_PREFIX ${PROJECT_SOURCE_DIR})

# 将目录安装到include/tiny_net_work
install(DIRECTORY ${PROJECT_SOURCE_DIR}/src/ DESTINATION include/my_network_demo
        FILES_MATCHING PATTERN "*.h")

add_subdirectory(example)

add_subdirectory(src/logger/test)
add_subdirectory(src/base/test)
add_subdirectory(src/net/test)