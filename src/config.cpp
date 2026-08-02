#include "config.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace redis_clone {

Config::Config() {
    // Load default configuration
    *this = get_default_config();
}

bool Config::load(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        // Parse key-value pairs
        size_t pos = line.find(' ');
        if (pos == std::string::npos) {
            pos = line.find('\t');
        }
        
        if (pos != std::string::npos) {
            std::string key = trim(line.substr(0, pos));
            std::string value = trim(line.substr(pos + 1));
            config_[key] = value;
        }
    }

    file.close();
    return true;
}

bool Config::save(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    for (const auto& pair : config_) {
        file << pair.first << " " << pair.second << std::endl;
    }

    file.close();
    return true;
}

std::string Config::get(const std::string& key, const std::string& default_value) {
    auto it = config_.find(key);
    if (it != config_.end()) {
        return it->second;
    }
    return default_value;
}

int Config::get_int(const std::string& key, int default_value) {
    std::string value = get(key, "");
    if (value.empty()) {
        return default_value;
    }
    try {
        return std::stoi(value);
    } catch (...) {
        return default_value;
    }
}

bool Config::get_bool(const std::string& key, bool default_value) {
    std::string value = to_lower(get(key, ""));
    if (value == "yes" || value == "true" || value == "1") {
        return true;
    } else if (value == "no" || value == "false" || value == "0") {
        return false;
    }
    return default_value;
}

void Config::set(const std::string& key, const std::string& value) {
    config_[key] = value;
}

void Config::set_int(const std::string& key, int value) {
    config_[key] = std::to_string(value);
}

void Config::set_bool(const std::string& key, bool value) {
    config_[key] = value ? "yes" : "no";
}

Config Config::get_default_config() {
    Config config;
    config.set("port", "6379");
    config.set("bind", "127.0.0.1");
    config.set("daemonize", "no");
    config.set("pidfile", "/var/run/redis.pid");
    config.set("loglevel", "notice");
    config.set("logfile", "");
    config.set("databases", "16");
    config.set("save", "900 1 300 10 60 10000");
    config.set("appendonly", "no");
    config.set("appendfilename", "appendonly.aof");
    config.set("appendfsync", "everysec");
    config.set("maxclients", "10000");
    config.set("timeout", "0");
    config.set("tcp-keepalive", "300");
    return config;
}

std::string Config::trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) {
        return "";
    }
    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

std::vector<std::string> Config::split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

std::string Config::to_lower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

} // namespace redis_clone
