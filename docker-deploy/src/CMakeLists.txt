cmake_minimum_required(VERSION 3.27)
project(my_proxy)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")

# boost
find_package(Boost REQUIRED COMPONENTS filesystem system)
include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})

# OpenSSL
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

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
        ${Boost_LIBRARIES}
        ${OPENSSL_LIBRARIES}
        pthread
)

