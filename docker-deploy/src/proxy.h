// the proxy class

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include <thread>
#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "cache_log.h"

#ifndef ERSS_HWK2_HL490_ZW297_PROXY_H
#define ERSS_HWK2_HL490_ZW297_PROXY_H

extern Log log_writer;
extern Cache cache;

class proxy {
    // port number
private:
    int connection_id = 0;
    const char * port;
    int socket_fd = -1;


public:
    // constructor
    explicit proxy(const char *port);

    // get a connection id
    int get_connection_id();

    void construct(); // 创建socket，绑定端口，监听端口
    // accept the proxy
    [[noreturn]] void accept();// 循环接受连接，创建线程处理连接
    // destructor
    ~proxy();

};


#endif //ERSS_HWK2_HL490_ZW297_PROXY_H
