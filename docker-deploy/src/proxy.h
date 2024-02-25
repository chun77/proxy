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
    // client access port
    const char * port;
    int socket_fd = -1;
public:
    // constructor
    explicit proxy(const char *port);

    // get a connection id
    int get_connection_id();

    // create a socket and bind it to the port, and listen to the port
    void construct();

    // accept a connection, and create a new thread to handle the connection
    [[noreturn]] void accept();

    // destructor
    ~proxy();
};


#endif //ERSS_HWK2_HL490_ZW297_PROXY_H
