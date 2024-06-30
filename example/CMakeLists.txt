add_executable(EchoServer EchoServer.cpp)
add_executable(HttpServer HttpServerTest.cpp)
add_executable(mysqlPoolTest MysqlPoolTest.cpp)
add_executable(loggingtest1 loggingtest.cpp)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/example)

target_link_libraries(EchoServer my_network_demo)
target_link_libraries(HttpServer my_network_demo)
target_link_libraries(mysqlPoolTest my_network_demo)
target_link_libraries(loggingtest1 my_network_demo)


