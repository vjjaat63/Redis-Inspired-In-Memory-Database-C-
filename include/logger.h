#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>
#include <mutex>
#include <memory>
#include <sstream>

namespace redis_clone {

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    FATAL
};

class Logger {
public:
    static Logger& get_instance();
    
    void set_log_level(LogLevel level);
    void set_log_file(const std::string& filename);
    
    void log(LogLevel level, const std::string& message);
    void debug(const std::string& message);
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);
    void fatal(const std::string& message);
    
private:
    Logger();
    ~Logger();
    
    LogLevel log_level_;
    std::ofstream log_file_;
    std::mutex mutex_;
    bool use_file_;
    
    std::string level_to_string(LogLevel level);
    std::string get_timestamp();
};

// Convenience macros
#define LOG_DEBUG(msg) Logger::get_instance().debug(msg)
#define LOG_INFO(msg) Logger::get_instance().info(msg)
#define LOG_WARNING(msg) Logger::get_instance().warning(msg)
#define LOG_ERROR(msg) Logger::get_instance().error(msg)
#define LOG_FATAL(msg) Logger::get_instance().fatal(msg)

} // namespace redis_clone

#endif // LOGGER_H
