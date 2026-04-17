/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <nlohmann/json.hpp>

#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <vector>

// ============================================
// ConfigTemplate - 通用配置模板结构体
// ============================================

template<typename Tag>
struct ConfigTemplate {
    // -------------------- 成员变量 --------------------
    // 基础字段
    std::string name;
    int value;
    bool enabled;

    // 容器字段
    std::vector<std::string> items;
    std::map<std::string, int> settings;

    // 时间字段
    std::chrono::seconds timeout;
    std::chrono::milliseconds interval;

    // 可选字段
    std::optional<std::string> description;
    std::optional<int> max_count;

    // -------------------- 静态方法 --------------------
    /// @brief JSON 序列化
    inline static void to_json(nlohmann::json& j, const ConfigTemplate& config) {
        j = nlohmann::json{
            {"name", config.name},
            {"value", config.value},
            {"enabled", config.enabled},
            {"items", config.items},
            {"settings", config.settings},
            {"timeout", config.timeout.count()},
            {"interval", config.interval.count()}
        };

        if (config.description.has_value()) {
            j["description"] = config.description.value();
        }
        if (config.max_count.has_value()) {
            j["max_count"] = config.max_count.value();
        }
    }

    /// @brief JSON 反序列化
    inline static void from_json(const nlohmann::json& j, ConfigTemplate& config) {
        j.at("name").get_to(config.name);
        j.at("value").get_to(config.value);
        j.at("enabled").get_to(config.enabled);
        j.at("items").get_to(config.items);
        j.at("settings").get_to(config.settings);

        config.timeout = std::chrono::seconds(j.value("timeout", 30));
        config.interval = std::chrono::milliseconds(j.value("interval", 1000));

        if (j.contains("description")) {
            config.description = j["description"].get<std::string>();
        }
        if (j.contains("max_count")) {
            config.max_count = j["max_count"].get<int>();
        }
    }

    /// @brief 配置验证
    inline bool validate() const {
        return !name.empty() && value >= 0;
    }
};

// ============================================
// MainConfig - 主配置结构体
// ============================================

struct MainConfig {
    // -------------------- 成员变量 --------------------
    std::string app_name_;
    std::string app_version_;
    bool debug_mode_ = false;
    int console_level_ = -1;
    bool is_only_type_info_ = false;
    int ui_log_level_ = -1;
    std::string python_path_;
    std::string bridge_path_;

    // -------------------- 静态方法 --------------------
    static void to_json(nlohmann::json& j, const MainConfig& config);
    static void from_json(const nlohmann::json& j, MainConfig& config);
    bool validate() const;
};

// ============================================
// SystemConfig - 系统配置结构体
// ============================================

struct SystemConfig {
    // -------------------- 成员变量 --------------------
    int websocket_port_ = 9999;

    // -------------------- 静态方法 --------------------
    static void to_json(nlohmann::json& j, const SystemConfig& config);
    static void from_json(const nlohmann::json& j, SystemConfig& config);
    bool validate() const;
};

// ============================================
// UserConfig - 用户配置结构体
// ============================================

struct UserConfig {
    // -------------------- 成员变量 --------------------
    std::string theme_ = "light";
    int ui_font_size_ = 16;

    // -------------------- 静态方法 --------------------
    static void to_json(nlohmann::json& j, const UserConfig& config);
    static void from_json(const nlohmann::json& j, UserConfig& config);
    bool validate() const;
};

// 字段映射宏（预留，未使用）
//BEGIN_FIELD_MAP(Struct)
//    FIELD(Struct, <T>, name)
//END_FIELD_MAP()
