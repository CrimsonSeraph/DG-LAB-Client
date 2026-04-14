#include "DebugLog.h"

#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <regex>
#include <sstream>
#include <string>
#include <typeinfo>

#ifdef _WIN32
#include <windows.h>
#endif

// ============================================
// 单例（public）
// ============================================

DebugLog& DebugLog::instance() {
    static DebugLog instance;
    static std::once_flag flag;
    std::call_once(flag, [&]() {
        LogSink consoleSink;
        consoleSink.callback = [](const std::string& module,
            const std::string& method,
            LogLevel level,
            const std::string& message) {
                std::string tag1 = "[" + module + "]";
                std::string tag2 = "<" + method + ">";
                std::string tag3 = "(" + std::string(DebugLog::instance().level_to_string(level)) + ")";

                const int TAG1_WIDTH = 30;
                const int TAG2_WIDTH = 35;
                const int TAG3_WIDTH = 10;

                auto padRight = [](const std::string& s, int width) {
                    if (s.length() >= width) return s.substr(0, width);
                    return s + std::string(width - s.length(), ' ');
                    };

                std::string formatted = padRight(tag1, TAG1_WIDTH) + " "
                    + padRight(tag2, TAG2_WIDTH) + " "
                    + padRight(tag3, TAG3_WIDTH) + ": "
                    + message;

#ifdef _WIN32
                HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
                if (hConsole != INVALID_HANDLE_VALUE) {
                    DWORD written;
                    WriteConsoleA(hConsole, formatted.c_str(), formatted.size(), &written, nullptr);
                    WriteConsoleA(hConsole, "\n", 1, &written, nullptr);
                }
                else {
                    std::cerr << formatted << std::endl;
                }
#else
                std::cerr << formatted << std::endl;
#endif
            };
        consoleSink.min_level = LOG_DEBUG;
        instance.register_log_sink("console", consoleSink);
        });
    return instance;
}

// ============================================
// 日志等级控制（public）
// ============================================

void DebugLog::set_all_log_level(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    default_log_level_ = level;
    for (auto& pair : module_log_levels_) {
        pair.second = level;
    }
}

void DebugLog::set_all_log_level(int level) {
    switch (level) {
    case 0: set_all_log_level(LOG_DEBUG); break;
    case 1: set_all_log_level(LOG_INFO);  break;
    case 2: set_all_log_level(LOG_WARN);  break;
    case 3: set_all_log_level(LOG_ERROR); break;
    case 4: set_all_log_level(LOG_NONE);  break;
    default: set_all_log_level(LOG_DEBUG); break;
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

void DebugLog::set_default_log_level(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    default_log_level_ = level;
}

// ============================================
// 日志输出（public）
// ============================================

void DebugLog::log(const std::string& module, const std::string& method,
    LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(sinks_mutex_);
    for (const auto& pair : log_sinks_) {
        const auto& sink = pair.second;
        if (level >= sink.min_level) {
            sink.callback(module, method, level, message);
        }
    }
}

// ============================================
// Sink 管理（public）
// ============================================

void DebugLog::register_log_sink(const std::string& name, const LogSink& sink) {
    std::lock_guard<std::mutex> lock(sinks_mutex_);
    log_sinks_[name] = sink;
}

void DebugLog::unregister_log_sink(const std::string& name) {
    std::lock_guard<std::mutex> lock(sinks_mutex_);
    log_sinks_.erase(name);
}

bool DebugLog::set_log_sink_level(const std::string& name, LogLevel level) {
    std::lock_guard<std::mutex> lock(sinks_mutex_);
    if (log_sinks_.find(name) == log_sinks_.end()) {
        return false;
    }
    if (level < LOG_DEBUG || level > LOG_NONE) {
        return false;
    }
    auto it = log_sinks_.find(name);
    if (it != log_sinks_.end()) {
        it->second.min_level = level;
    }
    return true;
}

// ============================================
// 工具函数（public）
// ============================================

const char* DebugLog::level_to_string(LogLevel level) {
    switch (level) {
    case LOG_DEBUG: return "DEBUG";
    case LOG_INFO:  return "INFO";
    case LOG_WARN:  return "WARN";
    case LOG_ERROR: return "ERROR";
    default:        return "UNKNOWN";
    }
}

LogLevel DebugLog::int_to_log_level(int level) {
    switch (level) {
    case 0: return LOG_DEBUG;
    case 1: return LOG_INFO;
    case 2: return LOG_WARN;
    case 3: return LOG_ERROR;
    case 4: return LOG_NONE;
    default: return LOG_DEBUG;
    }
}
