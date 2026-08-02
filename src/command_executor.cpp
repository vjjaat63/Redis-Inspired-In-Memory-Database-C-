#include "command_executor.h"
#include "resp_parser.h"
#include <algorithm>
#include <sstream>

namespace redis_clone {

CommandExecutor::CommandExecutor(std::shared_ptr<DataStore> data_store)
    : data_store_(data_store) {
}

RedisValue CommandExecutor::execute(const std::string& command, const std::vector<RedisValue>& args) {
    std::string cmd = to_lower(command);
    
    if (cmd == "ping") return handle_ping(args);
    if (cmd == "set") return handle_set(args);
    if (cmd == "get") return handle_get(args);
    if (cmd == "del") return handle_del(args);
    if (cmd == "exists") return handle_exists(args);
    if (cmd == "keys") return handle_keys(args);
    if (cmd == "type") return handle_type(args);
    if (cmd == "expire") return handle_expire(args);
    if (cmd == "ttl") return handle_ttl(args);
    if (cmd == "persist") return handle_persist(args);
    if (cmd == "flushdb") return handle_flushdb(args);
    if (cmd == "flushall") return handle_flushall(args);
    if (cmd == "dbsize") return handle_dbsize(args);
    
    // Hash commands
    if (cmd == "hset") return handle_hset(args);
    if (cmd == "hget") return handle_hget(args);
    if (cmd == "hdel") return handle_hdel(args);
    if (cmd == "hkeys") return handle_hkeys(args);
    if (cmd == "hvals") return handle_hvals(args);
    if (cmd == "hgetall") return handle_hgetall(args);
    if (cmd == "hexists") return handle_hexists(args);
    if (cmd == "hlen") return handle_hlen(args);
    
    // List commands
    if (cmd == "lpush") return handle_lpush(args);
    if (cmd == "rpush") return handle_rpush(args);
    if (cmd == "lpop") return handle_lpop(args);
    if (cmd == "rpop") return handle_rpop(args);
    if (cmd == "lrange") return handle_lrange(args);
    if (cmd == "llen") return handle_llen(args);
    if (cmd == "lindex") return handle_lindex(args);
    
    // Set commands
    if (cmd == "sadd") return handle_sadd(args);
    if (cmd == "srem") return handle_srem(args);
    if (cmd == "sismember") return handle_sismember(args);
    if (cmd == "smembers") return handle_smembers(args);
    if (cmd == "scard") return handle_scard(args);
    
    // Sorted set commands
    if (cmd == "zadd") return handle_zadd(args);
    if (cmd == "zrem") return handle_zrem(args);
    if (cmd == "zscore") return handle_zscore(args);
    if (cmd == "zrange") return handle_zrange(args);
    if (cmd == "zrevrange") return handle_zrevrange(args);
    if (cmd == "zcard") return handle_zcard(args);
    
    // Unknown command
    return std::string("ERR unknown command '" + command + "'");
}

std::vector<std::string> CommandExecutor::get_supported_commands() const {
    return {
        "ping", "set", "get", "del", "exists", "keys", "type", "expire", "ttl", "persist",
        "flushdb", "flushall", "dbsize",
        "hset", "hget", "hdel", "hkeys", "hvals", "hgetall", "hexists", "hlen",
        "lpush", "rpush", "lpop", "rpop", "lrange", "llen", "lindex",
        "sadd", "srem", "sismember", "smembers", "scard",
        "zadd", "zrem", "zscore", "zrange", "zrevrange", "zcard"
    };
}

RedisValue CommandExecutor::handle_ping(const std::vector<RedisValue>& args) {
    if (args.empty()) {
        return std::string("PONG");
    }
    return redis_value_to_string(args[0]);
}

RedisValue CommandExecutor::handle_set(const std::vector<RedisValue>& args) {
    if (args.size() < 2) {
        return std::string("ERR wrong number of arguments for 'set' command");
    }
    
    std::string key = redis_value_to_string(args[0]);
    RedisValue value = args[1];
    std::chrono::milliseconds ttl(0);
    
    // Parse optional EX argument
    for (size_t i = 2; i < args.size(); ++i) {
        std::string arg = to_lower(redis_value_to_string(args[i]));
        if (arg == "ex" && i + 1 < args.size()) {
            ttl = std::chrono::milliseconds(redis_value_to_int(args[i + 1]) * 1000);
            i++;
        }
    }
    
    bool result = data_store_->set(key, value, ttl);
    return result ? std::string("OK") : std::string("ERR set failed");
}

RedisValue CommandExecutor::handle_get(const std::vector<RedisValue>& args) {
    if (args.size() != 1) {
        return std::string("ERR wrong number of arguments for 'get' command");
    }
    
    std::string key = redis_value_to_string(args[0]);
    auto result = data_store_->get(key);
    
    if (!result) {
        return std::nullptr_t{};
    }
    
    return *result;
}

RedisValue CommandExecutor::handle_del(const std::vector<RedisValue>& args) {
    if (args.empty()) {
        return std::string("ERR wrong number of arguments for 'del' command");
    }
    
    int64_t count = 0;
    for (const auto& arg : args) {
        std::string key = redis_value_to_string(arg);
        if (data_store_->del(key)) {
            count++;
        }
    }
    
    return count;
}

RedisValue CommandExecutor::handle_exists(const std::vector<RedisValue>& args) {
    if (args.empty()) {
        return std::string("ERR wrong number of arguments for 'exists' command");
    }
    
    int64_t count = 0;
    for (const auto& arg : args) {
        std::string key = redis_value_to_string(arg);
        if (data_store_->exists(key)) {
            count++;
        }
    }
    
    return count;
}

RedisValue CommandExecutor::handle_keys(const std::vector<RedisValue>& args) {
    if (args.size() != 1) {
        return std::string("ERR wrong number of arguments for 'keys' command");
    }
    
    std::string pattern = redis_value_to_string(args[0]);
    auto keys = data_store_->keys(pattern);
    
    std::vector<RedisValue> result;
    for (const auto& key : keys) {
        result.push_back(key);
    }
    
    return result;
}

RedisValue CommandExecutor::handle_type(const std::vector<RedisValue>& args) {
    if (args.size() != 1) {
        return std::string("ERR wrong number of arguments for 'type' command");
    }
    
    std::string key = redis_value_to_string(args[0]);
    RedisType type = data_store_->type(key);
    
    switch (type) {
        case RedisType::STRING: return std::string("string");
        case RedisType::HASH: return std::string("hash");
        case RedisType::LIST: return std::string("list");
        case RedisType::SET: return std::string("set");
        case RedisType::SORTED_SET: return std::string("zset");
        default: return std::string("none");
    }
}

RedisValue CommandExecutor::handle_expire(const std::vector<RedisValue>& args) {
    if (args.size() != 2) {
        return std::string("ERR wrong number of arguments for 'expire' command");
    }
    
    std::string key = redis_value_to_string(args[0]);
    int64_t seconds = redis_value_to_int(args[1]);
    
    bool result = data_store_->expire(key, std::chrono::milliseconds(seconds * 1000));
    return result ? int64_t(1) : int64_t(0);
}

RedisValue CommandExecutor::handle_ttl(const std::vector<RedisValue>& args) {
    if (args.size() != 1) {
        return std::string("ERR wrong number of arguments for 'ttl' command");
    }
    
    std::string key = redis_value_to_string(args[0]);
    auto ttl = data_store_->ttl(key);
    
    if (!ttl) {
        return int64_t(-2);
    }
    
    return int64_t(ttl->count() / 1000);
}

RedisValue CommandExecutor::handle_persist(const std::vector<RedisValue>& args) {
    if (args.size() != 1) {
        return std::string("ERR wrong number of arguments for 'persist' command");
    }
    
    std::string key = redis_value_to_string(args[0]);
    bool result = data_store_->persist(key);
    return result ? int64_t(1) : int64_t(0);
}

RedisValue CommandExecutor::handle_flushdb(const std::vector<RedisValue>& args) {
    data_store_->flushdb();
    return std::string("OK");
}

RedisValue CommandExecutor::handle_flushall(const std::vector<RedisValue>& args) {
    data_store_->flushall();
    return std::string("OK");
}

RedisValue CommandExecutor::handle_dbsize(const std::vector<RedisValue>& args) {
    return int64_t(data_store_->dbsize());
}

RedisValue CommandExecutor::handle_hset(const std::vector<RedisValue>& args) {
    if (args.size() < 3) {
        return std::string("ERR wrong number of arguments for 'hset' command");
    }
    
    std::string key = redis_value_to_string(args[0]);
    std::string field = redis_value_to_string(args[1]);
    RedisValue value = args[2];
    
    bool result = data_store_->hset(key, field, value);
    return result ? int64_t(1) : int64_t(0);
}

RedisValue CommandExecutor::handle_hget(const std::vector<RedisValue>& args) {
    if (args.size() != 2) {
        return std::string("ERR wrong number of arguments for 'hget' command");
    }
    
    std::string key = redis_value_to_string(args[0]);
    std::string field = redis_value_to_string(args[1]);
    
    auto result = data_store_->hget(key, field);
    if (!result) {
        return std::nullptr_t{};
    }
    
    return *result;
}

RedisValue CommandExecutor::handle_hdel(const std::vector<RedisValue>& args) {
    if (args.size() < 2) {
        return std::string("ERR wrong number of arguments for 'hdel' command");
    }
    
    std::string key = redis_value_to_string(args[0]);
    int64_t count = 0;
    
    for (size_t i = 1; i < args.size(); ++i) {
        std::string field = redis_value_to_string(args[i]);
        if (data_store_->hdel(key, field)) {
            count++;
        }
    }
    
    return count;
}

RedisValue CommandExecutor::handle_hkeys(const std::vector<RedisValue>& args) {
    if (args.size() != 1) {
        return std::string("ERR wrong number of arguments for 'hkeys' command");
    }
    
    std::string key = redis_value_to_string(args[0]);
    auto keys = data_store_->hkeys(key);
    
    std::vector<RedisValue> result;
    for (const auto& k : keys) {
        result.push_back(k);
    }
    
    return result;
}

RedisValue CommandExecutor::handle_hvals(const std::vector<RedisValue>& args) {
    if (args.size() != 1) {
        return std::string("ERR wrong number of arguments for 'hvals' command");
    }
    
    std::string key = redis_value_to_string(args[0]);
    auto vals = data_store_->hvals(key);
    
    return vals;
}

RedisValue CommandExecutor::handle_hgetall(const std::vector<RedisValue>& args) {
    if (args.size() != 1) {
        return std::string("ERR wrong number of arguments for 'hgetall' command");
    }
    
    std::string key = redis_value_to_string(args[0]);
    auto pairs = data_store_->hgetall(key);
    
    std::vector<RedisValue> result;
    for (const auto& [field, value] : pairs) {
        result.push_back(field);
        result.push_back(value);
    }
    
    return result;
}

RedisValue CommandExecutor::handle_hexists(const std::vector<RedisValue>& args) {
    if (args.size() != 2) {
        return std::string("ERR wrong number of arguments for 'hexists' command");
    }
    
    std::string key = redis_value_to_string(args[0]);
    std::string field = redis_value_to_string(args[1]);
    
    bool result = data_store_->hexists(key, field);
    return result ? int64_t(1) : int64_t(0);
}

RedisValue CommandExecutor::handle_hlen(const std::vector<RedisValue>& args) {
    if (args.size() != 1) {
        return std::string("ERR wrong number of arguments for 'hlen' command");
    }
    
    std::string key = redis_value_to_string(args[0]);
    return int64_t(data_store_->hlen(key));
}

RedisValue CommandExecutor::handle_lpush(const std::vector<RedisValue>& args) {
    if (args.size() < 2) {
        return std::string("ERR wrong number of arguments for 'lpush' command");
    }
    
    std::string key = redis_value_to_string(args[0]);
    int64_t count = 0;
    
    for (size_t i = 1; i < args.size(); ++i) {
        if (data_store_->lpush(key, args[i])) {
            count++;
        }
    }
    
    return count;
}

RedisValue CommandExecutor::handle_rpush(const std::vector<RedisValue>& args) {
    if (args.size() < 2) {
        return std::string("ERR wrong number of arguments for 'rpush' command");
    }
    
    std::string key = redis_value_to_string(args[0]);
    int64_t count = 0;
    
    for (size_t i = 1; i < args.size(); ++i) {
        if (data_store_->rpush(key, args[i])) {
            count++;
        }
    }
    
    return count;
}

RedisValue CommandExecutor::handle_lpop(const std::vector<RedisValue>& args) {
    if (args.size() != 1) {
        return std::string("ERR wrong number of arguments for 'lpop' command");
    }
    
    std::string key = redis_value_to_string(args[0]);
    auto result = data_store_->lpop(key);
    
    if (!result) {
        return std::nullptr_t{};
    }
    
    return *result;
}

RedisValue CommandExecutor::handle_rpop(const std::vector<RedisValue>& args) {
    if (args.size() != 1) {
        return std::string("ERR wrong number of arguments for 'rpop' command");
    }
    
    std::string key = redis_value_to_string(args[0]);
    auto result = data_store_->rpop(key);
    
    if (!result) {
        return std::nullptr_t{};
    }
    
    return *result;
}

RedisValue CommandExecutor::handle_lrange(const std::vector<RedisValue>& args) {
    if (args.size() != 3) {
        return std::string("ERR wrong number of arguments for 'lrange' command");
    }
    
    std::string key = redis_value_to_string(args[0]);
    int64_t start = redis_value_to_int(args[1]);
    int64_t stop = redis_value_to_int(args[2]);
    
    return data_store_->lrange(key, start, stop);
}

RedisValue CommandExecutor::handle_llen(const std::vector<RedisValue>& args) {
    if (args.size() != 1) {
        return std::string("ERR wrong number of arguments for 'llen' command");
    }
    
    std::string key = redis_value_to_string(args[0]);
    return int64_t(data_store_->llen(key));
}

RedisValue CommandExecutor::handle_lindex(const std::vector<RedisValue>& args) {
    if (args.size() != 2) {
        return std::string("ERR wrong number of arguments for 'lindex' command");
    }
    
    std::string key = redis_value_to_string(args[0]);
    int64_t index = redis_value_to_int(args[1]);
    
    RedisValue value;
    if (data_store_->lindex(key, index, value)) {
        return value;
    }
    
    return std::nullptr_t{};
}

RedisValue CommandExecutor::handle_sadd(const std::vector<RedisValue>& args) {
    if (args.size() < 2) {
        return std::string("ERR wrong number of arguments for 'sadd' command");
    }
    
    std::string key = redis_value_to_string(args[0]);
    int64_t count = 0;
    
    for (size_t i = 1; i < args.size(); ++i) {
        std::string member = redis_value_to_string(args[i]);
        if (data_store_->sadd(key, member)) {
            count++;
        }
    }
    
    return count;
}

RedisValue CommandExecutor::handle_srem(const std::vector<RedisValue>& args) {
    if (args.size() < 2) {
        return std::string("ERR wrong number of arguments for 'srem' command");
    }
    
    std::string key = redis_value_to_string(args[0]);
    int64_t count = 0;
    
    for (size_t i = 1; i < args.size(); ++i) {
        std::string member = redis_value_to_string(args[i]);
        if (data_store_->srem(key, member)) {
            count++;
        }
    }
    
    return count;
}

RedisValue CommandExecutor::handle_sismember(const std::vector<RedisValue>& args) {
    if (args.size() != 2) {
        return std::string("ERR wrong number of arguments for 'sismember' command");
    }
    
    std::string key = redis_value_to_string(args[0]);
    std::string member = redis_value_to_string(args[1]);
    
    bool result = data_store_->sismember(key, member);
    return result ? int64_t(1) : int64_t(0);
}

RedisValue CommandExecutor::handle_smembers(const std::vector<RedisValue>& args) {
    if (args.size() != 1) {
        return std::string("ERR wrong number of arguments for 'smembers' command");
    }
    
    std::string key = redis_value_to_string(args[0]);
    auto members = data_store_->smembers(key);
    
    std::vector<RedisValue> result;
    for (const auto& member : members) {
        result.push_back(member);
    }
    
    return result;
}

RedisValue CommandExecutor::handle_scard(const std::vector<RedisValue>& args) {
    if (args.size() != 1) {
        return std::string("ERR wrong number of arguments for 'scard' command");
    }
    
    std::string key = redis_value_to_string(args[0]);
    return int64_t(data_store_->scard(key));
}

RedisValue CommandExecutor::handle_zadd(const std::vector<RedisValue>& args) {
    if (args.size() < 3) {
        return std::string("ERR wrong number of arguments for 'zadd' command");
    }
    
    std::string key = redis_value_to_string(args[0]);
    int64_t count = 0;
    
    for (size_t i = 1; i + 1 < args.size(); i += 2) {
        double score = redis_value_to_double(args[i]);
        std::string member = redis_value_to_string(args[i + 1]);
        if (data_store_->zadd(key, score, member)) {
            count++;
        }
    }
    
    return count;
}

RedisValue CommandExecutor::handle_zrem(const std::vector<RedisValue>& args) {
    if (args.size() < 2) {
        return std::string("ERR wrong number of arguments for 'zrem' command");
    }
    
    std::string key = redis_value_to_string(args[0]);
    int64_t count = 0;
    
    for (size_t i = 1; i < args.size(); ++i) {
        std::string member = redis_value_to_string(args[i]);
        if (data_store_->zrem(key, member)) {
            count++;
        }
    }
    
    return count;
}

RedisValue CommandExecutor::handle_zscore(const std::vector<RedisValue>& args) {
    if (args.size() != 2) {
        return std::string("ERR wrong number of arguments for 'zscore' command");
    }
    
    std::string key = redis_value_to_string(args[0]);
    std::string member = redis_value_to_string(args[1]);
    
    auto score = data_store_->zscore(key, member);
    if (!score) {
        return std::nullptr_t{};
    }
    
    return std::to_string(*score);
}

RedisValue CommandExecutor::handle_zrange(const std::vector<RedisValue>& args) {
    if (args.size() < 3) {
        return std::string("ERR wrong number of arguments for 'zrange' command");
    }
    
    std::string key = redis_value_to_string(args[0]);
    int64_t start = redis_value_to_int(args[1]);
    int64_t stop = redis_value_to_int(args[2]);
    
    auto members = data_store_->zrange(key, start, stop);
    
    std::vector<RedisValue> result;
    for (const auto& member : members) {
        result.push_back(member);
    }
    
    return result;
}

RedisValue CommandExecutor::handle_zrevrange(const std::vector<RedisValue>& args) {
    if (args.size() < 3) {
        return std::string("ERR wrong number of arguments for 'zrevrange' command");
    }
    
    std::string key = redis_value_to_string(args[0]);
    int64_t start = redis_value_to_int(args[1]);
    int64_t stop = redis_value_to_int(args[2]);
    
    auto members = data_store_->zrevrange(key, start, stop);
    
    std::vector<RedisValue> result;
    for (const auto& member : members) {
        result.push_back(member);
    }
    
    return result;
}

RedisValue CommandExecutor::handle_zcard(const std::vector<RedisValue>& args) {
    if (args.size() != 1) {
        return std::string("ERR wrong number of arguments for 'zcard' command");
    }
    
    std::string key = redis_value_to_string(args[0]);
    return int64_t(data_store_->zcard(key));
}

std::string CommandExecutor::redis_value_to_string(const RedisValue& value) {
    if (std::holds_alternative<std::string>(value)) {
        return std::get<std::string>(value);
    } else if (std::holds_alternative<int64_t>(value)) {
        return std::to_string(std::get<int64_t>(value));
    }
    return "";
}

int64_t CommandExecutor::redis_value_to_int(const RedisValue& value) {
    if (std::holds_alternative<int64_t>(value)) {
        return std::get<int64_t>(value);
    } else if (std::holds_alternative<std::string>(value)) {
        try {
            return std::stoll(std::get<std::string>(value));
        } catch (...) {
            return 0;
        }
    }
    return 0;
}

double CommandExecutor::redis_value_to_double(const RedisValue& value) {
    if (std::holds_alternative<int64_t>(value)) {
        return static_cast<double>(std::get<int64_t>(value));
    } else if (std::holds_alternative<std::string>(value)) {
        try {
            return std::stod(std::get<std::string>(value));
        } catch (...) {
            return 0.0;
        }
    }
    return 0.0;
}

std::string CommandExecutor::to_lower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

} // namespace redis_clone
