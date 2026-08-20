/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "Rule.h"

#include "DebugLog.h"

#include <QJSEngine>
#include <QJsonObject>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <stdexcept>

// ============================================
// 构造/析构（public）
// ============================================

Rule::Rule(const std::string& name, const std::string& channel, int mode,
    const std::string& value_pattern, bool enabled, const std::vector<RuleParent>& parents,
    int index)
    : name_(name)
    , channel_(normalize_channel(channel))
    , mode_(mode)
    , value_pattern_(value_pattern)
    , enabled_(enabled)
    , index_(index)
    , parents_(parents) {
    // 旧格式兼容：未显式指定父级时，使用通道字段作为唯一父级
    if (parents_.empty() && !channel_.empty()) {
        RuleParent parent;
        parent.type = ParentType::CHANNEL;
        parent.channel = channel_;
        parents_.push_back(parent);
    }
    parse_pattern();
}

// ============================================
// 公共接口实现（public）
// ============================================

bool Rule::has_parent_channel(const std::string& channel) const {
    std::string norm = normalize_channel(channel);
    for (const auto& parent : parents_) {
        if (parent.type == ParentType::CHANNEL && parent.channel == norm) {
            return true;
        }
    }
    return false;
}

bool Rule::has_parent_rule(int rule_index) const {
    for (const auto& parent : parents_) {
        if (parent.type == ParentType::RULE && parent.rule_index == rule_index) {
            return true;
        }
    }
    return false;
}

bool Rule::is_parents_all_channel() const {
    if (parents_.empty()) {
        return false;
    }
    for (const auto& parent : parents_) {
        if (parent.type != ParentType::CHANNEL) {
            return false;
        }
    }
    return true;
}

bool Rule::is_parents_partial_channel() const {
    bool has_channel = false;
    bool has_rule = false;
    for (const auto& parent : parents_) {
        if (parent.type == ParentType::CHANNEL) {
            has_channel = true;
        }
        else if (parent.type == ParentType::RULE) {
            has_rule = true;
        }
    }
    return has_channel && has_rule;
}

std::string Rule::normalize_channel(const std::string& channel) {
    std::string result = channel;
    // 去除首尾空格
    size_t start = result.find_first_not_of(" 	");
    if (start == std::string::npos) return "";
    size_t end = result.find_last_not_of(" 	");
    result = result.substr(start, end - start + 1);
    // 转为大写
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char c) { return std::toupper(c); });
    if (result == "A" || result == "B") {
        return result;
    }
    return "";
}

bool Rule::is_value_pattern_empty() const {
    return value_pattern_.empty();
}

size_t Rule::get_placeholder_count() const {
    return placeholder_count_;
}

// -------------------- 计算 --------------------

int Rule::compute_value(const std::vector<int>& params) const {
    std::vector<std::optional<int>> optional_params;
    optional_params.reserve(params.size());
    for (int value : params) {
        optional_params.push_back(value);
    }
    auto result = compute_value(optional_params);
    return result.value_or(0);
}

std::optional<int> Rule::compute_value(const std::vector<std::optional<int>>& values) const {
    // 空值模式：调用计算时忽略该项
    if (is_value_pattern_empty()) {
        LOG_MODULE("Rule", "computeValue", LOG_WARN, "规则 " << name_ << " 值模式为空，忽略计算");
        return std::nullopt;
    }
    std::string expr = evaluate_value_pattern(values);
    if (expr.empty()) {
        // 任一占位符为空值，该项被忽略（返回空，由调用方决定是否跳过）
        LOG_MODULE("Rule", "computeValue", LOG_DEBUG,
            "规则 " << name_ << " 存在空值占位符，本次计算被忽略");
        return std::nullopt;
    }
    return evaluate_expression(expr);
}

QJsonObject Rule::generate_command(const std::vector<int>& params) const {
    std::vector<std::optional<int>> optional_params;
    optional_params.reserve(params.size());
    for (int value : params) {
        optional_params.push_back(value);
    }
    auto result = generate_command(optional_params);
    return result.value_or(QJsonObject());
}

std::optional<QJsonObject> Rule::generate_command(const std::vector<std::optional<int>>& params) const {
    auto value = compute_value(params);
    if (!value.has_value()) {
        return std::nullopt;
    }
    QJsonObject cmd;
    cmd["cmd"] = "send_strength";

    // 通道处理（命令通道取首个通道父级）
    if (!channel_.empty()) {
        QString ch = QString::fromStdString(channel_).toUpper();
        if (ch == "A" || ch == "B")
            cmd["channel"] = ch;
        else
            cmd["channel"] = 1;
    }
    else {
        cmd["channel"] = 1;
    }

    cmd["mode"] = mode_;
    cmd["value"] = value.value();
    return cmd;
}

std::string Rule::get_display_string() const {
    std::string mode_str;
    switch (mode_) {
    case 0: mode_str = "递减"; break;
    case 1: mode_str = "递增"; break;
    case 2: mode_str = "设为"; break;
    case 3: mode_str = "连减"; break;
    case 4: mode_str = "连增"; break;
    default: mode_str = "未知";
    }
    std::string display_pattern = value_pattern_;
    size_t pos = 0;
    while ((pos = display_pattern.find("{}", pos)) != std::string::npos) {
        if (pos + 2 < display_pattern.size() && display_pattern[pos + 2] != '}') {
            pos += 2;
            continue;
        }
        display_pattern.replace(pos, 2, "{   }");
        pos += 7;
    }
    std::string state_str = enabled_ ? "启用" : "停用";
    return name_ + " [#" + std::to_string(index_) + "][" + state_str + "] "
        + "模式:" + mode_str + " 值模式:" + display_pattern;
}

// ============================================
// 私有辅助函数实现（private）
// ============================================

void Rule::parse_pattern() {
    placeholders_.clear();
    placeholder_positions_.clear();
    placeholder_count_ = 0;
    const std::string& pattern = value_pattern_;
    size_t pos = 0;
    while (pos < pattern.size()) {
        size_t open = pattern.find('{', pos);
        if (open == std::string::npos) {
            break;
        }
        // 找到配对的右花括号
        size_t close = pattern.find('}', open + 1);
        if (close == std::string::npos) {
            break;
        }
        std::string inner = pattern.substr(open + 1, close - open - 1);

        Placeholder ph;
        ph.pos = open;
        ph.len = close - open + 1;

        if (inner == "") {
            // 旧式外部参数占位符 {}
            ph.type = PlaceholderType::EXTERNAL;
            placeholder_positions_.push_back(open);
        }
        else if (inner.rfind("id:", 0) == 0) {
            // 模块数值引用 {id:xxx(名称)}，括号内为名称注释（求值忽略）
            ph.type = PlaceholderType::ID_REF;
            std::string id_part = inner.substr(3);
            size_t paren = id_part.find('(');
            if (paren != std::string::npos) {
                id_part = id_part.substr(0, paren);
            }
            ph.id = id_part;
        }
        else if (inner.rfind("rule:", 0) == 0) {
            // 规则结果引用 {rule:xx}
            ph.type = PlaceholderType::RULE_REF;
            ph.rule_index = std::atoi(inner.substr(5).c_str());
        }
        else {
            // 未知花括号内容：不识别为占位符，继续扫描
            pos = close + 1;
            continue;
        }

        placeholders_.push_back(ph);
        ++placeholder_count_;
        pos = close + 1;
    }
}

std::string Rule::evaluate_value_pattern(const std::vector<std::optional<int>>& values) const {
    if (values.size() != placeholder_count_) {
        LOG_MODULE("Rule", "evaluateValuePattern", LOG_ERROR,
            "规则 " << name_ << " 需要 " << placeholder_count_ << " 个参数，实际收到 " << values.size());
        return "";
    }
    std::string result = value_pattern_;
    // 从后往前替换，避免位置偏移
    for (size_t i = placeholder_count_; i > 0; --i) {
        const Placeholder& ph = placeholders_[i - 1];
        if (!values[i - 1].has_value()) {
            return "";
        }
        std::string val_str = std::to_string(values[i - 1].value());
        result.replace(ph.pos, ph.len, val_str);
    }
    return result;
}

int Rule::evaluate_expression(const std::string& expr) const {
    static thread_local QJSEngine engine;
    QJSValue result = engine.evaluate(QString::fromStdString(expr));
    if (result.isError()) {
        LOG_MODULE("Rule", "evaluateExpression", LOG_ERROR,
            "表达式求值失败: " << expr << ", 错误: " << result.toString().toStdString());
        return 0;
    }
    int raw_value = result.toInt();

    // 根据模式对结果进行范围钳位
    if (mode_ == 2) {
        raw_value = std::clamp(raw_value, 0, 200);
        LOG_MODULE("Rule", "evaluateExpression", LOG_DEBUG,
            "设为模式: 原始值 = " << raw_value << "（已钳位到 [0, 200]）");
    }
    else if (mode_ == 3 || mode_ == 4) {
        raw_value = std::clamp(raw_value, 1, 100);
        LOG_MODULE("Rule", "evaluateExpression", LOG_DEBUG,
            "连续模式: 重复次数 = " << raw_value << "（已钳位到 [1, 100]）");
    }
    return raw_value;
}