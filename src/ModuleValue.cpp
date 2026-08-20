/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "ModuleValue.h"

// ============================================
// 构造/析构（public）
// ============================================

ModuleValue::ModuleValue(const std::string& id, const std::string& name, QueryPeriod period,
    const std::string& field)
    : id_(id)
    , name_(name)
    , query_period_(period)
    , field_(field) {
}
