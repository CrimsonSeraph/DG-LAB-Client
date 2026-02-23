#pragma once

#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <sstream>
#include <typeinfo>

enum LogLevel {
    LOG_DEBUG = 0,
    LOG_INFO = 1,
    LOG_WARN = 2,
    LOG_ERROR = 3,
    LOG_NONE = 4
};

class DebugLog {
public:
    static DebugLog& Instance();

    DebugLog(const DebugLog&) = delete;
    DebugLog& operator=(const DebugLog&) = delete;

    // 统一日志等级
    void set_all_log_level(LogLevel level);
    void set_all_log_level(int level);
    // 设置日志是否输出全部
    void set_only_type_info(bool only_type_info);
    // 设置模块日志等级
    void set_log_level(const std::string& module, LogLevel level);
    // 获取模块日志等级
    LogLevel get_log_level(const std::string& module) const;
    // 核心输出
    void log(const std::string& module, const std::string& method, LogLevel level, const std::string& message);
    // 设置默认等级
    void set_default_log_level(LogLevel level);

private:
    DebugLog() = default;
    ~DebugLog() = default;

    const char* level_to_string(LogLevel level);

    mutable std::mutex mutex_;
    std::map<std::string, LogLevel> module_log_levels_;
    LogLevel default_log_level_ = LOG_DEBUG;
    bool is_only_type_info_ = false;
    static std::mutex out_put_mutex_;
};

#define LOG_MODULE(module, method, level, ...) \
    do { \
        if ((level) >= DebugLog::Instance().get_log_level(module)) { \
            std::ostringstream oss; \
            oss << __VA_ARGS__; \
            DebugLog::Instance().log(module, method, level, oss.str()); \
        } \
    } while (0)
