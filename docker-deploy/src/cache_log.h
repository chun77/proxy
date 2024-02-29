// deal with cache and log
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <ctime>
#include <thread>
#include <iostream>
#include <shared_mutex>
#include <unordered_map>
#include <chrono>
#include <sstream>
#include <vector>
#include <fstream>

#ifndef MY_PROXY_CACHE_LOG_H
#define MY_PROXY_CACHE_LOG_H

class Log {
private:
    std::shared_mutex mutex;
    std::ofstream log_file;
public:
    //initialize the log file
    Log(){
//        log_file.open("/var/log/erss/proxy.log");
        log_file.open("proxy.log");
        if (!log_file.is_open()){
            throw std::runtime_error("cannot open log file");
        }
    }

    // write to log file
    void write(const int &connection_id, const std::string &msg) {
        std::string id = std::to_string(connection_id);
        if (connection_id == -1){
            id = "no-id";
        }
        std::string message = id + ": " + msg;
        std::unique_lock<std::shared_mutex> lock(mutex);
        std::cout<<message<<std::endl;
        log_file<<message<<std::endl;
    }
};

// cache the response
struct cache_item {
    std::string content;
    std::string first_line;
    std::string status; // status code

    std::time_t response_time = 0;
    std::time_t expiration_time = 0; // response_time + max_age or Expires
    std::string ETag;
    std::time_t last_modified = 0;

    // Cache-Control fields
    int max_age = -1; // -1 means not set
    bool is_must_revalidate = false;
    bool is_public = false;
    bool is_no_cache = false;
    bool is_no_store = false;
    bool is_private = false;

    bool need_validation = false;
};

class Cache {
private:
    // mutex for the cache
    mutable std::shared_mutex mutex;
    // key is the first line of the http request (URL included), value is the cache item
    std::unordered_map<std::string, cache_item> cache_list;
public:
    // update the cache
    void update(const std::string &first_line, cache_item &response){
        std::unique_lock<std::shared_mutex> lock(mutex);
        cache_list[first_line] = response;
    }

    // add the response to the cache
    void add(const std::string &first_line, cache_item &response){
        std::unique_lock<std::shared_mutex> lock(mutex);
        cache_list.insert({first_line, response});
    }

    // check if the request is in the cache, return the cache item
    cache_item check(const std::string &first_line) const {
        std::shared_lock<std::shared_mutex> lock(mutex);
        auto it = cache_list.find(first_line);
        if (it == cache_list.end()){
            cache_item empty;
            return empty;
        }
        return it->second;
    }
};

#endif //MY_PROXY_CACHE_LOG_H
