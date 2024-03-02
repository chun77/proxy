//
// Created by Evan on 2/24/24.
//
#include "cache_log.h"
#include "client.h"
#ifndef MY_PROXY_RESPONSE_H
#define MY_PROXY_RESPONSE_H

bool check_cacheable(int id, cache_item &response);
void revalidate(int id, int client_fd, boost::beast::tcp_stream& stream, request_item & request, cache_item &in_cache);
int connect_server(int id, const std::string &hostname, const std::string &port);
// get and parse the response from the server
cache_item parse_response(int id, boost::beast::http::response<boost::beast::http::string_body> &content, request_item &request);
#endif //MY_PROXY_RESPONSE_H
