/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "ModuleManager.h"

#include "DebugLog.h"

#include <algorithm>
#include <tuple>
#include <utility>

// ============================================
// 单例（public）
// ============================================

ModuleManager& ModuleManager::instance() {
    static ModuleManager manager;
    return manager;
}

// ============================================
// 构造/析构（private）
// ============================================

ModuleManager::ModuleManager()
    : QObject(nullptr) {
    // 定时器由主线程驱动，调度查询基于最短周期
    timer_ = new QTimer(this);
    timer_->setTimerType(Qt::PreciseTimer);
    connect(timer_, &QTimer::timeout, this, &ModuleManager::on_timer_tick);
}

ModuleManager::~ModuleManager() {
    if (timer_) {
        timer_->stop();
    }
}

// ============================================
// 初始化（public）
// ============================================

void ModuleManager::init() {
    std::lock_guard<std::mutex> lock(mutex_);
    // 幂等处理：避免重复注册
    if (!modules_.empty()) {
        return;
    }
    register_default_modules();
    // 未设置自定义数据源时使用默认模拟数据源
    if (!data_source_) {
        data_source_ = &ModuleManager::default_data_source;
    }
    // 以最短查询周期为基准启动调度器
    rebuild_scheduler();
    LOG_MODULE("ModuleManager", "init", LOG_INFO,
        "数值模块初始化完成，基准周期: " << base_period_ms_ << "ms");
}

// ============================================
// 模块查询（public）
// ============================================

std::vector<std::string> ModuleManager::get_module_names() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> names;
    names.reserve(modules_.size());
    for (const auto& module : modules_) {
        names.push_back(module.get_name());
    }
    return names;
}

const Module* ModuleManager::get_module(const std::string& module_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& module : modules_) {
        if (module.get_name() == module_name) {
            return &module;
        }
    }
    return nullptr;
}

const ModuleValue* ModuleManager::get_value(const std::string& module_name,
    const std::string& value_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& module : modules_) {
        if (module.get_name() != module_name) {
            continue;
        }
        for (const auto& value : module.get_values()) {
            if (value.get_id() == value_id) {
                return &value;
            }
        }
    }
    return nullptr;
}

int ModuleManager::get_module_min_period_ms(const std::string& module_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& module : modules_) {
        if (module.get_name() == module_name) {
            return module.get_min_period_ms();
        }
    }
    return query_period_to_ms(QueryPeriod::SECOND);
}

std::string ModuleManager::find_module_by_value_id(const std::string& value_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& module : modules_) {
        for (const auto& value : module.get_values()) {
            if (value.get_id() == value_id) {
                return module.get_name();
            }
        }
    }
    return "";
}

// ============================================
// 周期设置（public）
// ============================================

void ModuleManager::set_value_period(const std::string& module_name, const std::string& value_id,
    QueryPeriod period) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        bool found = false;
        for (auto& module : modules_) {
            if (module.get_name() != module_name) {
                continue;
            }
            for (auto& value : module.get_values()) {
                if (value.get_id() == value_id) {
                    value.set_query_period(period);
                    found = true;
                    break;
                }
            }
            break;
        }
        if (!found) {
            LOG_MODULE("ModuleManager", "set_value_period", LOG_WARN,
                "未找到数值: " << module_name << "/" << value_id);
            return;
        }
        rebuild_scheduler();
    }
    emit period_changed();
}

void ModuleManager::set_module_period(const std::string& module_name, QueryPeriod period) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        bool found = false;
        for (auto& module : modules_) {
            if (module.get_name() == module_name) {
                module.set_all_values_period(period);
                found = true;
                break;
            }
        }
        if (!found) {
            LOG_MODULE("ModuleManager", "set_module_period", LOG_WARN,
                "未找到模块: " << module_name);
            return;
        }
        rebuild_scheduler();
    }
    emit period_changed();
}

void ModuleManager::set_all_period(QueryPeriod period) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& module : modules_) {
            module.set_all_values_period(period);
        }
        rebuild_scheduler();
    }
    emit period_changed();
    LOG_MODULE("ModuleManager", "set_all_period", LOG_INFO,
        "已统一设置所有数值查询周期: " << query_period_to_text(period));
}

// ============================================
// 数据源（public）
// ============================================

void ModuleManager::set_data_source(DataSource source) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_source_ = source ? std::move(source) : &ModuleManager::default_data_source;
}

// ============================================
// 查询（public）
// ============================================

int ModuleManager::query_value(const std::string& module_name, const std::string& value_id) {
    int new_value = 0;
    bool changed = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& module : modules_) {
            if (module.get_name() != module_name) {
                continue;
            }
            for (auto& value : module.get_values()) {
                if (value.get_id() == value_id) {
                    new_value = data_source_(value_id);
                    // 数值变化检测：已有历史值且与最新值不同才推送
                    changed = value.get_has_value() && value.get_last_value() != new_value;
                    value.set_last_value(new_value);
                    break;
                }
            }
            break;
        }
    }
    if (changed) {
        emit value_changed(QString::fromStdString(module_name),
            QString::fromStdString(value_id), new_value);
    }
    return new_value;
}

int ModuleManager::get_base_period_ms() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return base_period_ms_;
}

// ============================================
// private slots 实现
// ============================================

void ModuleManager::on_timer_tick() {
    ++tick_count_;
    // 收集本周期内发生变化的数值（先查后发，避免持锁发信号）
    std::vector<std::tuple<std::string, std::string, int>> changes;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& module : modules_) {
            for (auto& value : module.get_values()) {
                int period_ms = query_period_to_ms(value.get_query_period());
                int interval = period_ms / base_period_ms_;
                if (interval < 1) {
                    interval = 1;
                }
                // 按基准周期取模：周期越短的数值被查询的次数越多
                if (tick_count_ % interval != 0) {
                    continue;
                }
                if (query_value_locked(module, value)) {
                    changes.emplace_back(module.get_name(), value.get_id(),
                        value.get_last_value());
                }
            }
        }
    }
    // 数值变化时推送（触发规则计算与界面刷新）
    for (const auto& [module_name, value_id, new_value] : changes) {
        emit value_changed(QString::fromStdString(module_name),
            QString::fromStdString(value_id), new_value);
    }
}

// ============================================
// 私有辅助函数实现（private）
// ============================================

void ModuleManager::rebuild_scheduler() {
    base_period_ms_ = query_period_to_ms(QueryPeriod::SECOND);
    for (const auto& module : modules_) {
        base_period_ms_ = std::min(base_period_ms_, module.get_min_period_ms());
    }
    tick_count_ = 0;
    if (timer_) {
        timer_->start(base_period_ms_);
    }
    LOG_MODULE("ModuleManager", "rebuild_scheduler", LOG_DEBUG,
        "调度器已重建，基准周期: " << base_period_ms_ << "ms");
}

bool ModuleManager::query_value_locked(Module& module, ModuleValue& value) {
    int new_value = data_source_(value.get_id());
    // 数值变化检测：已有历史值且与最新值不同才返回 true（触发推送）
    bool changed = value.get_has_value() && value.get_last_value() != new_value;
    value.set_last_value(new_value);
    return changed;
}

void ModuleManager::register_default_modules() {
    Module cs2_module("CS2 GSI 模块");
    // 参照 CS2 官方 GSI 规范注册常用数值（id 用于规则引用，field 为底层字段名）
    cs2_module.add_value(ModuleValue("health", "当前血量", QueryPeriod::QUARTER_SECOND, "m_iHealth"));
    cs2_module.add_value(ModuleValue("armor", "当前护甲", QueryPeriod::HALF_SECOND, "m_ArmorValue"));
    cs2_module.add_value(ModuleValue("team_num", "队伍编号", QueryPeriod::SECOND, "m_iTeamNum"));
    cs2_module.add_value(ModuleValue("money", "金钱", QueryPeriod::TWO_SECONDS, "m_iMoney"));
    cs2_module.add_value(ModuleValue("has_helmet", "是否有头盔", QueryPeriod::FOUR_SECONDS, "m_bHasHelmet"));
    cs2_module.add_value(ModuleValue("has_defuser", "是否有拆弹器", QueryPeriod::FOUR_SECONDS, "m_bHasDefuser"));
    modules_.push_back(cs2_module);
    LOG_MODULE("ModuleManager", "register_default_modules", LOG_DEBUG,
        "已注册默认模块: " << cs2_module.get_name() << "，数值数量: " << cs2_module.get_values().size());
}

int ModuleManager::default_data_source(const std::string& value_id) {
    // 模拟数据源：真实 GSI 接入后通过 set_data_source 替换
    // 静态局部变量可直接在 lambda 内引用（无需捕获，规避 MSVC C3495）
    static unsigned int seed = 20260214u;
    auto random_range = [](int lo, int hi) {
        seed = seed * 1103515245u + 12345u;
        return lo + static_cast<int>((seed >> 16) % static_cast<unsigned int>(hi - lo + 1));
    };
    static int health = 100;
    static int armor = 50;
    static int money = 8000;
    static int helmet = 1;
    static int defuser = 0;
    if (value_id == "health") {
        health = std::clamp(health + random_range(-1, 1), 0, 100);
        return health;
    }
    if (value_id == "armor") {
        armor = std::clamp(armor + random_range(-2, 2), 0, 100);
        return armor;
    }
    if (value_id == "team_num") {
        return random_range(2, 3);
    }
    if (value_id == "money") {
        money = std::clamp(money + random_range(-50, 50), 0, 16000);
        return money;
    }
    if (value_id == "has_helmet") {
        helmet = random_range(0, 1);
        return helmet;
    }
    if (value_id == "has_defuser") {
        defuser = random_range(0, 1);
        return defuser;
    }
    return 0;
}