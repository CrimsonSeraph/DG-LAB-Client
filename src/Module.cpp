/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "Module.h"

// ============================================
// 构造/析构（public）
// ============================================

Module::Module(const std::string& name)
    : name_(name) {
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

void Module::add_value(const ModuleValue& value) {
    values_.push_back(value);
}

void Module::set_all_values_period(QueryPeriod period) {
    for (auto& value : values_) {
        value.set_query_period(period);
    }
}
