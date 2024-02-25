
// the proxy functions
#include "proxy.h"
#include "client.h"
#include "server.h"

Cache cache;
Log log_writer;

// initialize the proxy
proxy::proxy(const char *port) : port(port){}

int proxy::get_connection_id() {
    return ++connection_id;
}

// create a proxy
void proxy::construct() {
    // initialize the host info
    const char * hostname = nullptr;
    struct addrinfo host_info{};
    struct addrinfo * host_info_list;
    memset(&host_info, 0, sizeof(host_info));
    host_info.ai_family = AF_UNSPEC;
    host_info.ai_socktype = SOCK_STREAM;
    host_info.ai_flags = AI_PASSIVE;

    // get the host info
    if (getaddrinfo(hostname, port, &host_info, &host_info_list) != 0) {
        std::string error_msg = "Error: cannot get info of host "+ std::string(hostname) +":"+ std::string(port);
        log_writer.write(-1, error_msg);
        throw std::runtime_error(error_msg);
    }

    // create a socket
    socket_fd = socket(host_info_list->ai_family,
                       host_info_list->ai_socktype,
                       host_info_list->ai_protocol);
    if (socket_fd == -1) {
        std::string error_msg = "Error: cannot create socket " + std::string(hostname) + ":" + std::string(port);
        log_writer.write(-1, error_msg);
        throw std::runtime_error(error_msg);
    }

    // bind the socket
    int yes = 1;
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
    if (bind(socket_fd, host_info_list->ai_addr, host_info_list->ai_addrlen) == -1) {
        std::string error_msg = "Error: cannot bind socket " + std::string(hostname) + ":" + std::string(port);
        log_writer.write(-1, error_msg);
        throw std::runtime_error(error_msg);
    } //if

    // listen on the socket
    if (listen(socket_fd, 100) == -1) {
        std::string error_msg = "Error: cannot listen on socket " + std::string(hostname) + ":" + std::string(port);
        log_writer.write(-1, error_msg);
        throw std::runtime_error(error_msg);
    } //if

    freeaddrinfo(host_info_list);
}


// accept the client connections and create a new thread to handle the connection
[[noreturn]] void proxy::accept() {
    // accept the connection
    struct sockaddr_storage socket_addr{};
    socklen_t socket_addr_len = sizeof(socket_addr);
    int client_connection_fd;

    // accept the connection
    while (true){
        client_connection_fd = ::accept(socket_fd, (struct sockaddr *)&socket_addr, &socket_addr_len);
        if (client_connection_fd == -1) {
            std::string error_msg = "Error: cannot accept connection on socket";
            log_writer.write(-1, error_msg);
            continue;
        } //if

        char client_ip[INET6_ADDRSTRLEN] = {0};

        if (socket_addr.ss_family == AF_INET) {
            // IPv4
            auto *addr_in = (struct sockaddr_in *)&socket_addr;
            inet_ntop(AF_INET, &(addr_in->sin_addr), client_ip, INET6_ADDRSTRLEN);
        } else if (socket_addr.ss_family == AF_INET6) {
            // IPv6
            auto *addr_in6 = (struct sockaddr_in6 *)&socket_addr;
            inet_ntop(AF_INET6, &(addr_in6->sin6_addr), client_ip, INET6_ADDRSTRLEN);
        }

        // create a new thread to handle the connection
        std::string ip_str(client_ip);
        std::thread t(handle_client_connection, ip_str, get_connection_id(), client_connection_fd);
        t.detach();
    } //while
}

proxy::~proxy() {
    close(socket_fd);
    printf("Proxy destroyed on port\n");
}


