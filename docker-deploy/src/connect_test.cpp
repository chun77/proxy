//
// Created by Zichun Wang on 2/25/24.
//
#include <iostream>
#include <sstream>
#include <string>
#include <cstring>
#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>

int main() {
    const std::string proxy_host = "localhost"; // Proxy server address
    const std::string proxy_port = "8080";      // Proxy server port
    const std::string target_host = "your_https_server_host"; // Target HTTPS server address
    const std::string target_port = "443";      // Target HTTPS server port

    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(proxy_host.c_str(), proxy_port.c_str(), &hints, &servinfo)) != 0) {
        std::cerr << "getaddrinfo: " << gai_strerror(rv) << std::endl;
        return 1;
    }

    // Loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break; // If we get here, we must have a successful connection
    }

    if (p == NULL) {
        std::cerr << "client: failed to connect\n";
        return 2;
    }

    freeaddrinfo(servinfo); // All done with this structure

    std::string connect_request = "CONNECT " + target_host + ":" + target_port + " HTTP/1.1\r\nHost: " + target_host + "\r\n\r\n";
    if (send(sockfd, connect_request.c_str(), connect_request.length(), 0) == -1) {
        perror("send");
        close(sockfd);
        return 3;
    }

    char buf[1024];
    memset(buf, 0, sizeof buf);
    if (recv(sockfd, buf, sizeof buf, 0) == -1) {
        perror("recv");
        close(sockfd);
        return 4;
    }

    std::cout << "Received:\n" << buf << std::endl;

    close(sockfd);
    return 0;
}

