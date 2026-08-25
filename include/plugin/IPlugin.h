/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "plugin_export.h"

#include <cstdint>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

// ============================================
// PluginLogLevel - 插件日志级别（与主程序 DebugLog 等级对应，加载时由宿主映射）
// ============================================
enum class PluginLogLevel {
    Debug = 0, ///< 调试
    Info = 1,  ///< 信息
    Warn = 2,  ///< 警告
    Error = 3, ///< 错误
    None = 4   ///< 不输出
};

// ============================================
// PluginError - 插件错误码
// 健壮性约定：所有可能失败的操作返回错误码而非抛出异常
// ============================================
enum class PluginError {
    Ok = 0,             ///< 成功
    InvalidArgument,    ///< 参数无效
    NotInitialized,     ///< 未初始化
    AlreadyInitialized, ///< 重复初始化
    DependencyMissing,  ///< 依赖缺失
    ResourceError,      ///< 资源错误（文件、端口、内存等）
    NotSupported,       ///< 不支持的操作
    Unknown             ///< 未知错误
};

// ============================================
// PluginCapability - 插件能力标志（自描述：声明插件提供的能力）
// ============================================
enum class PluginCapability : std::uint32_t {
    None = 0,                 ///< 无特殊能力
    ProvidesValues = 1u << 0, ///< 提供可查询数值（挂载到 ModuleManager）
    ConsumesData = 1u << 1,   ///< 消费外部数据（注册 DataListener 数据处理器）
    HasSettingsUi = 1u << 2   ///< 提供设置界面
};

/// @brief 能力标志按位或
/// @param a 能力标志
/// @param b 能力标志
/// @return 合并后的能力标志
inline PluginCapability operator|(PluginCapability a, PluginCapability b) {
    return static_cast<PluginCapability>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}

/// @brief 判断能力标志是否包含指定能力
/// @param flags 能力标志组合
/// @param cap 待检查的能力
/// @return 包含返回 true
inline bool has_capability(PluginCapability flags, PluginCapability cap) {
    return (static_cast<std::uint32_t>(flags) & static_cast<std::uint32_t>(cap)) != 0;
}

// ============================================
// PluginThreadSafety - 插件线程安全声明（宿主据此决定调用方式）
// ============================================
enum class PluginThreadSafety {
    SingleThreaded = 0, ///< 仅可在主线程调用
    Reentrant,          ///< 可多线程调用，但不同实例之间不共享状态
    ThreadSafe          ///< 完全线程安全（任意线程并发调用）
};

// 前置声明
class IPlugin;

/// @brief 日志回调类型：插件通过回调向主程序上报日志（含级别、插件名、函数名、信息）
/// @param level 日志级别
/// @param plugin_name 插件名称
/// @param function 函数名
/// @param message 日志信息
using PluginLogCallback = std::function<void(PluginLogLevel level,
    const std::string& plugin_name, const std::string& function,
    const std::string& message)>;

// ============================================
// IPlugin - 插件纯虚基类
// 所有可动态加载的模块必须实现本接口，并通过 extern "C" 的
// create_plugin / destroy_plugin / get_plugin_api_version 导出。
// 内存隔离约定：插件实例由插件内部 new，宿主仅调用 destroy_plugin 销毁，
// 禁止跨模块 new/delete，插件内禁止使用静态全局变量。
// ============================================
class PLUGIN_API IPlugin {
public:
    virtual ~IPlugin() = default;

    // -------------------- 自描述 --------------------
    /// @brief 获取插件名称（用于 UI 显示与日志类名）
    /// @return 插件名称
    virtual std::string name() const = 0;

    /// @brief 获取插件版本号
    /// @return 版本号（如 "0.1.0"）
    virtual std::string version() const = 0;

    /// @brief 获取插件 API 版本（宿主加载时与 PLUGIN_API_VERSION 校验）
    /// @return API 版本号
    virtual int api_version() const { return PLUGIN_API_VERSION; }

    /// @brief 获取插件能力标志
    /// @return 能力标志组合
    virtual PluginCapability capabilities() const { return PluginCapability::None; }

    /// @brief 获取依赖的插件名称列表（宿主加载前校验依赖是否已加载）
    /// @return 依赖插件名称列表
    virtual std::vector<std::string> dependencies() const { return {}; }

    // -------------------- 线程安全声明 --------------------
    /// @brief 声明插件的线程安全级别（宿主据此决定调用方式）
    /// @return 线程安全级别
    virtual PluginThreadSafety thread_safety() const {
        return PluginThreadSafety::SingleThreaded;
    }

    // -------------------- 生命周期 --------------------
    /// @brief 初始化：分配资源、启动服务、注册数值与数据处理器
    /// @return 成功返回 PluginError::Ok，失败返回对应错误码
    virtual PluginError initialize() = 0;

    /// @brief 反初始化：停止服务、注销注册（与 initialize 对称）
    virtual void uninitialize() = 0;

    /// @brief 资源清理：释放内部资源（销毁前最后一步）
    virtual void cleanup() {}

    /// @brief 卸载前检查：确认当前状态允许动态卸载
    /// @return 允许卸载返回 true
    virtual bool can_unload() const { return true; }

    // -------------------- 日志转发 --------------------
    /// @brief 设置日志回调（宿主加载插件后调用；未设置时 log 静默丢弃）
    /// @param callback 日志回调
    void set_log_callback(PluginLogCallback callback) { log_callback_ = std::move(callback); }

    /// @brief 上报日志（宿主统一通过 LOG_MODULE 记录，类名为插件名称、方法名为函数名）
    /// @param level 日志级别
    /// @param function 函数名（通常传 __func__）
    /// @param message 日志信息
    void log(PluginLogLevel level, const std::string& function, const std::string& message) {
        if (log_callback_) {
            log_callback_(level, name(), function, message);
        }
    }

protected:
    PluginLogCallback log_callback_; ///< 日志回调（宿主注入）
};

// ============================================
// PLUGIN_LOG - 插件日志宏（插件内部使用，不直接使用主程序 LOG_MODULE）
// 用法: PLUGIN_LOG(this, PluginLogLevel::Info, "消息" << 变量);
// ============================================
#define PLUGIN_LOG(plugin, level, ...)             \
    do {                                           \
        std::ostringstream oss;                    \
        oss << __VA_ARGS__;                        \
        (plugin)->log(level, __func__, oss.str()); \
    } while (0)

// ============================================
// 插件导出函数（仅在构建插件动态库时声明，宿主通过 QLibrary 解析）
// ============================================
#if defined(PLUGIN_BUILD)
extern "C" {
/// @brief 获取插件 API 版本（宿主加载时校验）
/// @return 插件 API 版本号
PLUGIN_EXPORT int get_plugin_api_version();

/// @brief 创建插件实例（宿主调用；返回的实例由 destroy_plugin 销毁）
/// @return 插件实例指针
PLUGIN_EXPORT IPlugin* create_plugin();

/// @brief 销毁插件实例（插件自行 delete，宿主不得直接 delete）
/// @param plugin 插件实例指针
PLUGIN_EXPORT void destroy_plugin(IPlugin* plugin);
}
#endif
