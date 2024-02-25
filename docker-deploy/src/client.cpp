// deals with the request from the client

#include "client.h"
#include "proxy.h"
#include "server.h"

// handle the client connections
void handle_client_connection(const std::string& ip, int id, int client_fd) {
    std::string request = get_request(id, client_fd);
    if (request.empty()) {
        close(client_fd);
        return;
    }

    // parse the request from the client
    std::string first_line, method, path, httpVersion;
    std::string hostname, port;

    // parse the first line of the request
    std::regex requestLineRegex(R"(^(\w+)\s+(\S+)\s+(HTTP/\d\.\d))");
    std::smatch requestLineMatch;
    if (std::regex_search(request, requestLineMatch, requestLineRegex) && requestLineMatch.size() > 3) {
        first_line = requestLineMatch[0];
        method = requestLineMatch[1];
    }

    // parse the host and port
    std::regex hostRegex(R"(Host:\s*(\S+):(\d+))");
    std::smatch hostMatch;
    if (std::regex_search(request, hostMatch, hostRegex) && hostMatch.size() > 2) {
        hostname = hostMatch[1];
        port = hostMatch[2];
    }

    // current time
    auto time_now= std::time(nullptr);
    auto utc_time = *std::gmtime(&time_now);
    std::string time_str = std::asctime(&utc_time);

    log_writer.write(id, '"' + first_line + '"' + " from " + ip + "@" + time_str);

    // create a new connection to the server
    int server_fd = connect_server(id, hostname, port);
    if (server_fd == -1) {
        log_writer.write(id, "ERROR: Fail to create a new connection to the server");
        close(client_fd);
        return;
    }

    if (method == "CONNECT") {
        handle_connect(id, client_fd, server_fd);
    } else if (method == "POST") {
        handle_post(id, client_fd, server_fd, request);
    } else if (method == "GET") {
        handle_get(id, client_fd, server_fd, request);
    }
    close(client_fd);
}

std::string get_request(int id, int client_fd) {
    const int buffer_size = 4096;
    char buffer[buffer_size];
    std::string buffer_str;

    // receive the request from the client
    ssize_t bytes_received;
    while ((bytes_received = recv(client_fd, buffer, buffer_size, 0)) > 0) {
        buffer_str.append(buffer, bytes_received);
    }
    if (bytes_received < 0) {
        // Send a 400 error code to the client
        log_writer.write(id, "WARNING: Received nothing from the client");
        const char* response = "HTTP/1.1 400 Bad Request\r\n\r\n";

        if (send(client_fd, response, strlen(response), 0) < 0) {
            log_writer.write(id, "WARNING: Fail to send HTTP/1.1 400 Bad Request to the client");
        } else{
            log_writer.write(id, "Responding HTTP/1.1 400 Bad Request to the client");
        }
        return "";
    }
    return buffer_str;
}

// handle the CONNECT method
// Todo
void handle_connect(int id, int client_fd, int server_fd) {
    // Send a 200 OK response to the client
    const char* response_to_client = "HTTP/1.1 200 Connection established\r\n\r\n";
    if (send(client_fd, response_to_client, strlen(response_to_client), 0) < 0) {
        log_writer.write(id, "WARNING: Fail to send HTTP/1.1 200 Connection established to the client");
        return;
    } else{
        log_writer.write(id, "Responding HTTP/1.1 200 Connection established to the client");
    }

    // forward the data between the client and the server
    fd_set read_fds;
    int max_fd = std::max(client_fd, server_fd) + 1;
    while (true) {
        FD_ZERO(&read_fds);
        FD_SET(client_fd, &read_fds);
        FD_SET(server_fd, &read_fds);
        if (select(max_fd, &read_fds, nullptr, nullptr, nullptr) == -1) {
            log_writer.write(id, "ERROR: select() failed");
            break;
        }

        if (FD_ISSET(client_fd, &read_fds)) {
            char buf[BUFSIZ];
            ssize_t bytes_received = recv(client_fd, buf, BUFSIZ, 0);
            if (bytes_received <= 0) {
                break;
            }
            ssize_t bytes_sent = send(server_fd, buf, bytes_received, 0);
            if (bytes_sent < 0) {
                break;
            }
        }

        if (FD_ISSET(server_fd, &read_fds)) {
            char buf[BUFSIZ];
            ssize_t bytes_received = recv(server_fd, buf, BUFSIZ, 0);
            if (bytes_received <= 0) {
                break;
            }
            ssize_t bytes_sent = send(client_fd, buf, bytes_received, 0);
            if (bytes_sent < 0) {
                break;
            }
        }
    }
}

void handle_post(int id, int client_fd, int server_fd, const std::string& request){

}
void handle_get(int id, int client_fd, int server_fd, const std::string& request){

}