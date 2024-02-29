//
// Created by Evan on 2/24/24.
//
#include "cache_log.h"
#include "client.h"
#ifndef MY_PROXY_RESPONSE_H
#define MY_PROXY_RESPONSE_H

int connect_server(int id, const std::string &hostname, const std::string &port);
bool check_cacheable(int id, cache_item &response);
void revalidate(int id, int client_fd, int server_fd, request_item & request, cache_item &in_cache);

// get and parse the response from the server
cache_item get_response(int id, int server_fd, request_item &request);
#endif //MY_PROXY_RESPONSE_H
