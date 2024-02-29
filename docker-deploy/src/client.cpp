// deals with the request from the client

#include "client.h"
#include "proxy.h"
#include "server.h"



// handle the client connections
void handle_client_connection(const std::string& ip, int id, int client_fd) {
    request_item request = get_request(id, client_fd);
    if (request.content.empty()) {
        close(client_fd);
        return;
    }

    // get the current time
    auto time = std::time(nullptr);
    auto utc_time = *std::gmtime(&time);
    std::string time_str = std::asctime(&utc_time);
    time_str.pop_back();
    log_writer.write(id, '"' + request.first_line + '"' + " from " + ip + " @ " + time_str);

    // create a new connection to the server
    int server_fd = connect_server(id, request.host, request.port);
    if (server_fd == -1) {
        log_writer.write(id, "ERROR: Fail to create a new connection to the server");
        // send error message to the client
        const char* response = "HTTP/1.1 502 Bad Gateway\r\n\r\n";
        if (send(id, response, strlen(response), 0) < 0) {
            log_writer.write(id, "ERROR: Fail to send \"HTTP/1.1 502 Bad Gateway\" to the client");
        } else{
            log_writer.write(id, "Responding \"HTTP/1.1 502 Bad Gateway\" to the client");
        }
        close(client_fd);
        return;
    }

    if (request.method == "CONNECT") {
        handle_connect(id, client_fd, server_fd);
    } else if (request.method == "POST") {
        handle_post(id, client_fd, server_fd, request.content);
    } else if (request.method == "GET") {
        cache_item in_cache = cache.check(request.first_line);
        handle_get(id, client_fd, server_fd, request, in_cache);
    } else {
        log_writer.write(id, "WARNING: http method not supported");
        const char* response = "HTTP/1.1 400 Bad Request\r\n\r\n";
        if (send(client_fd, response, strlen(response), 0) < 0) {
            log_writer.write(id, "ERROR: Fail to send \"HTTP/1.1 400 Bad Request\" to the client");
        } else{
            log_writer.write(id, "Responding \"HTTP/1.1 400 Bad Request\" to the client");
        }
    }
    close(server_fd);
    close(client_fd);
}

// get the request from the client and parse it
request_item get_request(int id, int client_fd) {
    const int buffer_size = 4096;
    char buffer[buffer_size];
    std::string buffer_str;
    request_item item;

    struct timeval tv{};
    tv.tv_sec = 3;  // 3 seconds timeout
    tv.tv_usec = 0;
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    // receive the request from the client
    ssize_t bytes_received;
    while ((bytes_received = recv(client_fd, buffer, buffer_size, 0)) > 0) {
        buffer_str.append(buffer, bytes_received);
        // if the request is complete
        if (buffer_str.find("\r\n\r\n") != std::string::npos) {
            break;
        }
    }
    if (buffer_str.empty()) {
        // Send a 400 error code to the client
        log_writer.write(id, "ERROR: Received nothing from the client");
        const char* response = "HTTP/1.1 400 Bad Request\r\n\r\n";
        if (send(client_fd, response, strlen(response), 0) < 0) {
            log_writer.write(id, "ERROR: Fail to send \"HTTP/1.1 400 Bad Request\" to the client");
        } else{
            log_writer.write(id, "Responding \"HTTP/1.1 400 Bad Request\" to the client");
        }
        return item;
    }

    item.content = buffer_str;
    // parse the first line of the request
    size_t first_space = buffer_str.find(' ');
    if (first_space == std::string::npos) return item;
    // get the method
    item.method = buffer_str.substr(0, first_space);
    size_t second_space = buffer_str.find(' ', first_space + 1);
    if (second_space == std::string::npos) return item;

    size_t line_end = buffer_str.find("\r\n", second_space + 1);
    if (line_end == std::string::npos) {
        line_end = buffer_str.length();
    }
    // get the first line of the request
    item.first_line = buffer_str.substr(0, line_end);\

    // parse the host and port
    size_t host_pos = buffer_str.find("Host: ");
    if (host_pos != std::string::npos) {
        size_t start = buffer_str.find(' ', host_pos) + 1;
        size_t end = buffer_str.find("\r\n", start);
        std::string host_port = buffer_str.substr(start, end - start);
        size_t colon_pos = host_port.find(':');
        if (colon_pos != std::string::npos) {
            item.host = host_port.substr(0, colon_pos);
            std::istringstream(host_port.substr(colon_pos + 1)) >> item.port;
        } else {
            item.host = host_port;
            item.port = "80"; // http default port
        }
    }

    return item;
}

// handle the CONNECT method
// Todo
void handle_connect(int id, int client_fd, int server_fd) {
    // Send a 200 OK response to the client
    const char* response_to_client = "HTTP/1.1 200 Connection established\r\n\r\n";
    if (send(client_fd, response_to_client, strlen(response_to_client), 0) < 0) {
        log_writer.write(id, "ERROR: Fail to send \"HTTP/1.1 200 Connection established\" to the client");
        return;
    } else{
        log_writer.write(id, "Responding \"HTTP/1.1 200 Connection established\" to the client");
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
void handle_get(int id, int client_fd, int server_fd, request_item & request, cache_item &in_cache){
    // if the request is not in the cache
    if(in_cache.content.empty()){
        log_writer.write(id, "not in cache");

        // send the request to the server
        if (send(server_fd, request.content.c_str(), request.content.size(), 0) < 0) {
            log_writer.write(id, "ERROR: Fail to send the request to the server");
            return;
        } else{
            log_writer.write(id, "Requesting \"" + request.first_line + "\" from server " + request.host + ":" + request.port);
        }

        // get the response, parse it
        cache_item response = get_response(id, server_fd, request);
        if (response.content.empty() ) {
            return;
        }

        // if the response is cacheable
        if(check_cacheable(id, response)){
            cache.add(request.first_line, response);
        }

        // send the response to the client
        if (send(client_fd, response.content.c_str(), response.content.size(), 0) < 0) {
            log_writer.write(id, "ERROR: Fail to send the response to the client");
        } else{
            log_writer.write(id, "Responding \"" + response.first_line + "\" to client");
        }
    }
    // need revalidation
    else if (in_cache.need_validation || in_cache.expiration_time <= std::time(nullptr)){
        if (in_cache.need_validation){
            log_writer.write(id, "in cache, but requires re-validation");
        } else{
            std::string time_str = std::asctime(std::gmtime(&in_cache.expiration_time));
            time_str.pop_back();
            log_writer.write(id, "in cache, but expired at " +  time_str);
        }
       revalidate(id, client_fd, server_fd, request, in_cache);
    }
    else {
        log_writer.write(id, "in cache, valid");
        // send the response to the client
        if (send(client_fd, in_cache.content.c_str(), in_cache.content.size(), 0) < 0) {
            log_writer.write(id, "ERROR: Fail to send the response to the client");
        } else{
            log_writer.write(id, "Responding the response to the client");
        }
        return;
    }
}

