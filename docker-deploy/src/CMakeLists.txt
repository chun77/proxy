cmake_minimum_required(VERSION 3.27)
project(my_proxy)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

include_directories(.)

add_executable(my_proxy
        main.cpp
        proxy.cpp
        proxy.h)
