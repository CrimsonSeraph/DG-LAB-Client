#include "DebugLog.h"

#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <sstream>
#include <typeinfo>
#include <functional>

DebugLog& DebugLog::Instance() {
    static DebugLog instance;
    static std::once_flag flag;
    std::call_once(flag, [&]() {
        LogSink consoleSink;
        consoleSink.callback = [](const std::string& module,
            const std::string& method,
            LogLevel level,
            const std::string& message) {
                std::cerr << "[" << module << "] <" << method << "> ("
                    << instance.level_to_string(level) << "): " << message << std::endl;
            };
        consoleSink.min_level = LOG_DEBUG;
        instance.register_log_sink("console", consoleSink);
        });
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
    std::lock_guard<std::mutex> lock(sinks_mutex_);
    for (const auto& pair : log_sinks_) {
        const auto& sink = pair.second;
        if (level >= sink.min_level) {
            sink.callback(module, method, level, message);
        }
    }
}

void DebugLog::set_default_log_level(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    default_log_level_ = level;
}

void DebugLog::register_log_sink(const std::string& name, const LogSink& sink) {
    std::lock_guard<std::mutex> lock(sinks_mutex_);
    log_sinks_[name] = sink;
}

void DebugLog::unregister_log_sink(const std::string& name) {
    std::lock_guard<std::mutex> lock(sinks_mutex_);
    log_sinks_.erase(name);
}

void DebugLog::set_log_sink_level(const std::string& name, LogLevel level) {
    std::lock_guard<std::mutex> lock(sinks_mutex_);
    auto it = log_sinks_.find(name);
    if (it != log_sinks_.end()) {
        it->second.min_level = level;
    }
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
