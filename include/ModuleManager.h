/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "Module.h"

#include <QObject>
#include <QString>

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// ============================================
// ModuleManager - 数值模块管理器（单例）
// 负责模块注册、数值查询、周期设置与数据源管理
// ============================================
class ModuleManager : public QObject {
    Q_OBJECT

public:
    // -------------------- 单例 --------------------
    /// @brief 获取单例实例
    static ModuleManager& instance();

    // -------------------- 初始化 --------------------
    /// @brief 注册默认模块（CS2 GSI）
    void init();

    // -------------------- 模块查询 --------------------
    /// @brief 获取所有模块名称
    /// @return 模块名称列表
    std::vector<std::string> get_module_names() const;

    /// @brief 按名称获取模块（只读）
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

    // -------------------- 周期设置 --------------------
    /// @brief 设置单个数值的查询周期
    /// @param module_name 模块名称
    /// @param value_id 数值 ID
    /// @param period 新的查询周期
    void set_value_period(const std::string& module_name, const std::string& value_id,
        QueryPeriod period);

    /// @brief 统一设置模块内所有数值的查询周期
    /// @param module_name 模块名称
    /// @param period 新的查询周期
    void set_module_period(const std::string& module_name, QueryPeriod period);

    /// @brief 统一设置所有模块所有数值的查询周期（模块页统一入口）
    /// @param period 新的查询周期
    void set_all_period(QueryPeriod period);

    // -------------------- 数据源 --------------------
    /// @brief 数据源回调类型（通过数值 ID 获取最新值）
    using DataSource = std::function<int(const std::string& value_id)>;

    /// @brief 设置数据源（默认使用模拟数据源，后续可替换为真实 GSI 数据）
    /// @param source 数据源回调
    void set_data_source(DataSource source);

    // -------------------- 查询 --------------------
    /// @brief 查询指定数值（立即查询并记录结果）
    /// @param module_name 模块名称
    /// @param value_id 数值 ID
    /// @return 查询到的数值，数值不存在返回 0
    int query_value(const std::string& module_name, const std::string& value_id);

    /// @brief 获取当前最短查询周期（所有数值中的最短周期）
    /// @return 最短周期毫秒数
    int get_base_period_ms() const;

signals:
    /// @brief 查询周期设置变化时发出（用于刷新界面周期显示）
    void period_changed();

private:
    // -------------------- 构造/析构（单例私有）--------------------
    ModuleManager();
    ~ModuleManager() override;

    // -------------------- 私有辅助函数 --------------------
    /// @brief 注册默认模块（CS2 GSI 数值，参照官方 GSI 规范）
    void register_default_modules();
    /// @brief 默认模拟数据源（真实 GSI 接入前的占位实现）
    /// @param value_id 数值 ID
    /// @return 模拟数值
    static int default_data_source(const std::string& value_id);

    // -------------------- 成员变量 --------------------
    std::vector<Module> modules_;         ///< 模块列表
    mutable std::mutex mutex_;            ///< 保护模块数据
    DataSource data_source_;              ///< 数据源回调
};
