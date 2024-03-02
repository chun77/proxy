// deals with the request from the client

#include "client.h"
#include "proxy.h"
#include "server.h"

void send_error_response(int client_fd, int id, int error_occurred){
    if (error_occurred == 400){
        const char* response = "HTTP/1.1 400 Bad Request\r\n\r\n";
        if (send(client_fd, response, strlen(response), 0) < 0) {
            log_writer.write(id, "ERROR: Fail to send \"HTTP/1.1 400 Bad Request\" to the client");
        } else{
            log_writer.write(id, "Responding \"HTTP/1.1 400 Bad Request\" to the client");
        }
    }
    else if (error_occurred == 502){
        const char* response = "HTTP/1.1 502 Bad Gateway\r\n\r\n";
        if (send(client_fd, response, strlen(response), 0) < 0) {
            log_writer.write(id, "ERROR: Fail to send \"HTTP/1.1 502 Bad Gateway\" to the client");
        } else{
            log_writer.write(id, "Responding \"HTTP/1.1 502 Bad Gateway\" to the client");
        }
    } else if (error_occurred == 504){
        const char* response = "HTTP/1.1 504 Gateway Timeout\r\n\r\n";
        if (send(client_fd, response, strlen(response), 0) < 0) {
            log_writer.write(id, "ERROR: Fail to send \"HTTP/1.1 504 Gateway Timeout\" to the client");
        } else{
            log_writer.write(id, "Responding \"HTTP/1.1 504 Gateway Timeout\" to the client");
        }
    }
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

    // if the http method is CONNECT
    if (request.method == "CONNECT") {
        if (request.host.empty() || request.port.empty()) {
            send_error_response(client_fd, id, 400);
            return;
        }
        int server_fd = connect_server(id, request.host, request.port);
        if (server_fd == -1) {
            send_error_response(client_fd, id, 502);
        } else{
            handle_connect(id, client_fd, server_fd);
            close(server_fd);
        }
    } else{
        // if the request is not valid
        if (request.method.empty() || request.first_line.empty() || request.host.empty() || request.port.empty() || request.content.empty()) {
            send_error_response(client_fd, id, 400);
        }
        // if the http method is POST
        else if (request.method == "POST") {
            int server_fd = connect_server(id, request.host, request.port);
            if (server_fd == -1) {
                send_error_response(client_fd, id, 502);
            } else{
                handle_post(id, client_fd, server_fd, request.content);
                close(server_fd);
            }

        }
        // if the http method is GET
        else if (request.method == "GET") {
            boost::asio::io_context io_context;
            // resolve the hostname and port
            boost::asio::ip::tcp::resolver resolver(io_context);
            boost::beast::tcp_stream stream(io_context);

            boost::system::error_code ec;
            auto endpoints = resolver.resolve(request.host, request.port, ec);
            if (ec) {
                // handle the error
                log_writer.write(id, "ERROR: Failed to connect to server: " + ec.message());
                send_error_response(client_fd, id, 502);
                close(client_fd);
                return;
            }

            // connect to the server
            stream.connect(endpoints, ec);
            if (ec) {
                // handle the error
                log_writer.write(id, "ERROR: Failed to connect to server: " + ec.message());
                send_error_response(client_fd, id, 502);
                close(client_fd);
                return;
            }

            cache_item in_cache = cache.check(request.first_line);
            handle_get(id, client_fd, stream, request, in_cache);

        }
        // if the http method is not supported
        else {
            log_writer.write(id, "WARNING: http method not supported");
            send_error_response(client_fd, id, 400);
        }
    }
    close(client_fd);
}


// get the request from the client and parse it
request_item get_request(int id, int client_fd) {
    const int buffer_size = 65536;
    char buffer[buffer_size] = {};
    std::string buffer_str;
    request_item item;

    // receive the request from the client
    ssize_t bytes_received;
    bool error_occurred = false;
    bytes_received = recv(client_fd, buffer, buffer_size, 0);
    if (bytes_received > 0) {
        buffer_str = std::string(buffer, bytes_received);
    } else if (bytes_received < 0) {
        log_writer.write(id, "ERROR: Failed to receive data from the client.");
        send_error_response(client_fd, id, 502);
        error_occurred = true;
    } else {
        // Connection closed by the client
        log_writer.write(id, "ERROR: Connection closed by the client.");
        error_occurred = true;
    }

    if (buffer_str.empty()) {
        // No data received, treat as an error or handle accordingly
        log_writer.write(id, "ERROR: Received empty request from the client.");
        error_occurred = true;
    }

    if (error_occurred) {
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
    // get the url
    item.url = buffer_str.substr(first_space + 1, second_space - first_space - 1);
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
    while (true) {
        FD_ZERO(&read_fds);// Clear the set
        FD_SET(client_fd, &read_fds); // Add the client_fd to the set
        FD_SET(server_fd, &read_fds); // Add the server_fd to the set

        // check if there is data to read on the client_fd or server_fd
        // only leave the fds that have data to read
        if (select(max_fd, &read_fds, nullptr, nullptr, nullptr) < 0) {
            log_writer.write(id, "ERROR: select() failed");
            break;
        }

        // Forward data from client to server
        if (FD_ISSET(client_fd, &read_fds)) {
            auto bytes_read = read(client_fd, buffer, sizeof(buffer));
            if (bytes_read <= 0) break; // Connection closed or error
            if (send(server_fd, buffer, bytes_read, 0) < 0) {
                log_writer.write(id, "ERROR: Fail to forward data from client to server");
                break;
            }
        }

        // Forward data from server to client
        if (FD_ISSET(server_fd, &read_fds)) {
            long bytes_read = read(server_fd, buffer, sizeof(buffer));
            if (bytes_read <= 0) break; // Connection closed or error
            if (send(client_fd, buffer, bytes_read, 0) < 0) {
                log_writer.write(id, "ERROR: Fail to forward data from server to client");
                break;
            }
        }
    }
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
    const int buffer_size = 65536;
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

void handle_get(int id, int client_fd, boost::beast::tcp_stream& stream, request_item & request, cache_item &in_cache){
    // if the request is not in the cache
    if(in_cache.content.empty()){
        log_writer.write(id, "not in cache");

        // send the request to the server
        log_writer.write(id, "Requesting \"" + request.first_line + "\" from server " + request.host + ":" + request.port);
        boost::system::error_code ec;
        boost::beast::http::request<boost::beast::http::string_body> req;
        req.version(11);
        req.method(boost::beast::http::verb::get);
        req.target(request.url);
        req.set(boost::beast::http::field::host, request.host);
        req.set(boost::beast::http::field::user_agent, BOOST_BEAST_VERSION_STRING);

        // send the request to the server
        boost::beast::http::write(stream, req, ec);
        if (ec) {
            // failed to send the request
            log_writer.write(id, "ERROR: Fail to send the request to the server: " + ec.message());
            send_error_response(client_fd, id, 502);
            return;
        }

        // receive the parsed_response from the server
        boost::beast::flat_buffer response_buff;
        boost::beast::http::response<boost::beast::http::string_body> response;
        boost::beast::http::read(stream, response_buff, response, ec);

        // if the parsed_response is not received
        if (ec) {
            log_writer.write(id, "ERROR: Fail to receive the parsed_response from the server: " + ec.message());
            send_error_response(client_fd, id, 502);
            return;
        }

        // get the parsed_response, parse it
        cache_item parsed_response = parse_response(id, response, request);
        if (parsed_response.content.empty() ) {
            return;
        }

        // if the parsed_response is cacheable
        if(check_cacheable(id, parsed_response)){
            cache.add(request.first_line, parsed_response);
        }

        // send the parsed_response to the client
        if (send(client_fd, parsed_response.content.c_str(), parsed_response.content.size(), 0) < 0) {
            log_writer.write(id, "ERROR: Fail to send the parsed_response to the client");
        } else{
            log_writer.write(id, "Responding \"" + parsed_response.first_line + "\" to client");
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
        revalidate(id, client_fd, stream, request, in_cache);
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

