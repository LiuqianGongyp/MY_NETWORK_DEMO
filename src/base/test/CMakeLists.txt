set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/src/base/test)
add_executable(timestamptest test_timestamp.cpp)
add_executable(ThreadPoolTest test_ThreadPool.cpp)
target_link_libraries(timestamptest my_network_demo)
target_link_libraries(ThreadPoolTest my_network_demo)