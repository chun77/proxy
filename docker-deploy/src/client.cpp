// deals with the request from the client

#include "client.h"
#include "proxy.h"
#include "server.h"


void send_bad_request_response(int fd, int id) {
    const char* response = "HTTP/1.1 400 Bad Request\r\n\r\n";
    if (send(fd, response, strlen(response), 0) < 0) {
        log_writer.write(id, "ERROR: Fail to send \"HTTP/1.1 400 Bad Request\" to the client");
    } else{
        log_writer.write(id, "Responding \"HTTP/1.1 400 Bad Request\" to the client");
    }
    close(fd);

}


// handle the client connections
void handle_client_connection(const std::string& ip, int id, int client_fd) {
    request_item request = get_request(id, client_fd);

    // get the current time
    auto time = std::time(nullptr);
    auto utc_time = *std::gmtime(&time);
    std::string time_str = std::asctime(&utc_time);
    time_str.pop_back();
    log_writer.write(id, '"' + request.first_line + '"' + " from " + ip + " @ " + time_str);

    // create a new connection to the server
    // if the request destination is the server on the host machine, use "host.docker.internal" as the hostname
    if (request.host == "localhost") {
        request.host = "host.docker.internal"; // "host.docker.internal" is the hostname that refers to the host machine
    }

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
        if (request.host.empty() || request.port.empty()) {
            send_bad_request_response(client_fd, id);
            return;
        }
        handle_connect(id, client_fd, server_fd);
    } else{
        if (request.method.empty() || request.first_line.empty() || request.host.empty() || request.port.empty() || request.content.empty()) {
            send_bad_request_response(client_fd, id);
        }else if (request.method == "POST") {
            handle_post(id, client_fd, server_fd, request.content);
        }else if (request.method == "GET") {
            cache_item in_cache = cache.check(request.first_line);
            handle_get(id, client_fd, server_fd, request, in_cache);
        }else {
            log_writer.write(id, "WARNING: http method not supported");
            send_bad_request_response(client_fd, id);
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
    // increase the timeout for the client
    tv.tv_sec = 10;  // 10 seconds timeout
    tv.tv_usec = 0;
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    // receive the request from the client
    ssize_t bytes_received;
    while ((bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_received] = '\0'; // Null-terminate the buffer to make it a valid C-string
        buffer_str.append(buffer, bytes_received);
        // if the request is complete
        if (buffer_str.find("\r\n\r\n") != std::string::npos) {
            break;
        }
    }

    if (bytes_received < 0) {
        // An error occurred with recv
        log_writer.write(id, "ERROR: recv failed with error code " + std::to_string(errno));
        return item;  // Return an empty item to signify an error.
    }

    if (bytes_received == 0) {
        // The client closed the connection, or timeout happened
        log_writer.write(id, "INFO: No data received from the client, connection closed or timeout.");
        return item;  // Return an empty item to signify a closed connection or timeout.
    }

    // At this point, we have received some data. Now we check if it's valid.
    if (buffer_str.empty() || buffer_str.find("\r\n\r\n") == std::string::npos) {
        // The request is incomplete or empty, but no recv error has occurred.
        log_writer.write(id, "INFO: Incomplete request received from the client.");
        return item;  // Return an empty item to signify an incomplete request.
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
    // send the response to the client
    const char* response = "HTTP/1.1 200 Connection Established\r\n\r\n";
    if (send(client_fd, response, strlen(response), 0) < 0) {
        log_writer.write(id, "ERROR: Fail to send \"HTTP/1.1 200 Connection Established\" to the client");
    } else{
        log_writer.write(id, "Responding \"HTTP/1.1 200 Connection Established\" to the client");
    }

    // Forward the data between the client and the server
    int max_fd = std::max(client_fd, server_fd) + 1; // the first argument of select() should be the highest-numbered file descriptor plus 1
    fd_set read_fds; // Set of file descriptors to read from
    char buffer[4096]; // Adjust size as needed
    while (1) {
        FD_ZERO(&read_fds);// Clear the set
        FD_SET(client_fd, &read_fds); // Add the client_fd to the set
        FD_SET(server_fd, &read_fds); // Add the server_fd to the set

        // check if there is data to read on the client_fd or server_fd
        // only leave the fds that have data to read
        if (select(max_fd, &read_fds, NULL, NULL, NULL) < 0) {
            log_writer.write(id, "ERROR: select() failed");
            break;
        }

        // Forward data from client to server
        if (FD_ISSET(client_fd, &read_fds)) {
            int bytes_read = read(client_fd, buffer, sizeof(buffer));
            if (bytes_read <= 0) break; // Connection closed or error
            if (send(server_fd, buffer, bytes_read, 0) < 0) {
                log_writer.write(id, "ERROR: Fail to forward data from client to server");
                break;
            }
        }

        // Forward data from server to client
        if (FD_ISSET(server_fd, &read_fds)) {
            int bytes_read = read(server_fd, buffer, sizeof(buffer));
            if (bytes_read <= 0) break; // Connection closed or error
            if (send(client_fd, buffer, bytes_read, 0) < 0) {
                log_writer.write(id, "ERROR: Fail to forward data from server to client");
                break;
            }
        }
    }

    // Close connections if loop exits
    close(client_fd);
    close(server_fd);
}



void handle_post(int id, int client_fd, int server_fd, const std::string& request){
    // send the request to the server
    const char* request_to_server = request.c_str();
    if (send(server_fd, request_to_server, strlen(request_to_server), 0) < 0) {
        log_writer.write(id, "WARNING: Fail to send the request to the server");
        return;
    } else{
        log_writer.write(id, "Sending the request to the server");
    }

    std::cout<<"request to server: "<<request_to_server<<std::endl;

    // receive the response from the server
    const int buffer_size = 4096;
    char buffer[buffer_size];
    std::string buffer_str;
    ssize_t bytes_received;
    while ((bytes_received = recv(server_fd, buffer, buffer_size, 0)) > 0) {
        buffer_str.append(buffer, bytes_received);
    }
    if (bytes_received < 0) {
        log_writer.write(id, "WARNING: Received nothing from the server");
        return;
    }

    std::cout<<"response from server: "<<buffer_str<<std::endl;

    // send the response to the client
    const char* response_to_client = buffer_str.c_str();
    if (send(client_fd, response_to_client, strlen(response_to_client), 0) < 0) {
        log_writer.write(id, "WARNING: Fail to send the response to the client");
        return;
    } else{
        log_writer.write(id, "Sending the response to the client");
    }
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

