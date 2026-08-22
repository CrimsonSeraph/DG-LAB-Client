/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <string>

// ============================================
// 查询周期枚举
// ============================================
enum class QueryPeriod {
    QUARTER_SECOND = 250, ///< 四分之一秒
    HALF_SECOND = 500,    ///< 每半秒
    SECOND = 1000,        ///< 每秒
    TWO_SECONDS = 2000,   ///< 每两秒
    FOUR_SECONDS = 4000   ///< 每四秒
};

/// @brief 查询周期转毫秒数
/// @param period 查询周期枚举
/// @return 周期对应的毫秒数
inline int query_period_to_ms(QueryPeriod period) {
    return static_cast<int>(period);
}

/// @brief 查询周期转中文显示文本
/// @param period 查询周期枚举
/// @return 中文文本（如 "每秒"）
inline const char* query_period_to_text(QueryPeriod period) {
    switch (period) {
    case QueryPeriod::QUARTER_SECOND: return "四分之一秒";
    case QueryPeriod::HALF_SECOND: return "每半秒";
    case QueryPeriod::SECOND: return "每秒";
    case QueryPeriod::TWO_SECONDS: return "每两秒";
    case QueryPeriod::FOUR_SECONDS: return "每四秒";
    }
    return "每秒";
}

/// @brief 毫秒数转查询周期枚举
/// @param ms 毫秒数
/// @return 查询周期枚举，无法匹配时返回默认值 SECOND
inline QueryPeriod query_period_from_ms(int ms) {
    switch (ms) {
    case 250: return QueryPeriod::QUARTER_SECOND;
    case 500: return QueryPeriod::HALF_SECOND;
    case 2000: return QueryPeriod::TWO_SECONDS;
    case 4000: return QueryPeriod::FOUR_SECONDS;
    default: return QueryPeriod::SECOND;
    }
}

/// @brief 中文显示文本转查询周期枚举
/// @param text 中文文本（如 "每秒"）
/// @return 查询周期枚举，无法识别时返回默认值 SECOND
inline QueryPeriod query_period_from_text(const std::string& text) {
    if (text == "四分之一秒") return QueryPeriod::QUARTER_SECOND;
    if (text == "每半秒") return QueryPeriod::HALF_SECOND;
    if (text == "每两秒") return QueryPeriod::TWO_SECONDS;
    if (text == "每四秒") return QueryPeriod::FOUR_SECONDS;
    return QueryPeriod::SECOND;
}

// ============================================
// ModuleValue - 单个可查询数值
// 描述一个数值的名称、ID、查询周期与上次查询结果
// ============================================
class ModuleValue {
public:
    // -------------------- 构造/析构 --------------------
    ModuleValue() = default;

    /// @brief 构造函数
    /// @param id 数值 ID（参照 CS2 GSI 规范，如 "health"）
    /// @param name 数值中文名称（如 "当前血量"）
    /// @param period 查询周期
    /// @param field 底层字段名（用于展示，如 "m_iHealth"）
    ModuleValue(const std::string& id, const std::string& name, QueryPeriod period,
        const std::string& field);

    // -------------------- 公共接口（属性获取）--------------------
    /// @brief 获取数值 ID（用于规则引用，如 {id:health}）
    /// @return 数值 ID
    inline const std::string& get_id() const { return id_; }

    /// @brief 获取数值中文名称
    /// @return 数值名称
    inline const std::string& get_name() const { return name_; }

    /// @brief 获取查询周期
    /// @return 查询周期枚举
    inline QueryPeriod get_query_period() const { return query_period_; }

    /// @brief 获取底层字段名（如 m_iHealth，用于界面展示）
    /// @return 底层字段名
    inline const std::string& get_field() const { return field_; }

    /// @brief 获取上次查询到的数值
    /// @return 上次查询值
    inline int get_last_value() const { return last_value_; }

    /// @brief 是否已获取过数值
    /// @return 已获取返回 true
    inline bool get_has_value() const { return has_value_; }

    // -------------------- 公共接口（属性设置）--------------------
    /// @brief 设置查询周期
    /// @param period 新的查询周期
    inline void set_query_period(QueryPeriod period) { query_period_ = period; }

    /// @brief 记录最新查询到的数值
    /// @param value 查询到的数值
    inline void set_last_value(int value) {
        last_value_ = value;
        has_value_ = true;
    }

private:
    // -------------------- 成员变量 --------------------
    std::string id_;                                        ///< 数值 ID（如 "health"）
    std::string name_;                                      ///< 数值中文名称（如 "当前血量"）
    QueryPeriod query_period_ = QueryPeriod::SECOND;        ///< 查询周期
    std::string field_;                                     ///< 底层字段名（如 "m_iHealth"）
    int last_value_ = 0;                                    ///< 上次查询到的数值
    bool has_value_ = false;                                ///< 是否已获取过数值
};