#include "rdb_persistence.h"
#include <fstream>
#include <iostream>
#include <chrono>

namespace redis_clone {

const std::string RDBPersistence::REDIS_MAGIC = "REDIS";

RDBPersistence::RDBPersistence(std::shared_ptr<DataStore> data_store)
    : data_store_(data_store) {
}

bool RDBPersistence::save(const std::string& filename) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open RDB file for writing: " << filename << std::endl;
        return false;
    }

    // Write magic string
    file.write(REDIS_MAGIC.c_str(), REDIS_MAGIC.length());
    
    // Write version (4 bytes e.g. "0009")
    std::string ver = "0009";
    file.write(ver.c_str(), 4);
    
    // Write database selector
    uint8_t selectdb = static_cast<uint8_t>(Opcode::SELECTDB);
    file.write(reinterpret_cast<const char*>(&selectdb), 1);
    write_length(file, 0); // DB number 0

    // Dump all valid key-value entries
    auto entries = data_store_->get_all_valid_entries();

    for (const auto& [key, entry] : entries) {
        // Write expiry if present
        if (entry.has_expiry) {
            uint8_t expire_op = static_cast<uint8_t>(Opcode::EXPIRETIME_MS);
            file.write(reinterpret_cast<const char*>(&expire_op), 1);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                entry.expiry_time.time_since_epoch()
            ).count();
            uint64_t expiry_ms = static_cast<uint64_t>(ms);
            file.write(reinterpret_cast<const char*>(&expiry_ms), sizeof(expiry_ms));
        }

        // Write value type byte & payload
        if (entry.type == RedisType::STRING) {
            uint8_t type_byte = static_cast<uint8_t>(ValueType::STRING);
            file.write(reinterpret_cast<const char*>(&type_byte), 1);
            write_string(file, key);
            std::string val_str = std::holds_alternative<std::string>(entry.value) ?
                std::get<std::string>(entry.value) : std::to_string(std::get<int64_t>(entry.value));
            write_string(file, val_str);
        } else if (entry.type == RedisType::HASH) {
            uint8_t type_byte = static_cast<uint8_t>(ValueType::HASH);
            file.write(reinterpret_cast<const char*>(&type_byte), 1);
            write_string(file, key);
            const auto& hash = std::get<std::unordered_map<std::string, RedisValue>>(entry.value);
            write_length(file, static_cast<uint32_t>(hash.size()));
            for (const auto& [field, val] : hash) {
                write_string(file, field);
                std::string val_str = std::holds_alternative<std::string>(val) ?
                    std::get<std::string>(val) : std::to_string(std::get<int64_t>(val));
                write_string(file, val_str);
            }
        } else if (entry.type == RedisType::LIST) {
            uint8_t type_byte = static_cast<uint8_t>(ValueType::LIST);
            file.write(reinterpret_cast<const char*>(&type_byte), 1);
            write_string(file, key);
            const auto& list = std::get<std::list<std::string>>(entry.value);
            write_length(file, static_cast<uint32_t>(list.size()));
            for (const auto& item : list) {
                write_string(file, item);
            }
        } else if (entry.type == RedisType::SET) {
            uint8_t type_byte = static_cast<uint8_t>(ValueType::SET);
            file.write(reinterpret_cast<const char*>(&type_byte), 1);
            write_string(file, key);
            const auto& set = std::get<std::set<std::string>>(entry.value);
            write_length(file, static_cast<uint32_t>(set.size()));
            for (const auto& member : set) {
                write_string(file, member);
            }
        } else if (entry.type == RedisType::SORTED_SET) {
            uint8_t type_byte = static_cast<uint8_t>(ValueType::ZSET);
            file.write(reinterpret_cast<const char*>(&type_byte), 1);
            write_string(file, key);
            const auto& zset = std::get<ZSet>(entry.value);
            write_length(file, static_cast<uint32_t>(zset.score_set.size()));
            for (const auto& [score, member] : zset.score_set) {
                write_string(file, std::to_string(score));
                write_string(file, member);
            }
        }
    }
    
    // Write EOF marker
    uint8_t eof = static_cast<uint8_t>(Opcode::EOF_MARK);
    file.write(reinterpret_cast<const char*>(&eof), 1);
    
    // Write checksum (8 bytes)
    uint64_t checksum = 0;
    file.write(reinterpret_cast<const char*>(&checksum), 8);
    
    file.close();
    return true;
}

bool RDBPersistence::load(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    // Read magic string
    char magic[6];
    file.read(magic, 5);
    magic[5] = '\0';
    if (std::string(magic) != REDIS_MAGIC) {
        std::cerr << "Invalid RDB file: magic string mismatch" << std::endl;
        file.close();
        return false;
    }

    // Read 4-byte version
    char ver[5];
    file.read(ver, 4);

    bool has_expiry = false;
    std::chrono::system_clock::time_point expiry_time;
    
    // Read database records
    while (file.good()) {
        uint8_t type_or_op;
        file.read(reinterpret_cast<char*>(&type_or_op), 1);
        if (!file.good()) break;
        
        if (type_or_op == static_cast<uint8_t>(Opcode::EOF_MARK)) {
            uint64_t checksum;
            file.read(reinterpret_cast<char*>(&checksum), 8);
            break;
        }
        
        if (type_or_op == static_cast<uint8_t>(Opcode::SELECTDB)) {
            read_length(file);
            continue;
        }

        if (type_or_op == static_cast<uint8_t>(Opcode::EXPIRETIME_MS)) {
            uint64_t expiry_ms;
            file.read(reinterpret_cast<char*>(&expiry_ms), 8);
            has_expiry = true;
            expiry_time = std::chrono::system_clock::time_point(std::chrono::milliseconds(expiry_ms));
            continue;
        }

        if (type_or_op == static_cast<uint8_t>(Opcode::EXPIRETIME)) {
            uint32_t expiry_s;
            file.read(reinterpret_cast<char*>(&expiry_s), 4);
            has_expiry = true;
            expiry_time = std::chrono::system_clock::time_point(std::chrono::seconds(expiry_s));
            continue;
        }

        // It is a value type byte
        uint8_t vtype = type_or_op;
        auto key_opt = read_string(file);
        if (!key_opt) break;
        std::string key = *key_opt;

        std::chrono::milliseconds ttl(0);
        if (has_expiry) {
            auto now = std::chrono::system_clock::now();
            if (expiry_time > now) {
                ttl = std::chrono::duration_cast<std::chrono::milliseconds>(expiry_time - now);
            } else {
                // Expired key, skip reading value
                ttl = std::chrono::milliseconds(1);
            }
        }

        if (vtype == static_cast<uint8_t>(ValueType::STRING)) {
            auto val_opt = read_string(file);
            if (val_opt) {
                data_store_->set(key, *val_opt, ttl);
            }
        } else if (vtype == static_cast<uint8_t>(ValueType::HASH)) {
            auto len_opt = read_length(file);
            if (len_opt) {
                uint32_t count = *len_opt;
                for (uint32_t i = 0; i < count; ++i) {
                    auto f = read_string(file);
                    auto v = read_string(file);
                    if (f && v) {
                        data_store_->hset(key, *f, *v);
                    }
                }
                if (has_expiry && ttl.count() > 0) data_store_->expire(key, ttl);
            }
        } else if (vtype == static_cast<uint8_t>(ValueType::LIST)) {
            auto len_opt = read_length(file);
            if (len_opt) {
                uint32_t count = *len_opt;
                for (uint32_t i = 0; i < count; ++i) {
                    auto item = read_string(file);
                    if (item) {
                        data_store_->rpush(key, *item);
                    }
                }
                if (has_expiry && ttl.count() > 0) data_store_->expire(key, ttl);
            }
        } else if (vtype == static_cast<uint8_t>(ValueType::SET)) {
            auto len_opt = read_length(file);
            if (len_opt) {
                uint32_t count = *len_opt;
                for (uint32_t i = 0; i < count; ++i) {
                    auto m = read_string(file);
                    if (m) {
                        data_store_->sadd(key, *m);
                    }
                }
                if (has_expiry && ttl.count() > 0) data_store_->expire(key, ttl);
            }
        } else if (vtype == static_cast<uint8_t>(ValueType::ZSET)) {
            auto len_opt = read_length(file);
            if (len_opt) {
                uint32_t count = *len_opt;
                for (uint32_t i = 0; i < count; ++i) {
                    auto score_str = read_string(file);
                    auto m = read_string(file);
                    if (score_str && m) {
                        try {
                            double s = std::stod(*score_str);
                            data_store_->zadd(key, s, *m);
                        } catch (...) {}
                    }
                }
                if (has_expiry && ttl.count() > 0) data_store_->expire(key, ttl);
            }
        }

        has_expiry = false;
    }
    
    file.close();
    return true;
}

bool RDBPersistence::write_string(std::ofstream& file, const std::string& str) {
    return write_length(file, static_cast<uint32_t>(str.length())) && file.write(str.c_str(), str.length()).good();
}

bool RDBPersistence::write_integer(std::ofstream& file, int64_t value) {
    return file.write(reinterpret_cast<const char*>(&value), sizeof(value)).good();
}

bool RDBPersistence::write_length(std::ofstream& file, uint32_t length) {
    return file.write(reinterpret_cast<const char*>(&length), sizeof(length)).good();
}

std::optional<std::string> RDBPersistence::read_string(std::ifstream& file) {
    auto length = read_length(file);
    if (!length) {
        return std::nullopt;
    }
    
    std::string str(*length, '\0');
    file.read(&str[0], *length);
    if (!file.good()) {
        return std::nullopt;
    }
    
    return str;
}

std::optional<int64_t> RDBPersistence::read_integer(std::ifstream& file) {
    int64_t value;
    file.read(reinterpret_cast<char*>(&value), sizeof(value));
    if (!file.good()) {
        return std::nullopt;
    }
    return value;
}

std::optional<uint32_t> RDBPersistence::read_length(std::ifstream& file) {
    uint32_t length;
    file.read(reinterpret_cast<char*>(&length), sizeof(length));
    if (!file.good()) {
        return std::nullopt;
    }
    return length;
}

} // namespace redis_clone
