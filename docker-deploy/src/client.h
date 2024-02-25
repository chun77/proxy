//
// Created by Evan on 2/24/24.
//
#include <regex>

#ifndef MY_PROXY_REQUEST_H
#define MY_PROXY_REQUEST_H


void handle_client_connection(const std::string& ip, int id, int client_fd);
std::string get_request(int id, int client_fd);
void handle_connect(int id, int client_fd, int server_fd);
void handle_post(int id, int client_fd, int server_fd, const std::string& request);
void handle_get(int id, int client_fd, int server_fd, const std::string& request);
#endif //MY_PROXY_REQUEST_H
