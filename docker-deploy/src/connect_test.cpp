#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

void connectToProxyServer(const std::string& proxyHost, int proxyPort, const std::string& targetHost, int targetPort) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return;
    }

    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(proxyPort);

    if (inet_pton(AF_INET, proxyHost.c_str(), &serverAddr.sin_addr) <= 0) {
        std::cerr << "Invalid address/ Address not supported" << std::endl;
        return;
    }

    if (connect(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Connection Failed" << std::endl;
        return;
    }

    // send the CONNECT request to the proxy server
    std::string httpRequest = "CONNECT " + targetHost + ":" + std::to_string(targetPort) + " HTTP/1.1\r\nHost: " + targetHost + "\r\nConnection: close\r\n\r\n";
    send(sock, httpRequest.c_str(), httpRequest.size(), 0);

    const int bufferSize = 4096;
    char buffer[bufferSize];
    std::string response;

    while (int received = recv(sock, buffer, bufferSize, 0)) {
        if (received == -1) {
            std::cerr << "Failed to receive data" << std::endl;
            break;
        }

        response.append(buffer, received);
    }

    close(sock);

    std::cout << "Response from server:" << std::endl;
    std::cout << response << std::endl;
}

int main() {
    std::string proxyHost = "127.0.0.1"; // proxy server's IP address
    int proxyPort = 8080;                // proxy server's port that listens for incoming connections
    std::string targetHost = "www.google.com"; // target host's IP address
    int targetPort = 443;                    // target host's port

    connectToProxyServer(proxyHost, proxyPort, targetHost, targetPort);

    return 0;
}
