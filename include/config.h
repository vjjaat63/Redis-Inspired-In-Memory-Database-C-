#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <unordered_map>
#include <vector>

namespace redis_clone {

class Config {
public:
    Config();
    
    // Load configuration from file
    bool load(const std::string& filename);
    
    // Save configuration to file
    bool save(const std::string& filename);
    
    // Get configuration values
    std::string get(const std::string& key, const std::string& default_value = "");
    int get_int(const std::string& key, int default_value = 0);
    bool get_bool(const std::string& key, bool default_value = false);
    
    // Set configuration values
    void set(const std::string& key, const std::string& value);
    void set_int(const std::string& key, int value);
    void set_bool(const std::string& key, bool value);
    
    // Default configuration values
    static Config get_default_config();

private:
    std::unordered_map<std::string, std::string> config_;
    
    std::string trim(const std::string& str);
    std::vector<std::string> split(const std::string& str, char delimiter);
    std::string to_lower(const std::string& str);
};

} // namespace redis_clone

#endif // CONFIG_H
