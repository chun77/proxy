// deals with the response from the server
#include "server.h"
#include "proxy.h"
#include <iomanip>
#include <string>
#include <stdexcept>


// Determines if the response is cacheable based on HTTP headers and status.
bool check_cacheable(int id, cache_item &response) {
    // Check for cacheable status codes
    if (response.status != "200" && response.status != "301") {
        log_writer.write(id, "not cacheable because status code is not 200 or 301");
        return false;
    }

    // Check for no-store directive
    if (response.is_no_store) {
        log_writer.write(id, "not cacheable because no-store");
        return false;
    }

    // Check for private directive
    if (response.is_private) {
        log_writer.write(id, "not cacheable because private");
        return false;
    }

    // Get the current time for comparison
    auto current_time = std::time(nullptr);

    // Handle no-cache directive
    if (response.is_no_cache) {
        // CHANGED: Directly return false if no-cache is present.
        log_writer.write(id, "not cacheable because no-cache");
        return false;
    }

    // Check if the response is stale
    if (response.expiration_time <= current_time) {
        log_writer.write(id, "cached, but requires re-validation");
        response.need_validation = true;
        return true;
    }

    // Check if the response is still fresh
    if (response.expiration_time > current_time) {
        std::string time_str = std::asctime(std::gmtime(&response.expiration_time));
        time_str.pop_back(); // Remove newline character
        log_writer.write(id, "cached, expires at " + time_str);
        return true;
    }

    // Catch-all for any other condition not explicitly handled
    log_writer.write(id, "not cacheable because of control fields error");
    return false;
}


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
        log_writer.write(id, "ERROR: cannot get address info for server host: " + std::string(gai_strerror(status)));
        return -1;
    }

    socket_fd = socket(host_info_list->ai_family, host_info_list->ai_socktype, host_info_list->ai_protocol);
    if (socket_fd == -1) {
        log_writer.write(id, "ERROR: cannot create socket for server");
        freeaddrinfo(host_info_list); // Ensure resources are freed on failure
        return -1;
    }

    status = connect(socket_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
    if (status == -1) {
        log_writer.write(id, "ERROR: cannot connect to socket for server, hostname: " + hostname + ", port: " + port + ", error: " + strerror(errno));
        close(socket_fd); // Close the socket to avoid resource leak
        freeaddrinfo(host_info_list); // Ensure resources are freed on failure
        return -1;
    }

    freeaddrinfo(host_info_list);
    return socket_fd;
}


// get and parse the response from the server
cache_item parse_response(int id, boost::beast::http::response<boost::beast::http::string_body> & response, request_item &request){
    cache_item parsed_response;

    // parse the parsed_response
    //content
    std::ostringstream oss;
    oss << response;
    parsed_response.content = oss.str();

    //first line
    parsed_response.first_line = "HTTP/" + std::to_string(response.version() / 10) + "." + std::to_string(response.version() % 10) +
                                  " " + std::to_string(response.result_int()) +
                                  " " + std::string(response.reason());

    log_writer.write(id, "Received \"" + parsed_response.first_line + "\" from server " + request.host + ":" + request.port);
    //status
    parsed_response.status = std::to_string(response.result_int());
    //response time
    parsed_response.response_time = std::time(nullptr);

    // Expires
    auto it = response.find(boost::beast::http::field::expires);
    if (it != response.end()) {
        std::string expires_str = std::string(it->value());
        if(expires_str != "-1" && expires_str != "0"){
            std::tm tm = {};
            std::istringstream ss(expires_str);
            ss >> std::get_time(&tm, "%a, %d %b %Y %H:%M:%S GMT");
            parsed_response.expiration_time = std::mktime(&tm);
        }
    }

    // ETag
    // Check if the ETag field exists and then assign it to a std::string
    auto etag_header = response.find(boost::beast::http::field::etag);
    if (etag_header != response.end()) {
        parsed_response.ETag = std::string(etag_header->value().data(), etag_header->value().size());
    }

    // Check if the Last-Modified field exists and then assign it to a std::string
    auto last_mod_header = response.find(boost::beast::http::field::last_modified);
    if (last_mod_header != response.end()) {
        parsed_response.last_modified = std::string(last_mod_header->value().data(), last_mod_header->value().size());
    }

    // Cache-Control fields
    auto cache_control = response[boost::beast::http::field::cache_control];

    // max-age
    auto pos = cache_control.find("max-age=");
    if (pos != std::string::npos) {
        auto start = pos + std::strlen("max-age=");
        auto end = cache_control.find(',', start);
        if (end == std::string::npos) {
            end = cache_control.length();
        }
        auto value = cache_control.substr(start, end - start);
        if(!value.empty()) {
            std::string value_str(value.data(), value.size());
            parsed_response.max_age = std::stoi(value_str);
        }

        parsed_response.expiration_time = parsed_response.response_time + parsed_response.max_age;
    }

    // must-revalidate
    if (cache_control.find("must-revalidate") != std::string::npos) {
        parsed_response.is_must_revalidate = true;
    }

    // public
    if (cache_control.find("public") != std::string::npos) {
        parsed_response.is_public = true;
    }
    // no-cache
    if (cache_control.find("no-cache") != std::string::npos) {
        parsed_response.is_no_cache = true;
    }
    // no-store
    if (cache_control.find("no-store") != std::string::npos) {
        parsed_response.is_no_store = true;
    }
    // private
    if (cache_control.find("private") != std::string::npos) {
        parsed_response.is_private = true;
    }

    // Cache-Control fields
    if(parsed_response.content.find("Cache-Control") != std::string::npos)
        log_writer.write(id, "NOTE " + parsed_response.content.substr(parsed_response.content.find("Cache-Control"), parsed_response.content.find("\r\n", parsed_response.content.find("Cache-Control")) - parsed_response.content.find("Cache-Control")));
    // ETAG
    if (!parsed_response.ETag.empty()) {
        log_writer.write(id, "NOTE ETag: " + parsed_response.ETag);
    }
    // Last-Modified
    if (!parsed_response.last_modified.empty()) {
        log_writer.write(id, "NOTE Last-Modified: " + parsed_response.last_modified);
    }
    return parsed_response;
}

void revalidate(int id, int client_fd, boost::beast::tcp_stream& stream, request_item &request, cache_item &in_cache) {
    // Revalidation is required if the cached item is marked with no-cache or has expired.
    // Send the conditional request to the origin server to revalidate the cached item.
    boost::system::error_code ec;
    boost::beast::http::request<boost::beast::http::string_body> req;
    req.version(11);
    req.method(boost::beast::http::verb::get);
    req.target(request.url);
    req.set(boost::beast::http::field::host, request.host + ":" + request.port);
    req.set(boost::beast::http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    req.set(boost::beast::http::field::connection, "close");

    // Use ETag or Last-Modified header for re-validation if available.
    if (!in_cache.ETag.empty()) {
        req.set(boost::beast::http::field::if_none_match, in_cache.ETag);
    } else if (!in_cache.last_modified.empty()) {
        req.set(boost::beast::http::field::if_modified_since, in_cache.last_modified);
    }

    // Send the re-validation request to the server.
    boost::beast::http::write(stream, req, ec);
    if (ec) {
        log_writer.write(id, "ERROR: Failed to send re-validation request to the server: " + ec.message());
        send_error_response(client_fd, id, 502);
        return;
    }
    log_writer.write(id, "Requesting re-validation \"" + request.first_line + "\" from the server");

    // Receive the response from the server.
    boost::beast::flat_buffer response_buff;
    boost::beast::http::response<boost::beast::http::string_body> response;
    boost::beast::http::read(stream, response_buff, response, ec);
    if (ec) {
        log_writer.write(id, "ERROR: Failed to receive the response from the server: " + ec.message());
        send_error_response(client_fd, id, 502);
        return;
    }

    // Parse the response from the server.
    cache_item revalidation_response = parse_response(id, response, request);

    // Check if the response is 304 Not Modified.
    if (revalidation_response.status == "304") {
        // If 304, no need to update the cache. Respond with the cached content.
        log_writer.write(id, "Re-validation successful, no change in content (304 Not Modified).");
        // Respond with the cached content.
        if (send(client_fd, in_cache.content.c_str(), in_cache.content.size(), 0) < 0) {
            log_writer.write(id, "ERROR: Failed to send the cached response to the client.");
        }
        return;
    } else if (revalidation_response.status == "200") {
        // If 200, the content has changed, and we need to update the cache and respond with new content.
        if(check_cacheable(id, revalidation_response)) {
            // Update the cache with the new response.
            cache.update(request.first_line, revalidation_response);
            log_writer.write(id, "Cache updated with the new content from the origin server.");
        } else {
            log_writer.write(id, "New content from the origin server is not cacheable.");
        }
        // Send the new content to the client.
        if (send(client_fd, revalidation_response.content.c_str(), revalidation_response.content.size(), 0) < 0) {
            log_writer.write(id, "ERROR: Failed to send the new response to the client.");
        }
        return;
    } else {
        // If the status code is not 304 or 200, log the error and respond accordingly.
        log_writer.write(id, "Re-validation failed with status code: " + revalidation_response.status);
        send_error_response(client_fd, id, 502); // Or appropriate status code based on the response.
    }
}
