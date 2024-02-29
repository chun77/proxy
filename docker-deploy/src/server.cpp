// deals with the response from the server
#include "server.h"
#include "proxy.h"

int connect_server(int id, const std::string &hostname, const std::string &port) {
    struct addrinfo host_info{};
    struct addrinfo *host_info_list;
    int status;
    int socket_fd;

    memset(&host_info, 0, sizeof(host_info));
    host_info.ai_family = AF_UNSPEC;
    host_info.ai_socktype = SOCK_STREAM;

    status = getaddrinfo(hostname.c_str(), port.c_str(), &host_info, &host_info_list);
    if (status != 0) {
        log_writer.write(id, "ERROR: cannot get address info for server host");
        return -1;
    }

    socket_fd = socket(host_info_list->ai_family, host_info_list->ai_socktype, host_info_list->ai_protocol);
    if (socket_fd == -1) {
        log_writer.write(id, "ERROR: cannot create socket for server");
        return -1;
    }

    status = connect(socket_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
    if (status == -1) {
        log_writer.write(id, "ERROR: cannot connect to socket for server");
        return -1;
    }

    freeaddrinfo(host_info_list);
    return socket_fd;
}

bool check_cacheable(int id, cache_item &response) {
    if (response.status != "200") {
        log_writer.write(id, "not cacheable because status code is not 200");
        return false;
    }
    if (response.is_no_store) {
        log_writer.write(id, "not cacheable because no-store");
        return false;
    }
    if (response.is_private) {
        log_writer.write(id, "not cacheable because private");
        return false;
    }

    auto current_time = std::time(nullptr);
    if (response.is_no_cache || response.expiration_time <= current_time) {
        log_writer.write(id, "cached, but requires re-validation");
        response.need_validation = true;
        return true;
    }

    if (response.expiration_time > current_time) {
        std::string time_str = std::asctime(std::gmtime(&response.expiration_time));
        time_str.pop_back();
        log_writer.write(id, "cached, expires at " +  time_str);
        return true;
    }

    log_writer.write(id, "not cacheable because of control fields error");
    return false;
}




// get and parse the response from the server
cache_item get_response(int id, int server_fd, request_item &request){
    cache_item response;

    // receive the response from the server
    const int buffer_size = 4096;
    char buffer[buffer_size];
    std::string buffer_str;

    struct timeval tv{};
    tv.tv_sec = 3;  // 3 seconds timeout
    tv.tv_usec = 0;
    setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    // receive the request from the server
    ssize_t bytes_received;
    while ((bytes_received = recv(server_fd, buffer, buffer_size, 0)) > 0) {
        buffer_str.append(buffer, bytes_received);
        // if the response is complete
        if (buffer_str.find("\r\n\r\n") != std::string::npos) {
            break;
        }
    }
    if (buffer_str.empty()) {
        log_writer.write(id, "ERROR: Received nothing from the server " + request.host + ":" + request.port);
        // send error message to the client
        const char* response_to_client = "HTTP/1.1 502 Bad Gateway\r\n\r\n";
        if (send(id, response_to_client, strlen(response_to_client), 0) < 0) {
            log_writer.write(id, "ERROR: Fail to send \"HTTP/1.1 502 Bad Gateway\" to the client");
        } else{
            log_writer.write(id, "Responding \"HTTP/1.1 502 Bad Gateway\" to the client");
        }
        return response;
    }
    // parse the response

    //content
    response.content = buffer_str;
    //first line
    response.first_line = buffer_str.substr(0, buffer_str.find("\r\n"));
    log_writer.write(id, "Received \"" + response.first_line + "\" from server " + request.host + ":" + request.port);
    //status
    response.status = response.first_line.substr(response.first_line.find(' ') + 1, 3);
    if(response.status == "304") {
        return response;
    }
    //response time
    response.response_time = std::time(nullptr);

    // Expires
    std::regex expires_regex("Expires: (.*)");
    std::smatch expires_match;
    if (std::regex_search(buffer_str, expires_match, expires_regex) && expires_match.size() > 1) {
        std::tm tm = {};
        std::istringstream ss(expires_match[1]);
        ss >> std::get_time(&tm, "%a, %d %b %Y %H:%M:%S %Z");
        response.expiration_time = std::mktime(&tm);
    }

    // max-age
    std::regex max_age_regex("max-age=([0-9]+)");
    std::smatch max_age_match;
    if (std::regex_search(buffer_str, max_age_match, max_age_regex) && max_age_match.size() > 1) {
        response.max_age = std::stoi(max_age_match[1]);
        response.expiration_time = response.response_time + response.max_age;
    }

    // ETag
    std::regex ETag_regex("ETag: (.*)");
    std::smatch ETag_match;
    if (std::regex_search(buffer_str, ETag_match, ETag_regex) && ETag_match.size() > 1) {
        response.ETag = ETag_match[1];
    }
    //last modified
    std::regex last_modified_regex("Last-Modified: (.*)");
    std::smatch last_modified_match;
    if (std::regex_search(buffer_str, last_modified_match, last_modified_regex) && last_modified_match.size() > 1) {
        std::tm tm = {};
        std::istringstream ss(last_modified_match[1]);
        ss >> std::get_time(&tm, "%a, %d %b %Y %H:%M:%S %Z");
        response.last_modified = std::mktime(&tm);
    }
    // Cache-Control fields
    // must-revalidate
    if (buffer_str.find("must-revalidate") != std::string::npos) {
        response.is_must_revalidate = true;
    }
    // public
    if (buffer_str.find("public") != std::string::npos) {
        response.is_public = true;
    }
    // no-cache
    if (buffer_str.find("no-cache") != std::string::npos) {
        response.is_no_cache = true;
    }
    // no-store
    if (buffer_str.find("no-store") != std::string::npos) {
        response.is_no_store = true;
    }
    // private
    if (buffer_str.find("private") != std::string::npos) {
        response.is_private = true;
    }

    // Cache-Control fields
    log_writer.write(id, "NOTE " + buffer_str.substr(buffer_str.find("Cache-Control"), buffer_str.find("\r\n", buffer_str.find("Cache-Control")) - buffer_str.find("Cache-Control")));
    // ETAG
    if (!response.ETag.empty()) {
        log_writer.write(id, "NOTE ETag: " + response.ETag);
    }
    // Last-Modified
    if (response.last_modified != 0) {
        std::string time_str = std::asctime(std::gmtime(&response.last_modified));
        time_str.pop_back();
        log_writer.write(id, "NOTE Last-Modified: " + time_str);
    }
    return response;
}

void revalidate(int id, int client_fd, int server_fd, request_item & request, cache_item &in_cache){
    // revalidation
    // send the re-validation request to the server
    std::string revalidation_request = "GET " + request.first_line + " HTTP/1.1\r\n" + "Host: " + request.host + ":" + request.port + "\r\n";
    if(!in_cache.ETag.empty()){
        revalidation_request += "If-None-Match: " + in_cache.ETag + "\r\n";
    }
    if (in_cache.last_modified != 0){
        char buf[128];
        struct tm *gmt = gmtime(&in_cache.last_modified);
        strftime(buf, sizeof buf, "%a, %d %b %Y %H:%M:%S GMT", gmt);
        revalidation_request += "If-Modified-Since: ";
        revalidation_request += buf;
        revalidation_request += "\r\n";
    }
    revalidation_request += "Connection: close\r\n\r\n";
    if (send( server_fd, revalidation_request.c_str(), revalidation_request.size(), 0) < 0) {
        log_writer.write(id, "ERROR: Fail to send the re-validation request to the server");
        return;
    } else{
        log_writer.write(id, "Requesting \"" + request.first_line + "\" from server " + request.host + ":" + request.port);
    }

    // receive the response from the server
    cache_item revalidation_response = get_response(id, server_fd, request);
    // if it is 304
    if (revalidation_response.status == "304") {
        // update the cache
        in_cache.expiration_time = revalidation_response.expiration_time;
        in_cache.response_time = revalidation_response.response_time;
        cache.update(request.first_line, in_cache);
        // send the response to the client
        if (send(client_fd, in_cache.content.c_str(), in_cache.content.size(), 0) < 0) {
            log_writer.write(id, "ERROR: Fail to send the response to the client");
        } else{
            log_writer.write(id, "Responding the response to the client");
        }
        return ;
    } else if (revalidation_response.status == "200"){
        // if the response is cacheable
        if(check_cacheable(id, revalidation_response)){
            cache.update(request.first_line, revalidation_response);
        }
        // send the response to the client
        if (send(client_fd, revalidation_response.content.c_str(), revalidation_response.content.size(), 0) < 0) {
            log_writer.write(id, "ERROR: Fail to send the response to the client");
        } else{
            log_writer.write(id, "Responding the response to the client");
        }
        return;
    }
    else{
        // if the re-validation fails
        log_writer.write(id, "ERROR: Re-validation fails");
        return;
    }
}