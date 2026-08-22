/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "Module.h"

#include <algorithm>
#include <cctype>

// ============================================
// 构造/析构（public）
// ============================================

Module::Module(const std::string& name, const std::vector<std::string>& channels)
    : name_(name)
    , channels_(channels) {
}

// ============================================
// 公共接口实现（public）
// ============================================

int Module::get_min_period_ms() const {
    if (values_.empty()) {
        return query_period_to_ms(QueryPeriod::SECOND);
    }
    int min_period = query_period_to_ms(values_.front().get_query_period());
    for (const auto& value : values_) {
        // 取所有数值中最短的查询周期
        min_period = std::min(min_period, query_period_to_ms(value.get_query_period()));
    }
    return min_period;
}

namespace {
// 本地通道规范化（"A"/"a" -> "A"，无效返回空）
std::string normalize_channel_local(const std::string& channel) {
    std::string result = channel;
    size_t start = result.find_first_not_of(" \t");
    if (start == std::string::npos) return "";
    size_t end = result.find_last_not_of(" \t");
    result = result.substr(start, end - start + 1);
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
    if (result == "A" || result == "B") return result;
    return "";
}
} // namespace

bool Module::is_mounted_on_channel(const std::string& channel) const {
    std::string norm = normalize_channel_local(channel);
    for (const auto& ch : channels_) {
        if (ch == norm) {
            return true;
        }
    }
    return false;
}

void Module::mount_channel(const std::string& channel) {
    std::string norm = normalize_channel_local(channel);
    if (norm.empty()) {
        return;
    }
    // 避免重复挂载
    for (const auto& ch : channels_) {
        if (ch == norm) {
            return;
        }
    }
    channels_.push_back(norm);
}

void Module::unmount_channel(const std::string& channel) {
    std::string norm = normalize_channel_local(channel);
    channels_.erase(std::remove(channels_.begin(), channels_.end(), norm), channels_.end());
}

void Module::add_value(const ModuleValue& value) {
    values_.push_back(value);
}

void Module::set_all_values_period(QueryPeriod period) {
    for (auto& value : values_) {
        value.set_query_period(period);
    }
}
