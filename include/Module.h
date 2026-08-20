/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "ModuleValue.h"

#include <algorithm>
#include <string>
#include <vector>

// ============================================
// Module - 数据模块
// 一个模块包含一组可查询的数值（如 CS2 GSI 模块）
// ============================================
class Module {
public:
    // -------------------- 构造/析构 --------------------
    Module() = default;

    /// @brief 构造函数
    /// @param name 模块名称
    explicit Module(const std::string& name);

    // -------------------- 公共接口（属性获取）--------------------
    /// @brief 获取模块名称
    /// @return 模块名称
    inline const std::string& get_name() const { return name_; }

    /// @brief 获取数值列表（可变引用）
    /// @return 数值列表引用
    inline std::vector<ModuleValue>& get_values() { return values_; }

    /// @brief 获取数值列表（只读引用）
    /// @return 数值列表引用
    inline const std::vector<ModuleValue>& get_values() const { return values_; }

    /// @brief 获取模块内所有数值中最小的查询周期（毫秒）
    /// @return 最小周期毫秒数，无数值时返回 1000
    int get_min_period_ms() const;

    // -------------------- 公共接口（属性设置）--------------------
    /// @brief 添加一个数值到模块
    /// @param value 数值对象
    void add_value(const ModuleValue& value);

    /// @brief 统一设置模块内所有数值的查询周期
    /// @param period 查询周期
    void set_all_values_period(QueryPeriod period);

private:
    // -------------------- 成员变量 --------------------
    std::string name_;                  ///< 模块名称
    std::vector<ModuleValue> values_;   ///< 数值列表
};
