#pragma once

#include "RuleManager.h"

#include <stdexcept>
#include <vector>

template<typename... Args>
QJsonObject RuleManager::evaluate_command(const std::string& rule_name, Args... args) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = rules_.find(rule_name);
    if (it == rules_.end()) {
        throw std::runtime_error("规则不存在: " + rule_name);
    }
    std::vector<int> values = { args... };
    return it->second.generate_command(values);
}
