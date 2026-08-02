#ifndef COMMAND_EXECUTOR_H
#define COMMAND_EXECUTOR_H

#include "redis_clone.h"
#include "data_store.h"
#include <memory>
#include <string>
#include <vector>
#include <functional>

namespace redis_clone {

class CommandExecutor {
public:
    explicit CommandExecutor(std::shared_ptr<DataStore> data_store);
    
    // Execute a command and return the result
    RedisValue execute(const std::string& command, const std::vector<RedisValue>& args);
    
    // Get list of all supported commands
    std::vector<std::string> get_supported_commands() const;

private:
    std::shared_ptr<DataStore> data_store_;
    
    // Command handlers
    RedisValue handle_ping(const std::vector<RedisValue>& args);
    RedisValue handle_set(const std::vector<RedisValue>& args);
    RedisValue handle_get(const std::vector<RedisValue>& args);
    RedisValue handle_del(const std::vector<RedisValue>& args);
    RedisValue handle_exists(const std::vector<RedisValue>& args);
    RedisValue handle_keys(const std::vector<RedisValue>& args);
    RedisValue handle_type(const std::vector<RedisValue>& args);
    RedisValue handle_expire(const std::vector<RedisValue>& args);
    RedisValue handle_ttl(const std::vector<RedisValue>& args);
    RedisValue handle_persist(const std::vector<RedisValue>& args);
    RedisValue handle_flushdb(const std::vector<RedisValue>& args);
    RedisValue handle_flushall(const std::vector<RedisValue>& args);
    RedisValue handle_dbsize(const std::vector<RedisValue>& args);
    
    // Hash commands
    RedisValue handle_hset(const std::vector<RedisValue>& args);
    RedisValue handle_hget(const std::vector<RedisValue>& args);
    RedisValue handle_hdel(const std::vector<RedisValue>& args);
    RedisValue handle_hkeys(const std::vector<RedisValue>& args);
    RedisValue handle_hvals(const std::vector<RedisValue>& args);
    RedisValue handle_hgetall(const std::vector<RedisValue>& args);
    RedisValue handle_hexists(const std::vector<RedisValue>& args);
    RedisValue handle_hlen(const std::vector<RedisValue>& args);
    
    // List commands
    RedisValue handle_lpush(const std::vector<RedisValue>& args);
    RedisValue handle_rpush(const std::vector<RedisValue>& args);
    RedisValue handle_lpop(const std::vector<RedisValue>& args);
    RedisValue handle_rpop(const std::vector<RedisValue>& args);
    RedisValue handle_lrange(const std::vector<RedisValue>& args);
    RedisValue handle_llen(const std::vector<RedisValue>& args);
    RedisValue handle_lindex(const std::vector<RedisValue>& args);
    
    // Set commands
    RedisValue handle_sadd(const std::vector<RedisValue>& args);
    RedisValue handle_srem(const std::vector<RedisValue>& args);
    RedisValue handle_sismember(const std::vector<RedisValue>& args);
    RedisValue handle_smembers(const std::vector<RedisValue>& args);
    RedisValue handle_scard(const std::vector<RedisValue>& args);
    
    // Sorted set commands
    RedisValue handle_zadd(const std::vector<RedisValue>& args);
    RedisValue handle_zrem(const std::vector<RedisValue>& args);
    RedisValue handle_zscore(const std::vector<RedisValue>& args);
    RedisValue handle_zrange(const std::vector<RedisValue>& args);
    RedisValue handle_zrevrange(const std::vector<RedisValue>& args);
    RedisValue handle_zcard(const std::vector<RedisValue>& args);
    
    // Helper functions
    std::string redis_value_to_string(const RedisValue& value);
    int64_t redis_value_to_int(const RedisValue& value);
    double redis_value_to_double(const RedisValue& value);
    std::string to_lower(const std::string& str);
};

} // namespace redis_clone

#endif // COMMAND_EXECUTOR_H
