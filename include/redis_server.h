#ifndef REDIS_SERVER_H
#define REDIS_SERVER_H

#include "redis_clone.h"
#include "data_store.h"
#include "tcp_server.h"
#include "command_executor.h"
#include "config.h"
#include "rdb_persistence.h"
#include "aof_persistence.h"
#include "logger.h"
#include <memory>
#include <string>

namespace redis_clone {

class RedisServer {
public:
    RedisServer(int port = 6379);
    ~RedisServer();
    
    // Start the server
    bool start();
    
    // Stop the server
    void stop();
    
    // Load configuration
    bool load_config(const std::string& filename);
    
    // Save RDB snapshot
    bool save_rdb(const std::string& filename);
    
    // Load RDB snapshot
    bool load_rdb(const std::string& filename);
    
    // Enable/disable AOF
    void enable_aof(bool enabled, const std::string& filename = "appendonly.aof");
    
    // Get data store (for testing)
    std::shared_ptr<DataStore> get_data_store() { return data_store_; }

private:
    int port_;
    std::unique_ptr<TCPServer> tcp_server_;
    std::shared_ptr<DataStore> data_store_;
    std::unique_ptr<CommandExecutor> command_executor_;
    std::unique_ptr<Config> config_;
    std::unique_ptr<RDBPersistence> rdb_persistence_;
    std::unique_ptr<AOFPersistence> aof_persistence_;
    
    bool aof_enabled_;
    std::string aof_filename_;
    
    // Client handler
    void handle_client(socket_t client_socket, const std::string& client_address);
    
    // Process client request
    std::string process_request(const std::string& request);
    
    // Background tasks
    void start_background_tasks();
    void stop_background_tasks();
    
    // Cleanup thread
    std::thread cleanup_thread_;
    std::atomic<bool> cleanup_running_;
    void cleanup_expired_keys_loop();
};

} // namespace redis_clone

#endif // REDIS_SERVER_H
