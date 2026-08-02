#ifndef REDIS_CLONE_H
#define REDIS_CLONE_H

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <variant>
#include <vector>
#include <set>
#include <list>
#include <deque>
#include <map>

namespace redis_clone {

// Forward declarations
class RedisServer;
class CommandExecutor;
class RESPParser;
class DataStore;

// Custom Sorted Set structure with score-first ordering and member lookup
struct ZSet {
    std::set<std::pair<double, std::string>> score_set;
    std::unordered_map<std::string, double> member_map;

    bool operator==(const ZSet& other) const {
        return score_set == other.score_set && member_map == other.member_map;
    }
};

// Type aliases
using RedisValue = std::variant<
    std::nullptr_t,
    std::string,
    int64_t,
    std::vector<RedisValue>,
    std::unordered_map<std::string, RedisValue>,
    std::set<std::string>,
    std::list<std::string>,
    ZSet
>;

// Redis data types
enum class RedisType {
    STRING,
    HASH,
    LIST,
    SET,
    SORTED_SET,
    NONE
};

// Key with expiration
struct KeyEntry {
    RedisValue value;
    RedisType type;
    std::chrono::system_clock::time_point expiry_time;
    bool has_expiry;
};

} // namespace redis_clone

#endif // REDIS_CLONE_H
