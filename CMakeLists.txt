cmake_minimum_required(VERSION 3.5)
project(epoll_demo)

set(CMAKE_CXX_STANDARD 11)

add_executable(epoll_server_demo epoll_server.cpp)
