#pragma once

#include "RuleManager.h"

#include <stdexcept>
#include <vector>

template<typename... Args>
QJsonObject RuleManager::evaluateCommand(const std::string& ruleName, Args... args) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = rules_.find(ruleName);
    if (it == rules_.end()) {
        throw std::runtime_error("规则不存在: " + ruleName);
    }
    std::vector<int> values = { args... };
    return it->second.generateCommand(values);
}
