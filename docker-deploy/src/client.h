//
// Created by Evan on 2/24/24.
//
#include <regex>
#include "cache_log.h"
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio.hpp>

#ifndef MY_PROXY_REQUEST_H
#define MY_PROXY_REQUEST_H

struct request_item {
    std::string method;
    std::string url;
    std::string host;
    std::string port;
    std::string first_line;
    std::string content;
    std::string content_type;
};

void send_error_response(int client_fd, int id, int error_occurred);
void handle_client_connection(const std::string& ip, int id, int client_fd);
request_item get_request(int id, int client_fd);
void handle_connect(int id, int client_fd, int server_fd);
void handle_post(int id, int client_fd, int server_fd, const std::string& request);
void handle_get(int id, int client_fd, boost::beast::tcp_stream& stream, request_item & request, cache_item &in_cache);
#endif //MY_PROXY_REQUEST_H
