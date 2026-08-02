#include "redis_server.h"
#include "logger.h"
#include <iostream>
#include <csignal>
#include <memory>

using namespace redis_clone;

std::unique_ptr<RedisServer> g_server;

void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    if (g_server) {
        g_server->stop();
    }
    exit(0);
}

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --port <port>        Set server port (default: 6379)" << std::endl;
    std::cout << "  --config <file>      Load configuration from file" << std::endl;
    std::cout << "  --log-level <level>  Set log level (debug, info, warning, error, fatal)" << std::endl;
    std::cout << "  --log-file <file>    Set log file" << std::endl;
    std::cout << "  --help               Show this help message" << std::endl;
}

int main(int argc, char* argv[]) {
    int port = 6379;
    std::string config_file;
    std::string log_level = "info";
    std::string log_file;
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--port" && i + 1 < argc) {
            port = std::atoi(argv[++i]);
        } else if (arg == "--config" && i + 1 < argc) {
            config_file = argv[++i];
        } else if (arg == "--log-level" && i + 1 < argc) {
            log_level = argv[++i];
        } else if (arg == "--log-file" && i + 1 < argc) {
            log_file = argv[++i];
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    }
    
    // Setup signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    // Initialize logger
    Logger& logger = Logger::get_instance();
    
    if (log_level == "debug") {
        logger.set_log_level(LogLevel::DEBUG);
    } else if (log_level == "info") {
        logger.set_log_level(LogLevel::INFO);
    } else if (log_level == "warning") {
        logger.set_log_level(LogLevel::WARNING);
    } else if (log_level == "error") {
        logger.set_log_level(LogLevel::ERROR);
    } else if (log_level == "fatal") {
        logger.set_log_level(LogLevel::FATAL);
    }
    
    if (!log_file.empty()) {
        logger.set_log_file(log_file);
    }
    
    LOG_INFO("Redis Clone - C++ Implementation");
    LOG_INFO("=================================");
    
    // Create server
    g_server = std::make_unique<RedisServer>(port);
    
    // Load configuration if specified
    if (!config_file.empty()) {
        if (!g_server->load_config(config_file)) {
            LOG_WARNING("Using default configuration");
        }
    }
    
    // Try to load RDB snapshot
    g_server->load_rdb("dump.rdb");
    
    // Start server
    if (!g_server->start()) {
        LOG_FATAL("Failed to start Redis server");
        return 1;
    }
    
    LOG_INFO("Server is running. Press Ctrl+C to stop.");
    
    // Keep server running
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    return 0;
}
