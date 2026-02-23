#include "DebugLog.h"

DebugLog& DebugLog::Instance() {
    static DebugLog instance;
    return instance;
}

void DebugLog::set_log_level(const std::string& module, LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    module_log_levels_[module] = level;
}

LogLevel DebugLog::get_log_level(const std::string& module) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = module_log_levels_.find(module);
    if (it != module_log_levels_.end()) {
        return it->second;
    }
    return default_log_level_;
}

void DebugLog::log(const std::string& module, LogLevel level, const std::string& message) {
    if (level >= get_log_level(module)) {
        std::lock_guard<std::mutex> lock(out_put_mutex_);
        std::cerr << "[" << module << "] (" << level_to_string(level) << ")：" << message << std::endl;
    }
}

void DebugLog::set_default_log_level(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    default_log_level_ = level;
}

const char* DebugLog::level_to_string(LogLevel level) {
    switch (level) {
    case LOG_DEBUG:
        return "DEBUG";
    case LOG_INFO:
        return "INFO";
    case LOG_WARN:
        return "WARN";
    case LOG_ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

std::mutex DebugLog::out_put_mutex_;
