set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/src/net/test)

add_executable(InetAddrtest InetAddressTest.cpp)

target_link_libraries(InetAddrtest my_network_demo)