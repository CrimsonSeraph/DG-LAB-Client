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
#include <stdexcept>

// ============================================
// 构造/析构（public）
// ============================================

Rule::Rule(const std::string& name, const std::string& channel, int mode, const std::string& value_pattern)
    : name_(name), channel_(normalize_channel(channel)), mode_(mode), value_pattern_(value_pattern) {
    parse_pattern();
}

// ============================================
// 公共接口实现（public）
// ============================================

std::string Rule::normalize_channel(const std::string& channel) {
    std::string result = channel;
    // 去除首尾空格
    size_t start = result.find_first_not_of(" \t");
    if (start == std::string::npos) return "";
    size_t end = result.find_last_not_of(" \t");
    result = result.substr(start, end - start + 1);
    // 转为大写
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char c) { return std::toupper(c); });
    if (result == "A" || result == "B") {
        return result;
    }
    return "";
}

size_t Rule::get_placeholder_count() const {
    return placeholder_count_;
}

int Rule::compute_value(const std::vector<int>& params) const {
    std::string expr = evaluate_value_pattern(params);
    if (expr.empty()) {
        LOG_MODULE("Rule", "computeValue", LOG_ERROR, "计算表达式为空");
        return 0;
    }

    static thread_local QJSEngine engine;
    QJSValue result = engine.evaluate(QString::fromStdString(expr));
    if (result.isError()) {
        LOG_MODULE("Rule", "computeValue", LOG_ERROR,
            "表达式求值失败: " << expr << ", 错误: " << result.toString().toStdString());
        return 0;
    }
    int raw_value = result.toInt();

    // 根据模式对结果进行范围钳位
    if (mode_ == 2) {
        raw_value = std::clamp(raw_value, 0, 200);
        LOG_MODULE("Rule", "computeValue", LOG_DEBUG,
            "设为模式：原始值 = " << raw_value << "（已钳位到 [0, 200]）");
    }
    else if (mode_ == 3 || mode_ == 4) {
        raw_value = std::clamp(raw_value, 1, 100);
        LOG_MODULE("Rule", "computeValue", LOG_DEBUG,
            "连续模式：重复次数 = " << raw_value << "（已钳位到 [1, 100]）");
    }
    return raw_value;
}

QJsonObject Rule::generate_command(const std::vector<int>& params) const {
    QJsonObject cmd;
    cmd["cmd"] = "send_strength";

    // 通道处理
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
    cmd["value"] = compute_value(params);
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
    std::string channel_str = channel_.empty() ? "无" : channel_;
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
    return name_ + " [" + channel_str + "] " + mode_str + ": " + display_pattern;
}

// ============================================
// 私有辅助函数实现（private）
// ============================================

void Rule::parse_pattern() {
    placeholder_positions_.clear();
    placeholder_count_ = 0;
    std::string::size_type pos = 0;
    while ((pos = value_pattern_.find("{}", pos)) != std::string::npos) {
        placeholder_positions_.push_back(pos);
        ++placeholder_count_;
        pos += 2;
    }
}

std::string Rule::evaluate_value_pattern(const std::vector<int>& values) const {
    if (values.size() != placeholder_count_) {
        LOG_MODULE("Rule", "evaluateValuePattern", LOG_ERROR,
            "规则 " << name_ << " 需要 " << placeholder_count_ << " 个参数，实际收到 " << values.size());
        throw std::invalid_argument("参数数量不匹配");
    }
    std::string result = value_pattern_;
    int offset = 0;
    for (size_t i = 0; i < placeholder_count_; ++i) {
        size_t pos = placeholder_positions_[i] + offset;
        std::string val_str = std::to_string(values[i]);
        result.replace(pos, 2, val_str);
        offset += val_str.length() - 2;
    }
    return result;
}
