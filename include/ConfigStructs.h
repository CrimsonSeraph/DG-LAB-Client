#pragma once

#include "json.hpp"
#include <chrono>
#include <vector>
#include <map>
#include <optional>
#include <string>

// ============================================
// 配置结构体模板（复制此结构创建新配置）
// ============================================
/*
 * 如何添加新配置结构体：
 * 1. 复制此模板，修改结构体名称和字段
 * 2. 实现 to_json 和 from_json 方法
 * 3. 可选：实现 validate 方法
 */

template<typename Tag>
struct ConfigTemplate {
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

    // 必需方法：JSON序列化
    static void to_json(nlohmann::json& j, const ConfigTemplate& config) {
        j = nlohmann::json{
            {"name", config.name},
            {"value", config.value},
            {"enabled", config.enabled},
            {"items", config.items},
            {"settings", config.settings},
            {"timeout", config.timeout.count()},
            {"interval", config.interval.count()}
        };

        // 可选字段处理
        if (config.description.has_value()) {
            j["description"] = config.description.value();
        }
        if (config.max_count.has_value()) {
            j["max_count"] = config.max_count.value();
        }
    }

    // 必需方法：JSON反序列化
    static void from_json(const nlohmann::json& j, ConfigTemplate& config) {
        // 必需字段
        j.at("name").get_to(config.name);
        j.at("value").get_to(config.value);
        j.at("enabled").get_to(config.enabled);
        j.at("items").get_to(config.items);
        j.at("settings").get_to(config.settings);

        // 时间字段（带默认值）
        config.timeout = std::chrono::seconds(j.value("timeout", 30));
        config.interval = std::chrono::milliseconds(j.value("interval", 1000));

        // 可选字段
        if (j.contains("description")) {
            config.description = j["description"].get<std::string>();
        }
        if (j.contains("max_count")) {
            config.max_count = j["max_count"].get<int>();
        }
    }

    // 可选方法：配置验证
    bool validate() const {
        return !name.empty() && value >= 0;
    }
};
