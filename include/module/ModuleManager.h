/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "Module.h"
#include "PluginHost.h"

#include <QObject>
#include <QString>
#include <QTimer>

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// 前置声明
class QLibrary;
class IPlugin;

// ============================================
// ModuleManager - 数值模块管理器（单例）
// 负责模块注册、周期设置、以最短周期为基准的调度查询与数值变化推送；
// 同时作为插件宿主：扫描/加载/卸载动态库插件（IPlugin），
// 注入日志回调与宿主上下文（IPluginHost），管理插件加载状态
// ============================================
class ModuleManager : public QObject, public IPluginHost {
    Q_OBJECT

public:
    // -------------------- 插件加载状态 --------------------
    enum class PluginLoadState {
        NotLoaded = 0, ///< 已扫描、未加载
        Loaded,        ///< 已加载（initialize 成功）
        Failed         ///< 加载失败（error 记录原因）
    };

    /// @brief 插件状态信息（供 UI 展示）
    struct PluginInfo {
        std::string file_name;                              ///< 插件文件名（唯一标识）
        std::string file_path;                              ///< 插件完整路径
        std::string display_name;                           ///< 显示名称（加载后为插件 name()，未加载为文件名）
        std::string version;                                ///< 插件版本（未加载为空）
        PluginLoadState state = PluginLoadState::NotLoaded; ///< 加载状态
        std::string error;                                  ///< 加载失败原因（成功为空）
    };

    // -------------------- 单例 --------------------
    /// @brief 获取单例实例
    static ModuleManager& instance();

    // -------------------- 初始化 --------------------
    /// @brief 初始化：注册默认模块、扫描插件目录（可选扫描即加载）并启动周期调度器
    void init();

    // -------------------- 模块查询 --------------------
    /// @brief 获取所有模块名称
    /// @return 模块名称列表
    std::vector<std::string> get_module_names() const;

    /// @brief 按名称获取模块（只读；插件加载/卸载后指针可能失效，勿长期持有）
    /// @param module_name 模块名称
    /// @return 模块指针，不存在返回 nullptr
    const Module* get_module(const std::string& module_name) const;

    /// @brief 按模块名与数值 ID 获取数值（只读）
    /// @param module_name 模块名称
    /// @param value_id 数值 ID
    /// @return 数值指针，不存在返回 nullptr
    const ModuleValue* get_value(const std::string& module_name, const std::string& value_id) const;

    /// @brief 获取模块内所有数值中最小的查询周期（毫秒）
    /// @param module_name 模块名称
    /// @return 最小周期毫秒数，模块不存在返回 1000
    int get_module_min_period_ms(const std::string& module_name) const;

    /// @brief 按数值 ID 全局查找所在模块（跨模块遍历）
    /// @param value_id 数值 ID
    /// @return 模块名称，未找到返回空字符串
    std::string find_module_by_value_id(const std::string& value_id) const;

    /// @brief 获取挂载到指定通道的模块名称列表
    /// @param channel 通道（"A"/"B"）
    /// @return 模块名称列表
    std::vector<std::string> get_modules_for_channel(const std::string& channel) const;

    // -------------------- 周期设置 --------------------
    /// @brief 设置单个数值的查询周期（设置后自动重建调度器）
    /// @param module_name 模块名称
    /// @param value_id 数值 ID
    /// @param period 新的查询周期
    void set_value_period(const std::string& module_name, const std::string& value_id,
        QueryPeriod period);

    /// @brief 统一设置模块内所有数值的查询周期（设置后自动重建调度器）
    /// @param module_name 模块名称
    /// @param period 新的查询周期
    void set_module_period(const std::string& module_name, QueryPeriod period);

    /// @brief 统一设置所有模块所有数值的查询周期（模块页统一入口，自动重建调度器）
    /// @param period 新的查询周期
    void set_all_period(QueryPeriod period);

    // -------------------- 数据源 --------------------
    /// @brief 数据源回调类型（通过数值 ID 获取最新值）
    using DataSource = std::function<int(const std::string& value_id)>;

    /// @brief 设置数据源（默认使用模拟数据源，后续可替换为真实 GSI 数据）
    /// @param source 数据源回调
    void set_data_source(DataSource source);

    // -------------------- 查询 --------------------
    /// @brief 手动查询指定数值（立即查询，若变化则推送）
    /// @param module_name 模块名称
    /// @param value_id 数值 ID
    /// @return 查询到的数值，数值不存在返回 0
    int query_value(const std::string& module_name, const std::string& value_id);

    /// @brief 外部写入数值（如 GSI 数据接收后），变化时触发推送
    /// @param module_name 模块名称
    /// @param value_id 数值 ID
    /// @param value 最新数值
    void set_value(const std::string& module_name, const std::string& value_id, int value);

    /// @brief 获取当前调度基准周期（所有数值中的最短查询周期）
    /// @return 基准周期毫秒数
    int get_base_period_ms() const;

    // -------------------- 插件管理 --------------------
    /// @brief 获取插件扫描目录（config app.module.path，默认 <程序目录>/module）
    /// @return 插件目录绝对路径
    std::string get_plugin_dir() const;

    /// @brief 是否启用"扫描即加载"（config app.module.scan_load）
    /// @return 启用返回 true
    bool get_scan_load() const;

    /// @brief 获取全部插件状态信息（含未加载候选）
    /// @return 插件状态列表
    std::vector<PluginInfo> get_plugins() const;

    /// @brief 加载指定插件（按文件名）
    /// @param file_name 插件文件名
    /// @return 加载成功返回 true
    bool load_plugin(const std::string& file_name);

    /// @brief 卸载指定插件（按文件名）
    /// @param file_name 插件文件名
    /// @return 卸载成功返回 true
    bool unload_plugin(const std::string& file_name);

    /// @brief 判断指定插件是否已加载
    /// @param file_name 插件文件名
    /// @return 已加载返回 true
    bool is_plugin_loaded(const std::string& file_name) const;

    // -------------------- 宿主能力（IPluginHost 实现）--------------------
    /// @brief 注册插件数值到模块管理器（插件通过 IPluginHost 调用）
    /// @param module_name 模块名称
    /// @param values 数值列表
    /// @param channels 挂载通道列表
    /// @return 成功返回 true
    bool register_module_values(const std::string& module_name,
        const std::vector<ModuleValue>& values,
        const std::vector<std::string>& channels) override;

    /// @brief 注销模块（插件卸载时调用）
    /// @param module_name 模块名称
    void unregister_module(const std::string& module_name) override;

    /// @brief 获取宿主共享的数据接收器（懒创建）
    /// @return 数据接收器指针
    DataListener* data_listener() override;

signals:
    /// @brief 数值变化时发出（用于触发规则计算与界面刷新）
    /// @param module_name 模块名称
    /// @param value_id 数值 ID
    /// @param new_value 最新数值
    void value_changed(const QString& module_name, const QString& value_id, int new_value);

    /// @brief 查询周期设置变化时发出（用于刷新界面周期显示）
    void period_changed();

    /// @brief 插件加载/卸载/失败时发出（用于刷新模块页与插件状态显示）
    void plugin_state_changed();

private slots:
    /// @brief 调度定时器触发（按基准周期执行到期数值的查询）
    void on_timer_tick();

private:
    /// @brief 单个插件条目（内部结构）
    struct PluginEntry {
        std::string file_name;                              ///< 插件文件名
        std::string file_path;                              ///< 插件完整路径
        QLibrary* library = nullptr;                        ///< 动态库句柄（未加载为空）
        IPlugin* instance = nullptr;                        ///< 插件实例（未加载为空）
        PluginLoadState state = PluginLoadState::NotLoaded; ///< 加载状态
        std::string error;                                  ///< 加载失败原因
        std::string display_name;                           ///< 显示名称（加载后为插件 name()）
        std::string version;                                ///< 插件版本
    };

    // -------------------- 构造/析构（单例私有）--------------------
    ModuleManager();
    ~ModuleManager() override;

    // -------------------- 私有辅助函数 --------------------
    /// @brief 注册默认模块（CS2 GSI 数值，参照官方 GSI 规范）
    void register_default_modules();
    /// @brief 重建调度器（以所有数值中最短查询周期为基准）
    void rebuild_scheduler();
    /// @brief 查询单个数值并检测变化（需已持有锁）
    /// @param module 模块引用
    /// @param value 数值引用
    /// @return 是否发生变化
    bool query_value_locked(Module& module, ModuleValue& value);

    /// @brief 解析插件扫描目录（相对路径相对于程序目录解析）
    /// @return 插件目录绝对路径
    std::string resolve_plugin_dir() const;
    /// @brief 扫描插件目录（*.dll/*.so/*.dylib），重建插件候选列表
    void scan_plugins();
    /// @brief 加载单个插件条目（动态加载、版本校验、依赖校验、初始化）
    /// @param entry 插件条目
    /// @return 加载成功返回 true
    bool load_plugin_entry(PluginEntry& entry);
    /// @brief 卸载单个插件条目（卸载前检查、反初始化、资源清理、销毁与卸载）
    /// @param entry 插件条目
    void unload_plugin_entry(PluginEntry& entry);
    /// @brief 按文件名查找插件条目
    /// @param file_name 插件文件名
    /// @return 插件条目指针，不存在返回 nullptr
    PluginEntry* find_plugin_entry(const std::string& file_name);
    /// @brief 按文件名查找插件条目（只读）
    /// @param file_name 插件文件名
    /// @return 插件条目指针，不存在返回 nullptr
    const PluginEntry* find_plugin_entry(const std::string& file_name) const;

    // -------------------- 成员变量 --------------------
    std::vector<Module> modules_; ///< 模块列表
    mutable std::mutex mutex_;    ///< 保护模块数据（plugins_ 仅主线程访问）
    QTimer* timer_ = nullptr;     ///< 调度定时器
    int tick_count_ = 0;          ///< 调度计数（以基准周期递增）
    int base_period_ms_ = 1000;   ///< 基准周期（毫秒）
    DataSource data_source_;      ///< 数据源回调

    std::vector<PluginEntry> plugins_;        ///< 插件条目列表（仅主线程访问）
    std::string plugin_dir_;                  ///< 插件扫描目录（绝对路径）
    bool scan_load_ = false;                  ///< 扫描即加载开关
    DataListener* shared_listener_ = nullptr; ///< 宿主共享数据接收器
};
