cmake_minimum_required(VERSION 3.9)
project(http_client)

set(CMAKE_CXX_STANDARD 14)

add_executable(http_client main.cpp http_client.hpp)
target_link_libraries(
        http_client
        boost_system
        boost_chrono
        boost_thread
        pthread
)
