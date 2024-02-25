// deals with the response from the server
#include "server.h"
#include "proxy.h"

int connect_server(int id, const std::string &hostname, const std::string &port) {
    struct addrinfo host_info;
    struct addrinfo *host_info_list;
    int status;
    int socket_fd;

    memset(&host_info, 0, sizeof(host_info));
    host_info.ai_family = AF_UNSPEC;
    host_info.ai_socktype = SOCK_STREAM;

    status = getaddrinfo(hostname.c_str(), port.c_str(), &host_info, &host_info_list);
    if (status != 0) {
        log_writer.write(id, "ERROR: cannot get address info for host");
        return -1;
    }

    socket_fd = socket(host_info_list->ai_family, host_info_list->ai_socktype, host_info_list->ai_protocol);
    if (socket_fd == -1) {
        log_writer.write(id, "ERROR: cannot create socket");
        return -1;
    }

    status = connect(socket_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
    if (status == -1) {
        log_writer.write(id, "ERROR: cannot connect to socket");
        return -1;
    }

    freeaddrinfo(host_info_list);
    return socket_fd;
}


static bool is_cacheable(cache_item &response) {
    if(response.is_no_store){
        log_writer.write(response.id, "not cacheable because of no-store");
        return false;
    }
    if(response.is_private){
        log_writer.write(response.id, "not cacheable because it is private");
        return false;
    }
    if(response.ETag.empty() && response.last_modified == 0){
        log_writer.write(response.id, "not cacheable because it has no ETag or Last_Modified");
        return false;
    }
    if(response.is_chunked){
        log_writer.write(response.id, "not cacheable because it is chunked");
        return false;
    }
    return true;
}

static bool is_valid(cache_item &response){
    if(response.is_no_cache){
        log_writer.write(response.id, "in cache, requires validation");
        return false;
    }

    // get current time
    std::time_t current_time = std::time(nullptr);
    std::stringstream expiration_time_ss;
    expiration_time_ss << std::put_time(std::localtime(&response.expiration_time), "%Y-%m-%d %H:%M:%S");

    // there is no expiration time
    if(response.expiration_time == 0){
        if (response.is_must_revalidate){
            log_writer.write(response.id, "in cache, requires validation");
            return false;
        }
        log_writer.write(response.id, "in cache, valid");
        return true;
    }

    // there is expiration time
    if (response.expiration_time <= current_time){
        log_writer.write(response.id, "in cache, but expired at " + expiration_time_ss.str());
        return false;
    } else {
        log_writer.write(response.id, "in cache, valid");
        return true;
    }
}


// parse the response from the server
cache_item server_response_parse(const std::string &response, const std::string &url) {
    cache_item item;
    std::istringstream response_stream(response);
    std::string line;
    while (std::getline(response_stream, line)) {
        // parse the response
        if (line.find("HTTP/1.1 200 OK") != std::string::npos) {
            // parse the response time
            std::time_t response_time = std::time(nullptr);
            item.response_time = response_time;
            // parse the expiration time
            std::time_t expiration_time = 0;
            std::time_t max_age = 0;
            std::time_t expires = 0;
            std::string ETag;
            std::time_t last_modified = 0;
            while (std::getline(response_stream, line)) {
                if (line.find("Cache-Control: max-age=") != std::string::npos) {
                    std::string max_age_str = line.substr(23);
                    max_age = std::stol(max_age_str);
                    expiration_time = response_time + max_age;
                }
                if (line.find("Expires: ") != std::string::npos) {
                    std::string expires_str = line.substr(9);
                    std::tm tm = {};
                    std::istringstream expires_stream(expires_str);
                    expires_stream >> std::get_time(&tm, "%a, %d %b %Y %H:%M:%S %Z");
                    expires = std::mktime(&tm);
                    expiration_time = expires;
                }
                if (line.find("ETag: ") != std::string::npos) {
                    ETag = line.substr(6);
                }
                if (line.find("Last-Modified: ") != std::string::npos) {
                    std::string last_modified_str = line.substr(15);
                    std::tm tm = {};
                    std::istringstream last_modified_stream(last_modified_str);
                    last_modified_stream >> std::get_time(&tm, "%a, %d %b %Y %H:%M:%S %Z");
                    last_modified = std::mktime(&tm);
                }
            }
            item.expiration_time = expiration_time;
            item.ETag = ETag;
            item.last_modified = last_modified;
            // parse the content
            std::string content;
            while (std::getline(response_stream, line)) {
                content += line + "\n";
            }
        }
    }
    return item;
}