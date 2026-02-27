#pragma once

#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <sstream>
#include <typeinfo>
#include <functional>

enum LogLevel {
    LOG_DEBUG = 0,
    LOG_INFO = 1,
    LOG_WARN = 2,
    LOG_ERROR = 3,
    LOG_NONE = 4
};

using LogSinkCallback = std::function<void(const std::string& module,
    const std::string& method, LogLevel level, const std::string& message)>;

struct LogSink {
    LogSinkCallback callback;
    LogLevel min_level;
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
    // 注册 Sink
    void register_log_sink(const std::string& name, const LogSink& sink);
    // 注销 Sink
    void unregister_log_sink(const std::string& name);
    // 设置某个 Sink 的最小日志等级
    void set_log_sink_level(const std::string& name, LogLevel level);

    const char* level_to_string(LogLevel level);
    inline bool is_only_type_info() const { return is_only_type_info_; }

private:
    DebugLog() = default;
    ~DebugLog() = default;

    mutable std::mutex mutex_;
    std::map<std::string, LogLevel> module_log_levels_;
    LogLevel default_log_level_ = LOG_DEBUG;
    bool is_only_type_info_ = false;
    std::map<std::string, LogSink> log_sinks_;
    mutable std::mutex sinks_mutex_;
};

#define LOG_MODULE(module, method, level, ...) \
    do { \
        LogLevel moduleLevel = DebugLog::Instance().get_log_level(module); \
        bool shouldLog = DebugLog::Instance().is_only_type_info() ? (level == moduleLevel) : (level >= moduleLevel); \
        if (shouldLog) { \
            std::ostringstream oss; \
            oss << __VA_ARGS__; \
            DebugLog::Instance().log(module, method, level, oss.str()); \
        } \
    } while (0)
