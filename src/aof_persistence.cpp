#include "aof_persistence.h"
#include "resp_parser.h"
#include "data_store.h"
#include <iostream>
#include <filesystem>
#include <sstream>

namespace redis_clone {

AOFPersistence::AOFPersistence(const std::string& filename, std::shared_ptr<DataStore> data_store)
    : filename_(filename), data_store_(data_store), fsync_enabled_(true), fsync_policy_("everysec") {
    open_file();
}

AOFPersistence::~AOFPersistence() {
    close_file();
}

bool AOFPersistence::open_file() {
    aof_file_.open(filename_, std::ios::app);
    if (!aof_file_.is_open()) {
        std::cerr << "Failed to open AOF file: " << filename_ << std::endl;
        return false;
    }
    return true;
}

void AOFPersistence::close_file() {
    if (aof_file_.is_open()) {
        aof_file_.close();
    }
}

bool AOFPersistence::append_command(const std::string& command, const std::vector<RedisValue>& args) {
    if (!aof_file_.is_open()) {
        if (!open_file()) {
            return false;
        }
    }

    // Serialize command and args as RESP array
    std::vector<RedisValue> full_command;
    full_command.push_back(command);
    for (const auto& arg : args) {
        full_command.push_back(arg);
    }

    std::string serialized = RESPParser::serialize_array(full_command);
    aof_file_ << serialized;
    aof_file_.flush();

    if (fsync_enabled_ && fsync_policy_ == "always") {
        perform_fsync();
    }

    return true;
}

bool AOFPersistence::rewrite() {
    close_file();

    std::string temp_filename = filename_ + ".tmp";
    std::ofstream temp_file(temp_filename, std::ios::out);
    if (!temp_file.is_open()) {
        std::cerr << "Failed to create temporary AOF file for rewrite" << std::endl;
        open_file();
        return false;
    }

    if (data_store_) {
        auto entries = data_store_->get_all_valid_entries();
        for (const auto& [key, entry] : entries) {
            if (entry.type == RedisType::STRING) {
                std::string val = std::holds_alternative<std::string>(entry.value) ?
                    std::get<std::string>(entry.value) : std::to_string(std::get<int64_t>(entry.value));
                std::vector<RedisValue> cmd = {std::string("SET"), key, val};
                temp_file << RESPParser::serialize_array(cmd);
            } else if (entry.type == RedisType::HASH) {
                const auto& hash = std::get<std::unordered_map<std::string, RedisValue>>(entry.value);
                for (const auto& [field, val] : hash) {
                    std::string val_str = std::holds_alternative<std::string>(val) ?
                        std::get<std::string>(val) : std::to_string(std::get<int64_t>(val));
                    std::vector<RedisValue> cmd = {std::string("HSET"), key, field, val_str};
                    temp_file << RESPParser::serialize_array(cmd);
                }
            } else if (entry.type == RedisType::LIST) {
                const auto& list = std::get<std::list<std::string>>(entry.value);
                for (const auto& item : list) {
                    std::vector<RedisValue> cmd = {std::string("RPUSH"), key, item};
                    temp_file << RESPParser::serialize_array(cmd);
                }
            } else if (entry.type == RedisType::SET) {
                const auto& set = std::get<std::set<std::string>>(entry.value);
                for (const auto& member : set) {
                    std::vector<RedisValue> cmd = {std::string("SADD"), key, member};
                    temp_file << RESPParser::serialize_array(cmd);
                }
            } else if (entry.type == RedisType::SORTED_SET) {
                const auto& zset = std::get<ZSet>(entry.value);
                for (const auto& [score, member] : zset.score_set) {
                    std::vector<RedisValue> cmd = {std::string("ZADD"), key, std::to_string(score), member};
                    temp_file << RESPParser::serialize_array(cmd);
                }
            }

            if (entry.has_expiry) {
                auto now = std::chrono::system_clock::now();
                if (entry.expiry_time > now) {
                    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(entry.expiry_time - now).count();
                    if (seconds > 0) {
                        std::vector<RedisValue> cmd = {std::string("EXPIRE"), key, std::to_string(seconds)};
                        temp_file << RESPParser::serialize_array(cmd);
                    }
                }
            }
        }
    }

    temp_file.close();

    std::filesystem::remove(filename_);
    std::filesystem::rename(temp_filename, filename_);

    return open_file();
}

std::vector<std::pair<std::string, std::vector<RedisValue>>> AOFPersistence::load() {
    std::vector<std::pair<std::string, std::vector<RedisValue>>> commands;

    std::ifstream file(filename_, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        return commands;
    }

    std::stringstream ss;
    ss << file.rdbuf();
    std::string buffer = ss.str();
    file.close();

    while (!buffer.empty()) {
        size_t consumed_bytes = 0;
        auto parsed = RESPParser::parse_with_consumed(buffer, consumed_bytes);
        if (!parsed || consumed_bytes == 0) {
            break;
        }

        if (std::holds_alternative<std::vector<RedisValue>>(*parsed)) {
            auto args = std::get<std::vector<RedisValue>>(*parsed);
            if (!args.empty()) {
                std::string cmd_name = std::holds_alternative<std::string>(args[0]) ?
                    std::get<std::string>(args[0]) : "";
                std::vector<RedisValue> cmd_args(args.begin() + 1, args.end());
                commands.push_back({cmd_name, cmd_args});
            }
        }

        buffer.erase(0, consumed_bytes);
    }

    return commands;
}

void AOFPersistence::set_fsync_enabled(bool enabled) {
    fsync_enabled_ = enabled;
}

void AOFPersistence::set_fsync_policy(const std::string& policy) {
    fsync_policy_ = policy;
}

bool AOFPersistence::perform_fsync() {
    return true;
}

} // namespace redis_clone
