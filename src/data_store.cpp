#include "data_store.h"
#include <algorithm>

using namespace std;

namespace redis_clone {

// --- String Operations ---

bool DataStore::set(const string& key, const string& value, chrono::milliseconds ttl) {
    LockGuard lock(mutex_);
    KeyEntry entry;
    entry.type = RedisType::STRING;
    entry.str_val = value;
    entry.has_expiry = (ttl.count() > 0);
    if (entry.has_expiry) {
        entry.expiry_time = chrono::system_clock::now() + ttl;
    }
    store_[key] = entry;
    return true;
}

bool DataStore::get(const string& key, string& out_val) {
    LockGuard lock(mutex_);
    auto it = store_.find(key);
    if (it == store_.end() || is_expired(it->second) || it->second.type != RedisType::STRING) {
        return false;
    }
    out_val = it->second.str_val;
    return true;
}

bool DataStore::del(const string& key) {
    LockGuard lock(mutex_);
    return store_.erase(key) > 0;
}

bool DataStore::exists(const string& key) {
    LockGuard lock(mutex_);
    auto it = store_.find(key);
    return (it != store_.end() && !is_expired(it->second));
}

vector<string> DataStore::keys(const string& pattern) {
    LockGuard lock(mutex_);
    vector<string> result;
    for (const auto& pair : store_) {
        if (!is_expired(pair.second) && (pattern == "*" || pair.first == pattern)) {
            result.push_back(pair.first);
        }
    }
    return result;
}

// --- Hash Operations ---

bool DataStore::hset(const string& key, const string& field, const string& value) {
    LockGuard lock(mutex_);
    auto it = store_.find(key);
    if (it == store_.end() || is_expired(it->second)) {
        KeyEntry entry;
        entry.type = RedisType::HASH;
        entry.hash_val[field] = value;
        store_[key] = entry;
        return true;
    }
    if (it->second.type != RedisType::HASH) return false;
    it->second.hash_val[field] = value;
    return true;
}

bool DataStore::hget(const string& key, const string& field, string& out_val) {
    LockGuard lock(mutex_);
    auto it = store_.find(key);
    if (it == store_.end() || is_expired(it->second) || it->second.type != RedisType::HASH) {
        return false;
    }
    auto f_it = it->second.hash_val.find(field);
    if (f_it == it->second.hash_val.end()) return false;
    out_val = f_it->second;
    return true;
}

bool DataStore::hdel(const string& key, const string& field) {
    LockGuard lock(mutex_);
    auto it = store_.find(key);
    if (it == store_.end() || is_expired(it->second) || it->second.type != RedisType::HASH) {
        return false;
    }
    return it->second.hash_val.erase(field) > 0;
}

vector<pair<string, string>> DataStore::hgetall(const string& key) {
    LockGuard lock(mutex_);
    vector<pair<string, string>> result;
    auto it = store_.find(key);
    if (it != store_.end() && !is_expired(it->second) && it->second.type == RedisType::HASH) {
        result.assign(it->second.hash_val.begin(), it->second.hash_val.end());
    }
    return result;
}

size_t DataStore::hlen(const string& key) {
    LockGuard lock(mutex_);
    auto it = store_.find(key);
    if (it == store_.end() || is_expired(it->second) || it->second.type != RedisType::HASH) {
        return 0;
    }
    return it->second.hash_val.size();
}

// --- List Operations ---

size_t DataStore::lpush(const string& key, const string& value) {
    LockGuard lock(mutex_);
    auto it = store_.find(key);
    if (it == store_.end() || is_expired(it->second)) {
        KeyEntry entry;
        entry.type = RedisType::LIST;
        entry.list_val.push_back(value);
        store_[key] = entry;
        return 1;
    }
    if (it->second.type != RedisType::LIST) return 0;
    it->second.list_val.insert(it->second.list_val.begin(), value);
    return it->second.list_val.size();
}

size_t DataStore::rpush(const string& key, const string& value) {
    LockGuard lock(mutex_);
    auto it = store_.find(key);
    if (it == store_.end() || is_expired(it->second)) {
        KeyEntry entry;
        entry.type = RedisType::LIST;
        entry.list_val.push_back(value);
        store_[key] = entry;
        return 1;
    }
    if (it->second.type != RedisType::LIST) return 0;
    it->second.list_val.push_back(value);
    return it->second.list_val.size();
}

bool DataStore::lpop(const string& key, string& out_val) {
    LockGuard lock(mutex_);
    auto it = store_.find(key);
    if (it == store_.end() || is_expired(it->second) || it->second.type != RedisType::LIST) {
        return false;
    }
    if (it->second.list_val.empty()) return false;
    out_val = it->second.list_val.front();
    it->second.list_val.erase(it->second.list_val.begin());
    return true;
}

bool DataStore::rpop(const string& key, string& out_val) {
    LockGuard lock(mutex_);
    auto it = store_.find(key);
    if (it == store_.end() || is_expired(it->second) || it->second.type != RedisType::LIST) {
        return false;
    }
    if (it->second.list_val.empty()) return false;
    out_val = it->second.list_val.back();
    it->second.list_val.pop_back();
    return true;
}

vector<string> DataStore::lrange(const string& key, int64_t start, int64_t stop) {
    LockGuard lock(mutex_);
    vector<string> result;
    auto it = store_.find(key);
    if (it == store_.end() || is_expired(it->second) || it->second.type != RedisType::LIST) {
        return result;
    }
    const auto& vec = it->second.list_val;
    int64_t sz = static_cast<int64_t>(vec.size());
    if (sz == 0) return result;

    if (start < 0) start += sz;
    if (stop < 0) stop += sz;
    start = max<int64_t>(0, start);
    stop = min<int64_t>(sz - 1, stop);
    if (start > stop || start >= sz) return result;

    for (int64_t i = start; i <= stop; ++i) {
        result.push_back(vec[i]);
    }
    return result;
}

size_t DataStore::llen(const string& key) {
    LockGuard lock(mutex_);
    auto it = store_.find(key);
    if (it == store_.end() || is_expired(it->second) || it->second.type != RedisType::LIST) {
        return 0;
    }
    return it->second.list_val.size();
}

// --- Expiration & TTL ---

bool DataStore::expire(const string& key, chrono::milliseconds ttl) {
    LockGuard lock(mutex_);
    auto it = store_.find(key);
    if (it == store_.end() || is_expired(it->second)) return false;
    it->second.has_expiry = true;
    it->second.expiry_time = chrono::system_clock::now() + ttl;
    return true;
}

int64_t DataStore::ttl(const string& key) {
    LockGuard lock(mutex_);
    auto it = store_.find(key);
    if (it == store_.end() || is_expired(it->second)) return -2;
    if (!it->second.has_expiry) return -1;
    auto rem_ms = chrono::duration_cast<chrono::milliseconds>(it->second.expiry_time - chrono::system_clock::now()).count();
    if (rem_ms <= 0) return -2;
    return (rem_ms + 999) / 1000;
}

bool DataStore::persist(const string& key) {
    LockGuard lock(mutex_);
    auto it = store_.find(key);
    if (it == store_.end() || is_expired(it->second) || !it->second.has_expiry) return false;
    it->second.has_expiry = false;
    return true;
}

void DataStore::cleanup_expired_keys() {
    LockGuard lock(mutex_);
    for (auto it = store_.begin(); it != store_.end(); ) {
        if (is_expired(it->second)) it = store_.erase(it);
        else ++it;
    }
}

// --- Utilities & Snapshot ---

RedisType DataStore::type(const string& key) {
    LockGuard lock(mutex_);
    auto it = store_.find(key);
    if (it == store_.end() || is_expired(it->second)) return RedisType::NONE;
    return it->second.type;
}

size_t DataStore::dbsize() {
    LockGuard lock(mutex_);
    size_t count = 0;
    for (const auto& pair : store_) {
        if (!is_expired(pair.second)) count++;
    }
    return count;
}

void DataStore::flushall() {
    LockGuard lock(mutex_);
    store_.clear();
}

unordered_map<string, KeyEntry> DataStore::get_all_entries() {
    LockGuard lock(mutex_);
    unordered_map<string, KeyEntry> result;
    for (const auto& pair : store_) {
        if (!is_expired(pair.second)) result[pair.first] = pair.second;
    }
    return result;
}

void DataStore::restore_entries(const unordered_map<string, KeyEntry>& entries) {
    LockGuard lock(mutex_);
    store_ = entries;
}

bool DataStore::is_expired(const KeyEntry& entry) const {
    return entry.has_expiry && (chrono::system_clock::now() >= entry.expiry_time);
}

} // namespace redis_clone
