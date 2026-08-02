#include "logger.h"
#include <iostream>
#include <iomanip>
#include <ctime>

namespace redis_clone {

Logger::Logger()
    : log_level_(LogLevel::INFO), use_file_(false) {
}

Logger::~Logger() {
    if (log_file_.is_open()) {
        log_file_.close();
    }
}

Logger& Logger::get_instance() {
    static Logger instance;
    return instance;
}

void Logger::set_log_level(LogLevel level) {
    log_level_ = level;
}

void Logger::set_log_file(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (log_file_.is_open()) {
        log_file_.close();
    }
    
    log_file_.open(filename, std::ios::app);
    use_file_ = log_file_.is_open();
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level < log_level_) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string timestamp = get_timestamp();
    std::string level_str = level_to_string(level);
    std::string log_message = "[" + timestamp + "] [" + level_str + "] " + message;
    
    if (use_file_) {
        log_file_ << log_message << std::endl;
        log_file_.flush();
    }
    
    std::cout << log_message << std::endl;
}

void Logger::debug(const std::string& message) {
    log(LogLevel::DEBUG, message);
}

void Logger::info(const std::string& message) {
    log(LogLevel::INFO, message);
}

void Logger::warning(const std::string& message) {
    log(LogLevel::WARNING, message);
}

void Logger::error(const std::string& message) {
    log(LogLevel::ERROR, message);
}

void Logger::fatal(const std::string& message) {
    log(LogLevel::FATAL, message);
}

std::string Logger::level_to_string(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

std::string Logger::get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms.count();
    
    return ss.str();
}

} // namespace redis_clone
