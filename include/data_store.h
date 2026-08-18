#pragma once

#include "redis_clone.h"
#include <vector>
#include <string>
#include <chrono>
#include <unordered_map>

using namespace std;

namespace redis_clone {

class DataStore {
public:
    DataStore() = default;

    // String operations
    bool set(const string& key, const string& value, chrono::milliseconds ttl = chrono::milliseconds(0));
    bool get(const string& key, string& out_val);
    bool del(const string& key);
    bool exists(const string& key);
    vector<string> keys(const string& pattern);
    
    // Hash operations
    bool hset(const string& key, const string& field, const string& value);
    bool hget(const string& key, const string& field, string& out_val);
    bool hdel(const string& key, const string& field);
    vector<pair<string, string>> hgetall(const string& key);
    size_t hlen(const string& key);
    
    // List operations
    size_t lpush(const string& key, const string& value);
    size_t rpush(const string& key, const string& value);
    bool lpop(const string& key, string& out_val);
    bool rpop(const string& key, string& out_val);
    vector<string> lrange(const string& key, int64_t start, int64_t stop);
    size_t llen(const string& key);
    
    // Expiration & TTL
    bool expire(const string& key, chrono::milliseconds ttl);
    int64_t ttl(const string& key);
    bool persist(const string& key);
    void cleanup_expired_keys();

    // Database utilities & Persistence snapshot
    RedisType type(const string& key);
    size_t dbsize();
    void flushall();
    unordered_map<string, KeyEntry> get_all_entries();
    void restore_entries(const unordered_map<string, KeyEntry>& entries);

private:
    unordered_map<string, KeyEntry> store_;
    mutable Mutex mutex_;
    
    bool is_expired(const KeyEntry& entry) const;
};

} // namespace redis_clone
