//
// Created by Evan on 2/24/24.
//
#include "cache_log.h"

#ifndef MY_PROXY_RESPONSE_H
#define MY_PROXY_RESPONSE_H

int connect_server(int id, const std::string &hostname, const std::string &port);
static bool is_cacheable(cache_item &response);

#endif //MY_PROXY_RESPONSE_H
