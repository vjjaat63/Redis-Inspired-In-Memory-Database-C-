#ifndef AOF_PERSISTENCE_H
#define AOF_PERSISTENCE_H

#include "redis_clone.h"
#include <string>
#include <fstream>
#include <memory>

namespace redis_clone {

class AOFPersistence {
public:
    explicit AOFPersistence(const std::string& filename, std::shared_ptr<DataStore> data_store = nullptr);
    ~AOFPersistence();
    
    // Append a command to the AOF file
    bool append_command(const std::string& command, const std::vector<RedisValue>& args);
    
    // Rewrite the AOF file (compact)
    bool rewrite();
    
    // Load commands from AOF file
    std::vector<std::pair<std::string, std::vector<RedisValue>>> load();
    
    // Enable/disable fsync
    void set_fsync_enabled(bool enabled);
    
    // Set fsync policy (always, everysec, no)
    void set_fsync_policy(const std::string& policy);

private:
    std::string filename_;
    std::ofstream aof_file_;
    std::shared_ptr<DataStore> data_store_;
    bool fsync_enabled_;
    std::string fsync_policy_;
    
    bool open_file();
    void close_file();
    bool perform_fsync();
};

} // namespace redis_clone

#endif // AOF_PERSISTENCE_H
