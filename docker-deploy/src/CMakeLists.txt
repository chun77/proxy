cmake_minimum_required(VERSION 3.27)
project(my_proxy)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")

# boost
find_package(Boost REQUIRED COMPONENTS filesystem system)
include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})

#thread
# Add thread support
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(my_proxy
        main.cpp
        proxy.cpp
        proxy.h
        cache_log.h
        client.cpp
        client.h
        server.cpp
        server.h
)
target_link_libraries(my_proxy
        Boost::filesystem
        Boost::system
        Threads::Threads # Add thread support
)

