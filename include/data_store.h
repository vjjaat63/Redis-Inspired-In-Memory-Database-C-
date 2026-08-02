#ifndef DATA_STORE_H
#define DATA_STORE_H

#include "redis_clone.h"
#include <shared_mutex>
#include <atomic>

namespace redis_clone {

class DataStore {
public:
    DataStore();
    ~DataStore();

    // String operations
    bool set(const std::string& key, const RedisValue& value, std::chrono::milliseconds ttl = std::chrono::milliseconds(0));
    std::optional<RedisValue> get(const std::string& key);
    bool del(const std::string& key);
    bool exists(const std::string& key);
    std::vector<std::string> keys(const std::string& pattern);
    
    // Type operations
    RedisType type(const std::string& key);
    
    // Expiration operations
    bool expire(const std::string& key, std::chrono::milliseconds ttl);
    std::optional<std::chrono::milliseconds> ttl(const std::string& key);
    bool persist(const std::string& key);
    
    // Hash operations
    bool hset(const std::string& key, const std::string& field, const RedisValue& value);
    std::optional<RedisValue> hget(const std::string& key, const std::string& field);
    bool hdel(const std::string& key, const std::string& field);
    std::vector<std::string> hkeys(const std::string& key);
    std::vector<RedisValue> hvals(const std::string& key);
    std::vector<std::pair<std::string, RedisValue>> hgetall(const std::string& key);
    bool hexists(const std::string& key, const std::string& field);
    size_t hlen(const std::string& key);
    
    // List operations
    bool lpush(const std::string& key, const RedisValue& value);
    bool rpush(const std::string& key, const RedisValue& value);
    std::optional<RedisValue> lpop(const std::string& key);
    std::optional<RedisValue> rpop(const std::string& key);
    std::vector<RedisValue> lrange(const std::string& key, int64_t start, int64_t stop);
    size_t llen(const std::string& key);
    bool lindex(const std::string& key, int64_t index, RedisValue& out_value);
    
    // Set operations
    bool sadd(const std::string& key, const std::string& member);
    bool srem(const std::string& key, const std::string& member);
    bool sismember(const std::string& key, const std::string& member);
    std::vector<std::string> smembers(const std::string& key);
    size_t scard(const std::string& key);
    
    // Sorted Set operations
    bool zadd(const std::string& key, double score, const std::string& member);
    bool zrem(const std::string& key, const std::string& member);
    std::optional<double> zscore(const std::string& key, const std::string& member);
    std::vector<std::string> zrange(const std::string& key, int64_t start, int64_t stop);
    std::vector<std::string> zrevrange(const std::string& key, int64_t start, int64_t stop);
    size_t zcard(const std::string& key);
    
    // Utility operations
    size_t dbsize();
    void flushdb();
    void flushall();
    
    // Cleanup expired keys
    void cleanup_expired_keys();
    
    // Dump all non-expired entries for persistence
    std::vector<std::pair<std::string, KeyEntry>> get_all_valid_entries();

private:
    std::unordered_map<std::string, KeyEntry> store_;
    mutable std::shared_mutex mutex_;
    
    bool is_expired(const KeyEntry& entry) const;
    void remove_if_expired(const std::string& key);
};

} // namespace redis_clone

#endif // DATA_STORE_H
