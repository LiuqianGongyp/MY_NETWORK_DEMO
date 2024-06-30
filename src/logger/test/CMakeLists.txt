
add_executable(loggingtest loggingtest.cpp)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/src/logger/test)

target_link_libraries(loggingtest my_network_demo)