/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QJsonObject>

#include <optional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// ============================================
// 父级类型与父级结构
// ============================================
enum class ParentType {
    NONE = 0,   ///< 无父级
    CHANNEL = 1, ///< 通道父级（A/B）
    RULE = 2    ///< 规则父级（按规则序号引用）
};

/// @brief 规则父级（下游接收者：规则结果推送给父级）
struct RuleParent {
    ParentType type = ParentType::NONE; ///< 父级类型
    std::string channel;                ///< CHANNEL 类型时的通道（"A"/"B"）
    int rule_index = -1;                ///< RULE 类型时的规则序号
};

// ============================================
// 值模式占位符类型与结构
// ============================================
enum class PlaceholderType {
    EXTERNAL = 0, ///< 外部参数占位符（{}）
    ID_REF = 1,   ///< 模块数值引用（{id:xxx(名称)}）
    RULE_REF = 2  ///< 规则结果引用（{rule:xx}）
};

/// @brief 值模式中的占位符
struct Placeholder {
    PlaceholderType type = PlaceholderType::EXTERNAL; ///< 占位符类型
    std::string id;                                   ///< ID_REF 的数值 ID
    int rule_index = -1;                              ///< RULE_REF 的规则序号
    size_t pos = 0;                                   ///< 在表达式中的起始位置
    size_t len = 0;                                   ///< 占位符长度（含括号注释）
};

// ============================================
// Rule - 规则类，表示一条强度控制规则
// ============================================
class Rule {
public:
    // -------------------- 构造/析构 --------------------
    Rule() = default;

    /// @brief 构造函数
    /// @param name 规则名称
    /// @param channel 通道（"A"/"B"/空，兼容旧格式）
    /// @param mode 模式（0-4）
    /// @param value_pattern 值计算表达式，包含占位符
    /// @param enabled 是否启用
    /// @param parents 父级列表（默认使用 channel 转换）
    /// @param index 规则序号（1 起始）
    Rule(const std::string& name,
        const std::string& channel,
        int mode,
        const std::string& value_pattern,
        bool enabled = true,
        const std::vector<RuleParent>& parents = {},
        int index = 0);

    // -------------------- 公共接口（属性获取）--------------------
    inline const std::string& get_name() const { return name_; }
    inline const std::string& get_channel() const { return channel_; }
    inline int get_mode() const { return mode_; }
    inline const std::string& get_value_pattern() const { return value_pattern_; }
    inline bool get_enabled() const { return enabled_; }
    inline int get_index() const { return index_; }
    inline const std::vector<RuleParent>& get_parents() const { return parents_; }

    // -------------------- 公共接口（属性设置）--------------------
    /// @brief 设置启用状态
    /// @param enabled 是否启用
    inline void set_enabled(bool enabled) { enabled_ = enabled; }

    /// @brief 设置规则序号
    /// @param index 规则序号
    inline void set_index(int index) { index_ = index; }

    /// @brief 设置父级列表
    /// @param parents 父级列表
    inline void set_parents(const std::vector<RuleParent>& parents) { parents_ = parents; }

    // -------------------- 公共接口（父级查询）--------------------
    /// @brief 判断父级列表是否包含指定通道
    /// @param channel 通道（"A"/"B"）
    /// @return 包含返回 true
    bool has_parent_channel(const std::string& channel) const;

    /// @brief 判断父级列表是否包含指定规则序号
    /// @param rule_index 规则序号
    /// @return 包含返回 true
    bool has_parent_rule(int rule_index) const;

    /// @brief 判断父级是否全部为通道
    /// @return 父级非空且全部为通道返回 true
    bool is_parents_all_channel() const;

    /// @brief 判断父级是否部分为通道（通道与规则混合）
    /// @return 父级中同时存在通道与规则返回 true
    bool is_parents_partial_channel() const;

    // -------------------- 公共接口（静态工具）--------------------
    /// @brief 规范化通道输入，接受 "A"/"a"/"B"/"b"，返回 "A"/"B" 或空字符串（无效）
    static std::string normalize_channel(const std::string& channel);

    // -------------------- 公共接口（值模式）--------------------
    /// @brief 判断值模式是否为空（空值模式调用计算时忽略该项）
    /// @return 值模式为空字符串返回 true
    bool is_value_pattern_empty() const;

    /// @brief 获取占位符列表（按表达式顺序）
    /// @return 占位符列表
    inline const std::vector<Placeholder>& get_placeholders() const { return placeholders_; }

    /// @brief 获取占位符数量
    size_t get_placeholder_count() const;

    // -------------------- 公共接口（计算）--------------------
    /// @brief 计算值: 将参数填入表达式并求值，根据模式钳位（旧式 {} 占位符）
    /// @param values 参数列表（数量必须匹配占位符）
    /// @return 计算结果（已钳位）
    int compute_value(const std::vector<int>& values) const;

    /// @brief 计算值: 支持空值语义，任一占位符为空则返回空（调用方忽略该项）
    /// @param values 参数列表（与占位符一一对应，可为空值）
    /// @return 计算结果（可选），任一参数为空返回 std::nullopt
    std::optional<int> compute_value(const std::vector<std::optional<int>>& values) const;

    /// @brief 生成完整的 QJsonObject 命令
    /// @param values 参数列表
    /// @return JSON 命令对象
    QJsonObject generate_command(const std::vector<int>& values) const;

    /// @brief 生成完整命令（空值语义版本）
    /// @param values 参数列表（可为空值）
    /// @return 命令对象（可选），任一参数为空返回 std::nullopt
    std::optional<QJsonObject> generate_command(const std::vector<std::optional<int>>& values) const;

    /// @brief 获取用于 UI 显示的字符串
    std::string get_display_string() const;

private:
    // -------------------- 成员变量 --------------------
    std::string name_;                          ///< 规则名称
    std::string channel_;                       ///< 通道 "A"/"B"/""（兼容旧格式）
    int mode_;                                  ///< 模式 0-4
    std::string value_pattern_;                 ///< 包含占位符的表达式
    bool enabled_ = true;                       ///< 是否启用
    int index_ = 0;                             ///< 规则序号（1 起始）
    std::vector<RuleParent> parents_;           ///< 父级列表
    std::vector<Placeholder> placeholders_;     ///< 占位符列表
    std::vector<size_t> placeholder_positions_; ///< 各外部占位符位置（旧式 {}）
    size_t placeholder_count_ = 0;              ///< 占位符数量

    // -------------------- 私有辅助函数 --------------------
    /// @brief 解析 value_pattern_ 中的占位符（{id:xxx}/{rule:xx}/{}），记录位置与类型
    void parse_pattern();

    /// @brief 将参数填入表达式，返回替换后的字符串（不计算）
    /// @param values 参数列表（与占位符一一对应）
    /// @return 替换后的表达式，任一参数为空返回空字符串
    std::string evaluate_value_pattern(const std::vector<std::optional<int>>& values) const;

    /// @brief 对原始表达式求值并钳位
    /// @param expr 已替换占位符的表达式
    /// @return 计算结果（已钳位）
    int evaluate_expression(const std::string& expr) const;
};