/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "RuleManager.h"

#include <stdexcept>
#include <vector>

// ============================================
// 模板方法实现
// ============================================

template<typename... Args>
inline QJsonObject RuleManager::evaluate_command(const std::string& rule_name, Args... args) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = rules_.find(rule_name);
    if (it == rules_.end()) {
        throw std::runtime_error("规则不存在: " + rule_name);
    }
    std::vector<int> values = { args... };
    return it->second.generate_command(values);
}
