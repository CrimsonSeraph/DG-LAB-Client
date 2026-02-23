#include "DebugLog.h"

DebugLog& DebugLog::Instance() {
    static DebugLog instance;
    return instance;
}

void DebugLog::set_all_log_level(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    default_log_level_ = level;
    for (auto& pair : module_log_levels_) {
        pair.second = level;
    }
}

void DebugLog::set_all_log_level(int level) {
    switch (level) {
    case 0:
        set_all_log_level(LOG_DEBUG);
        break;
    case 1:
        set_all_log_level(LOG_INFO);
        break;
    case 2:
        set_all_log_level(LOG_WARN);
        break;
    case 3:
        set_all_log_level(LOG_ERROR);
        break;
    case 4:
        set_all_log_level(LOG_NONE);
        break;
    default:
        set_all_log_level(LOG_DEBUG);
        break;
    }
}

void DebugLog::set_only_type_info(bool only_type_info) {
    std::lock_guard<std::mutex> lock(mutex_);
    is_only_type_info_ = only_type_info;
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

void DebugLog::log(const std::string& module, const std::string& method, LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(out_put_mutex_);
    if ((level == get_log_level(module)) ||
        (!is_only_type_info_ &&
            (level > get_log_level(module)))) {
        std::cerr << "[" << module << "] <" << method << "> (" << level_to_string(level) << "): " << message << std::endl;
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
