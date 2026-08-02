#include "redis_server.h"
#include "resp_parser.h"
#include <iostream>
#include <thread>
#include <chrono>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <unistd.h>
    #define closesocket close
#endif

namespace redis_clone {

RedisServer::RedisServer(int port)
    : port_(port),
      data_store_(std::make_shared<DataStore>()),
      command_executor_(std::make_unique<CommandExecutor>(data_store_)),
      config_(std::make_unique<Config>()),
      rdb_persistence_(std::make_unique<RDBPersistence>(data_store_)),
      aof_persistence_(nullptr),
      aof_enabled_(false),
      cleanup_running_(false) {
}

RedisServer::~RedisServer() {
    stop();
}

bool RedisServer::start() {
    LOG_INFO("Starting Redis Server on port " + std::to_string(port_));
    
    tcp_server_ = std::make_unique<TCPServer>(port_);
    tcp_server_->set_client_handler([this](socket_t client_socket, const std::string& client_address) {
        handle_client(client_socket, client_address);
    });
    
    if (!tcp_server_->start()) {
        LOG_ERROR("Failed to start TCP server");
        return false;
    }
    
    start_background_tasks();
    LOG_INFO("Redis Server started successfully");
    
    return true;
}

void RedisServer::stop() {
    LOG_INFO("Stopping Redis Server");
    
    stop_background_tasks();
    
    if (tcp_server_) {
        tcp_server_->stop();
    }
    
    LOG_INFO("Redis Server stopped");
}

bool RedisServer::load_config(const std::string& filename) {
    if (!config_->load(filename)) {
        LOG_WARNING("Failed to load config file: " + filename + ", using defaults");
        return false;
    }
    
    LOG_INFO("Configuration loaded from: " + filename);
    
    // Update port from config
    port_ = config_->get_int("port", 6379);
    
    // Enable AOF if configured
    if (config_->get_bool("appendonly", false)) {
        std::string aof_filename = config_->get("appendfilename", "appendonly.aof");
        enable_aof(true, aof_filename);
    }
    
    return true;
}

bool RedisServer::save_rdb(const std::string& filename) {
    LOG_INFO("Saving RDB snapshot to: " + filename);
    bool result = rdb_persistence_->save(filename);
    if (result) {
        LOG_INFO("RDB snapshot saved successfully");
    } else {
        LOG_ERROR("Failed to save RDB snapshot");
    }
    return result;
}

bool RedisServer::load_rdb(const std::string& filename) {
    LOG_INFO("Loading RDB snapshot from: " + filename);
    bool result = rdb_persistence_->load(filename);
    if (result) {
        LOG_INFO("RDB snapshot loaded successfully");
    } else {
        LOG_WARNING("Failed to load RDB snapshot (file may not exist)");
    }
    return result;
}

void RedisServer::enable_aof(bool enabled, const std::string& filename) {
    aof_enabled_ = enabled;
    aof_filename_ = filename;
    
    if (enabled) {
        aof_persistence_ = std::make_unique<AOFPersistence>(filename, data_store_);
        LOG_INFO("AOF enabled with file: " + filename);
        
        // Replay existing AOF commands
        auto commands = aof_persistence_->load();
        if (!commands.empty()) {
            LOG_INFO("Replaying " + std::to_string(commands.size()) + " commands from AOF...");
            for (const auto& [cmd, args] : commands) {
                command_executor_->execute(cmd, args);
            }
        }
    } else {
        aof_persistence_.reset();
        LOG_INFO("AOF disabled");
    }
}

void RedisServer::handle_client(socket_t client_socket, const std::string& client_address) {
    LOG_INFO("Handling client: " + client_address);
    
    char buffer[4096];
    std::string request_buffer;
    
    while (true) {
#ifdef _WIN32
        int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
#else
        ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
#endif
        
        if (bytes_received <= 0) {
            LOG_INFO("Client disconnected: " + client_address);
            break;
        }
        
        request_buffer.append(buffer, bytes_received);
        
        // Loop parsing commands from buffer
        while (!request_buffer.empty()) {
            size_t consumed_bytes = 0;
            auto parsed = RESPParser::parse_with_consumed(request_buffer, consumed_bytes);
            if (!parsed || consumed_bytes == 0) {
                // Incomplete command, wait for more data from socket
                break;
            }
            
            // Extract command and args
            if (std::holds_alternative<std::vector<RedisValue>>(*parsed)) {
                auto args = std::get<std::vector<RedisValue>>(*parsed);
                if (!args.empty()) {
                    std::string command = std::holds_alternative<std::string>(args[0]) ?
                        std::get<std::string>(args[0]) : "";
                    
                    std::vector<RedisValue> cmd_args(args.begin() + 1, args.end());
                    
                    // Execute command
                    RedisValue result = command_executor_->execute(command, cmd_args);
                    
                    // Serialize response
                    std::string response = RESPParser::serialize(result);
                    
                    // Send response
#ifdef _WIN32
                    send(client_socket, response.c_str(), static_cast<int>(response.length()), 0);
#else
                    send(client_socket, response.c_str(), response.length(), 0);
#endif
                    
                    // Append to AOF if enabled
                    if (aof_enabled_ && aof_persistence_) {
                        aof_persistence_->append_command(command, cmd_args);
                    }
                }
            }
            
            // Advance request buffer by exact number of consumed bytes
            request_buffer.erase(0, consumed_bytes);
        }
    }
    
    closesocket(client_socket);
}

std::string RedisServer::process_request(const std::string& request) {
    auto parsed = RESPParser::parse(request);
    if (!parsed) {
        return RESPParser::serialize_error("ERR invalid request");
    }
    
    if (std::holds_alternative<std::vector<RedisValue>>(*parsed)) {
        auto args = std::get<std::vector<RedisValue>>(*parsed);
        if (!args.empty()) {
            std::string command = std::holds_alternative<std::string>(args[0]) ?
                std::get<std::string>(args[0]) : "";
            
            std::vector<RedisValue> cmd_args(args.begin() + 1, args.end());
            
            RedisValue result = command_executor_->execute(command, cmd_args);
            return RESPParser::serialize(result);
        }
    }
    
    return RESPParser::serialize_error("ERR invalid request");
}

void RedisServer::start_background_tasks() {
    cleanup_running_ = true;
    cleanup_thread_ = std::thread(&RedisServer::cleanup_expired_keys_loop, this);
}

void RedisServer::stop_background_tasks() {
    cleanup_running_ = false;
    if (cleanup_thread_.joinable()) {
        cleanup_thread_.join();
    }
}

void RedisServer::cleanup_expired_keys_loop() {
    while (cleanup_running_) {
        data_store_->cleanup_expired_keys();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

} // namespace redis_clone
