#include "data_store.h"
#include <algorithm>
#include <sstream>

namespace redis_clone {

DataStore::DataStore() {}

DataStore::~DataStore() {}

bool DataStore::set(const std::string& key, const RedisValue& value, std::chrono::milliseconds ttl) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    KeyEntry entry;
    entry.value = value;
    entry.has_expiry = ttl.count() > 0;
    
    if (entry.has_expiry) {
        entry.expiry_time = std::chrono::system_clock::now() + ttl;
    }
    
    // Determine type
    if (std::holds_alternative<std::string>(value)) {
        entry.type = RedisType::STRING;
    } else if (std::holds_alternative<std::unordered_map<std::string, RedisValue>>(value)) {
        entry.type = RedisType::HASH;
    } else if (std::holds_alternative<std::list<std::string>>(value)) {
        entry.type = RedisType::LIST;
    } else if (std::holds_alternative<std::set<std::string>>(value)) {
        entry.type = RedisType::SET;
    } else if (std::holds_alternative<ZSet>(value)) {
        entry.type = RedisType::SORTED_SET;
    } else {
        entry.type = RedisType::NONE;
    }
    
    store_[key] = entry;
    return true;
}

std::optional<RedisValue> DataStore::get(const std::string& key) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    auto it = store_.find(key);
    if (it == store_.end()) {
        return std::nullopt;
    }
    
    if (is_expired(it->second)) {
        return std::nullopt;
    }
    
    return it->second.value;
}

bool DataStore::del(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    auto it = store_.find(key);
    if (it == store_.end()) {
        return false;
    }
    
    store_.erase(it);
    return true;
}

bool DataStore::exists(const std::string& key) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    auto it = store_.find(key);
    if (it == store_.end()) {
        return false;
    }
    
    return !is_expired(it->second);
}

std::vector<std::string> DataStore::keys(const std::string& pattern) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    std::vector<std::string> result;
    
    for (const auto& [key, entry] : store_) {
        if (is_expired(entry)) {
            continue;
        }
        
        if (pattern == "*" || key == pattern) {
            result.push_back(key);
        }
    }
    
    return result;
}

RedisType DataStore::type(const std::string& key) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    auto it = store_.find(key);
    if (it == store_.end()) {
        return RedisType::NONE;
    }
    
    if (is_expired(it->second)) {
        return RedisType::NONE;
    }
    
    return it->second.type;
}

bool DataStore::expire(const std::string& key, std::chrono::milliseconds ttl) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    auto it = store_.find(key);
    if (it == store_.end()) {
        return false;
    }
    
    if (is_expired(it->second)) {
        return false;
    }
    
    it->second.has_expiry = true;
    it->second.expiry_time = std::chrono::system_clock::now() + ttl;
    return true;
}

std::optional<std::chrono::milliseconds> DataStore::ttl(const std::string& key) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    auto it = store_.find(key);
    if (it == store_.end()) {
        return std::nullopt;
    }
    
    if (is_expired(it->second)) {
        return std::nullopt;
    }
    
    if (!it->second.has_expiry) {
        return std::chrono::milliseconds(-1);
    }
    
    auto now = std::chrono::system_clock::now();
    auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
        it->second.expiry_time - now
    );
    
    if (remaining.count() <= 0) {
        return std::chrono::milliseconds(-2);
    }
    
    return remaining;
}

bool DataStore::persist(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    auto it = store_.find(key);
    if (it == store_.end()) {
        return false;
    }
    
    if (is_expired(it->second)) {
        return false;
    }
    
    if (!it->second.has_expiry) {
        return false;
    }
    
    it->second.has_expiry = false;
    return true;
}

bool DataStore::hset(const std::string& key, const std::string& field, const RedisValue& value) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    auto it = store_.find(key);
    if (it == store_.end() || is_expired(it->second)) {
        // Create new hash
        KeyEntry entry;
        entry.type = RedisType::HASH;
        entry.has_expiry = false;
        entry.value = std::unordered_map<std::string, RedisValue>{{field, value}};
        store_[key] = entry;
        return true;
    }
    
    if (it->second.type != RedisType::HASH) {
        return false;
    }
    
    auto& hash = std::get<std::unordered_map<std::string, RedisValue>>(it->second.value);
    hash[field] = value;
    return true;
}

std::optional<RedisValue> DataStore::hget(const std::string& key, const std::string& field) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    auto it = store_.find(key);
    if (it == store_.end() || is_expired(it->second)) {
        return std::nullopt;
    }
    
    if (it->second.type != RedisType::HASH) {
        return std::nullopt;
    }
    
    auto& hash = std::get<std::unordered_map<std::string, RedisValue>>(it->second.value);
    auto field_it = hash.find(field);
    if (field_it == hash.end()) {
        return std::nullopt;
    }
    
    return field_it->second;
}

bool DataStore::hdel(const std::string& key, const std::string& field) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    auto it = store_.find(key);
    if (it == store_.end() || is_expired(it->second)) {
        return false;
    }
    
    if (it->second.type != RedisType::HASH) {
        return false;
    }
    
    auto& hash = std::get<std::unordered_map<std::string, RedisValue>>(it->second.value);
    return hash.erase(field) > 0;
}

std::vector<std::string> DataStore::hkeys(const std::string& key) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    std::vector<std::string> result;
    
    auto it = store_.find(key);
    if (it == store_.end() || is_expired(it->second)) {
        return result;
    }
    
    if (it->second.type != RedisType::HASH) {
        return result;
    }
    
    auto& hash = std::get<std::unordered_map<std::string, RedisValue>>(it->second.value);
    for (const auto& [field, _] : hash) {
        result.push_back(field);
    }
    
    return result;
}

std::vector<RedisValue> DataStore::hvals(const std::string& key) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    std::vector<RedisValue> result;
    
    auto it = store_.find(key);
    if (it == store_.end() || is_expired(it->second)) {
        return result;
    }
    
    if (it->second.type != RedisType::HASH) {
        return result;
    }
    
    auto& hash = std::get<std::unordered_map<std::string, RedisValue>>(it->second.value);
    for (const auto& [_, value] : hash) {
        result.push_back(value);
    }
    
    return result;
}

std::vector<std::pair<std::string, RedisValue>> DataStore::hgetall(const std::string& key) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    std::vector<std::pair<std::string, RedisValue>> result;
    
    auto it = store_.find(key);
    if (it == store_.end() || is_expired(it->second)) {
        return result;
    }
    
    if (it->second.type != RedisType::HASH) {
        return result;
    }
    
    auto& hash = std::get<std::unordered_map<std::string, RedisValue>>(it->second.value);
    for (const auto& [field, value] : hash) {
        result.emplace_back(field, value);
    }
    
    return result;
}

bool DataStore::hexists(const std::string& key, const std::string& field) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    auto it = store_.find(key);
    if (it == store_.end() || is_expired(it->second)) {
        return false;
    }
    
    if (it->second.type != RedisType::HASH) {
        return false;
    }
    
    auto& hash = std::get<std::unordered_map<std::string, RedisValue>>(it->second.value);
    return hash.find(field) != hash.end();
}

size_t DataStore::hlen(const std::string& key) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    auto it = store_.find(key);
    if (it == store_.end() || is_expired(it->second)) {
        return 0;
    }
    
    if (it->second.type != RedisType::HASH) {
        return 0;
    }
    
    auto& hash = std::get<std::unordered_map<std::string, RedisValue>>(it->second.value);
    return hash.size();
}

bool DataStore::lpush(const std::string& key, const RedisValue& value) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    auto it = store_.find(key);
    if (it == store_.end() || is_expired(it->second)) {
        // Create new list
        KeyEntry entry;
        entry.type = RedisType::LIST;
        entry.has_expiry = false;
        std::string str_value = std::holds_alternative<std::string>(value) ? 
            std::get<std::string>(value) : std::to_string(std::get<int64_t>(value));
        entry.value = std::list<std::string>{str_value};
        store_[key] = entry;
        return true;
    }
    
    if (it->second.type != RedisType::LIST) {
        return false;
    }
    
    auto& list = std::get<std::list<std::string>>(it->second.value);
    std::string str_value = std::holds_alternative<std::string>(value) ? 
        std::get<std::string>(value) : std::to_string(std::get<int64_t>(value));
    list.push_front(str_value);
    return true;
}

bool DataStore::rpush(const std::string& key, const RedisValue& value) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    auto it = store_.find(key);
    if (it == store_.end() || is_expired(it->second)) {
        // Create new list
        KeyEntry entry;
        entry.type = RedisType::LIST;
        entry.has_expiry = false;
        std::string str_value = std::holds_alternative<std::string>(value) ? 
            std::get<std::string>(value) : std::to_string(std::get<int64_t>(value));
        entry.value = std::list<std::string>{str_value};
        store_[key] = entry;
        return true;
    }
    
    if (it->second.type != RedisType::LIST) {
        return false;
    }
    
    auto& list = std::get<std::list<std::string>>(it->second.value);
    std::string str_value = std::holds_alternative<std::string>(value) ? 
        std::get<std::string>(value) : std::to_string(std::get<int64_t>(value));
    list.push_back(str_value);
    return true;
}

std::optional<RedisValue> DataStore::lpop(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    auto it = store_.find(key);
    if (it == store_.end() || is_expired(it->second)) {
        return std::nullopt;
    }
    
    if (it->second.type != RedisType::LIST) {
        return std::nullopt;
    }
    
    auto& list = std::get<std::list<std::string>>(it->second.value);
    if (list.empty()) {
        return std::nullopt;
    }
    
    std::string value = list.front();
    list.pop_front();
    return value;
}

std::optional<RedisValue> DataStore::rpop(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    auto it = store_.find(key);
    if (it == store_.end() || is_expired(it->second)) {
        return std::nullopt;
    }
    
    if (it->second.type != RedisType::LIST) {
        return std::nullopt;
    }
    
    auto& list = std::get<std::list<std::string>>(it->second.value);
    if (list.empty()) {
        return std::nullopt;
    }
    
    std::string value = list.back();
    list.pop_back();
    return value;
}

std::vector<RedisValue> DataStore::lrange(const std::string& key, int64_t start, int64_t stop) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    std::vector<RedisValue> result;
    
    auto it = store_.find(key);
    if (it == store_.end() || is_expired(it->second)) {
        return result;
    }
    
    if (it->second.type != RedisType::LIST) {
        return result;
    }
    
    auto& list = std::get<std::list<std::string>>(it->second.value);
    size_t list_size = list.size();
    
    // Handle negative indices
    if (start < 0) start = list_size + start;
    if (stop < 0) stop = list_size + stop;
    
    // Clamp values
    start = std::max<int64_t>(0, start);
    stop = std::min<int64_t>(list_size - 1, stop);
    
    if (start > stop || start >= static_cast<int64_t>(list_size)) {
        return result;
    }
    
    auto list_it = list.begin();
    std::advance(list_it, start);
    
    for (int64_t i = start; i <= stop && i < static_cast<int64_t>(list_size); ++i) {
        result.push_back(*list_it);
        ++list_it;
    }
    
    return result;
}

size_t DataStore::llen(const std::string& key) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    auto it = store_.find(key);
    if (it == store_.end() || is_expired(it->second)) {
        return 0;
    }
    
    if (it->second.type != RedisType::LIST) {
        return 0;
    }
    
    auto& list = std::get<std::list<std::string>>(it->second.value);
    return list.size();
}

bool DataStore::lindex(const std::string& key, int64_t index, RedisValue& out_value) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    auto it = store_.find(key);
    if (it == store_.end() || is_expired(it->second)) {
        return false;
    }
    
    if (it->second.type != RedisType::LIST) {
        return false;
    }
    
    auto& list = std::get<std::list<std::string>>(it->second.value);
    size_t list_size = list.size();
    
    // Handle negative index
    if (index < 0) index = list_size + index;
    
    if (index < 0 || index >= static_cast<int64_t>(list_size)) {
        return false;
    }
    
    auto list_it = list.begin();
    std::advance(list_it, index);
    out_value = *list_it;
    return true;
}

bool DataStore::sadd(const std::string& key, const std::string& member) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    auto it = store_.find(key);
    if (it == store_.end() || is_expired(it->second)) {
        // Create new set
        KeyEntry entry;
        entry.type = RedisType::SET;
        entry.has_expiry = false;
        entry.value = std::set<std::string>{member};
        store_[key] = entry;
        return true;
    }
    
    if (it->second.type != RedisType::SET) {
        return false;
    }
    
    auto& set = std::get<std::set<std::string>>(it->second.value);
    auto result = set.insert(member);
    return result.second;
}

bool DataStore::srem(const std::string& key, const std::string& member) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    auto it = store_.find(key);
    if (it == store_.end() || is_expired(it->second)) {
        return false;
    }
    
    if (it->second.type != RedisType::SET) {
        return false;
    }
    
    auto& set = std::get<std::set<std::string>>(it->second.value);
    return set.erase(member) > 0;
}

bool DataStore::sismember(const std::string& key, const std::string& member) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    auto it = store_.find(key);
    if (it == store_.end() || is_expired(it->second)) {
        return false;
    }
    
    if (it->second.type != RedisType::SET) {
        return false;
    }
    
    auto& set = std::get<std::set<std::string>>(it->second.value);
    return set.find(member) != set.end();
}

std::vector<std::string> DataStore::smembers(const std::string& key) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    std::vector<std::string> result;
    
    auto it = store_.find(key);
    if (it == store_.end() || is_expired(it->second)) {
        return result;
    }
    
    if (it->second.type != RedisType::SET) {
        return result;
    }
    
    auto& set = std::get<std::set<std::string>>(it->second.value);
    for (const auto& member : set) {
        result.push_back(member);
    }
    
    return result;
}

size_t DataStore::scard(const std::string& key) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    auto it = store_.find(key);
    if (it == store_.end() || is_expired(it->second)) {
        return 0;
    }
    
    if (it->second.type != RedisType::SET) {
        return 0;
    }
    
    auto& set = std::get<std::set<std::string>>(it->second.value);
    return set.size();
}

bool DataStore::zadd(const std::string& key, double score, const std::string& member) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    auto it = store_.find(key);
    if (it == store_.end() || is_expired(it->second)) {
        // Create new sorted set
        KeyEntry entry;
        entry.type = RedisType::SORTED_SET;
        entry.has_expiry = false;
        ZSet zset;
        zset.score_set.insert({score, member});
        zset.member_map[member] = score;
        entry.value = zset;
        store_[key] = entry;
        return true;
    }
    
    if (it->second.type != RedisType::SORTED_SET) {
        return false;
    }
    
    auto& zset = std::get<ZSet>(it->second.value);
    auto map_it = zset.member_map.find(member);
    if (map_it != zset.member_map.end()) {
        double old_score = map_it->second;
        zset.score_set.erase({old_score, member});
    }
    zset.score_set.insert({score, member});
    zset.member_map[member] = score;
    return true;
}

bool DataStore::zrem(const std::string& key, const std::string& member) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    auto it = store_.find(key);
    if (it == store_.end() || is_expired(it->second)) {
        return false;
    }
    
    if (it->second.type != RedisType::SORTED_SET) {
        return false;
    }
    
    auto& zset = std::get<ZSet>(it->second.value);
    auto map_it = zset.member_map.find(member);
    if (map_it == zset.member_map.end()) {
        return false;
    }
    
    zset.score_set.erase({map_it->second, member});
    zset.member_map.erase(map_it);
    return true;
}

std::optional<double> DataStore::zscore(const std::string& key, const std::string& member) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    auto it = store_.find(key);
    if (it == store_.end() || is_expired(it->second)) {
        return std::nullopt;
    }
    
    if (it->second.type != RedisType::SORTED_SET) {
        return std::nullopt;
    }
    
    auto& zset = std::get<ZSet>(it->second.value);
    auto map_it = zset.member_map.find(member);
    if (map_it == zset.member_map.end()) {
        return std::nullopt;
    }
    
    return map_it->second;
}

std::vector<std::string> DataStore::zrange(const std::string& key, int64_t start, int64_t stop) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    std::vector<std::string> result;
    
    auto it = store_.find(key);
    if (it == store_.end() || is_expired(it->second)) {
        return result;
    }
    
    if (it->second.type != RedisType::SORTED_SET) {
        return result;
    }
    
    auto& zset = std::get<ZSet>(it->second.value);
    size_t zset_size = zset.score_set.size();
    
    // Handle negative indices
    if (start < 0) start = zset_size + start;
    if (stop < 0) stop = zset_size + stop;
    
    // Clamp values
    start = std::max<int64_t>(0, start);
    stop = std::min<int64_t>(zset_size - 1, stop);
    
    if (start > stop || start >= static_cast<int64_t>(zset_size)) {
        return result;
    }
    
    auto zset_it = zset.score_set.begin();
    std::advance(zset_it, start);
    
    for (int64_t i = start; i <= stop && zset_it != zset.score_set.end(); ++i) {
        result.push_back(zset_it->second);
        ++zset_it;
    }
    
    return result;
}

std::vector<std::string> DataStore::zrevrange(const std::string& key, int64_t start, int64_t stop) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    std::vector<std::string> result;
    
    auto it = store_.find(key);
    if (it == store_.end() || is_expired(it->second)) {
        return result;
    }
    
    if (it->second.type != RedisType::SORTED_SET) {
        return result;
    }
    
    auto& zset = std::get<ZSet>(it->second.value);
    size_t zset_size = zset.score_set.size();
    
    // Handle negative indices
    if (start < 0) start = zset_size + start;
    if (stop < 0) stop = zset_size + stop;
    
    // Clamp values
    start = std::max<int64_t>(0, start);
    stop = std::min<int64_t>(zset_size - 1, stop);
    
    if (start > stop || start >= static_cast<int64_t>(zset_size)) {
        return result;
    }
    
    auto zset_it = zset.score_set.rbegin();
    std::advance(zset_it, start);
    
    for (int64_t i = start; i <= stop && zset_it != zset.score_set.rend(); ++i) {
        result.push_back(zset_it->second);
        ++zset_it;
    }
    
    return result;
}

size_t DataStore::zcard(const std::string& key) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    auto it = store_.find(key);
    if (it == store_.end() || is_expired(it->second)) {
        return 0;
    }
    
    if (it->second.type != RedisType::SORTED_SET) {
        return 0;
    }
    
    auto& zset = std::get<ZSet>(it->second.value);
    return zset.member_map.size();
}

size_t DataStore::dbsize() {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    size_t count = 0;
    for (const auto& [key, entry] : store_) {
        if (!is_expired(entry)) {
            count++;
        }
    }
    
    return count;
}

void DataStore::flushdb() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    store_.clear();
}

void DataStore::flushall() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    store_.clear();
}

void DataStore::cleanup_expired_keys() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    auto now = std::chrono::system_clock::now();
    for (auto it = store_.begin(); it != store_.end(); ) {
        if (is_expired(it->second)) {
            it = store_.erase(it);
        } else {
            ++it;
        }
    }
}

std::vector<std::pair<std::string, KeyEntry>> DataStore::get_all_valid_entries() {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    std::vector<std::pair<std::string, KeyEntry>> result;
    for (const auto& [key, entry] : store_) {
        if (!is_expired(entry)) {
            result.push_back({key, entry});
        }
    }
    return result;
}

bool DataStore::is_expired(const KeyEntry& entry) const {
    if (!entry.has_expiry) {
        return false;
    }
    
    auto now = std::chrono::system_clock::now();
    return now >= entry.expiry_time;
}

void DataStore::remove_if_expired(const std::string& key) {
    auto it = store_.find(key);
    if (it != store_.end() && is_expired(it->second)) {
        store_.erase(it);
    }
}

} // namespace redis_clone
