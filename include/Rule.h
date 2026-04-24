/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QJsonObject>

#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// ============================================
// Rule - 规则类，表示一条强度控制规则
// ============================================
class Rule {
public:
    // -------------------- 构造/析构 --------------------
    Rule() = default;

    /// @brief 构造函数
    /// @param name 规则名称
    /// @param channel 通道（"A"/"B"/空）
    /// @param mode 模式（0-4）
    /// @param value_pattern 值计算表达式，包含 {} 占位符
    Rule(const std::string& name,
        const std::string& channel,
        int mode,
        const std::string& value_pattern);

    // -------------------- 公共接口（属性获取）--------------------
    inline const std::string& get_name() const { return name_; }
    inline const std::string& get_channel() const { return channel_; }
    inline int get_mode() const { return mode_; }
    inline const std::string& get_value_pattern() const { return value_pattern_; }

    // -------------------- 公共接口（静态工具）--------------------
    /// @brief 规范化通道输入，接受 "A"/"a"/"B"/"b"，返回 "A"/"B" 或空字符串（无效）
    static std::string normalize_channel(const std::string& channel);

    // -------------------- 公共接口（计算）--------------------
    /// @brief 获取占位符数量
    size_t get_placeholder_count() const;

    /// @brief 计算值: 将参数填入表达式并求值，根据模式钳位
    /// @param values 参数列表（数量必须匹配占位符）
    /// @return 计算结果（已钳位）
    int compute_value(const std::vector<int>& values) const;

    /// @brief 生成完整的 QJsonObject 命令
    /// @param values 参数列表
    /// @return JSON 命令对象
    QJsonObject generate_command(const std::vector<int>& values) const;

    /// @brief 获取用于 UI 显示的字符串
    std::string get_display_string() const;

private:
    // -------------------- 成员变量 --------------------
    std::string name_;  ///< 规则名称
    std::string channel_;   ///< 通道 "A"/"B"/""
    int mode_;  ///< 模式 0-4
    std::string value_pattern_; ///< 包含 {} 的表达式
    std::vector<size_t> placeholder_positions_; ///< 各占位符位置
    size_t placeholder_count_ = 0;  ///< 占位符数量

    // -------------------- 私有辅助函数 --------------------
    /// @brief 解析 value_pattern_ 中的 {}，记录位置和数量
    void parse_pattern();

    /// @brief 将参数填入表达式，返回替换后的字符串（不计算）
    std::string evaluate_value_pattern(const std::vector<int>& values) const;
};
