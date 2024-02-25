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
        std::cout<<message<<std::endl;
        std::unique_lock<std::shared_mutex> lock(mutex);
        log_file<<message<<std::endl;
    }
};

struct cache_item {
    std::string content;
    std::time_t response_time = 0; // response_time + max_age or Expires
    std::time_t expiration_time = 0;
    std::string ETag;
    std::time_t last_modified = 0;

    // Cache-Control fields
    int max_age = -1; // -1 means not set
    bool is_must_revalidate = false;
    bool is_public = false;
    bool is_no_cache = false;
    bool is_no_store = false;
    bool is_private = false;
    bool is_chunked = false;

    int id = -1;
};

class Cache {
private:
    // mutex for the cache
    mutable std::shared_mutex mutex;
    // key is the URL, value is the cache item
    std::unordered_map<std::string, cache_item> cache;
public:
    void add(const std::string &url, cache_item &response){
        std::unique_lock<std::shared_mutex> lock(mutex);
        cache[url] = response;
    }

    bool is_cached(const std::string &url) const {
        std::shared_lock<std::shared_mutex> lock(mutex);
        return cache.find(url) != cache.end();
    }
};

#endif //MY_PROXY_CACHE_LOG_H
