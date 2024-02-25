//
// Created by Evan on 2/24/24.
//
#include "cache_log.h"

#ifndef MY_PROXY_RESPONSE_H
#define MY_PROXY_RESPONSE_H

int connect_server(int id, const std::string &hostname, const std::string &port);
static bool is_cacheable(cache_item &response);
static bool is_valid(cache_item &response);
cache_item server_response_parse(const std::string &response, const std::string &url);
#endif //MY_PROXY_RESPONSE_H
