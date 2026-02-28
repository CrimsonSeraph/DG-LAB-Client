#pragma once

#include <nlohmann/json.hpp>
#include <chrono>
#include <vector>
#include <map>
#include <optional>
#include <string>

// ============================================
// 配置结构体模板
// ============================================

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

    // JSON序列化
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

        // 可选字段处理
        if (config.description.has_value()) {
            j["description"] = config.description.value();
        }
        if (config.max_count.has_value()) {
            j["max_count"] = config.max_count.value();
        }
    }

    // JSON反序列化
    inline static void from_json(const nlohmann::json& j, ConfigTemplate& config) {
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

    // 配置验证
    inline bool validate() const {
        return !name.empty() && value >= 0;
    }
};

struct MainConfig {
    std::string app_name_;
    std::string app_version_;
    bool debug_mode_ = false;
    int console_level_ = -1;
    bool is_only_type_info_ = false;
    int ui_log_level_ = -1;
    std::string python_path_;
    std::string packages_path_;

    // JSON序列化
    static void to_json(nlohmann::json& j, const MainConfig& config);

    // JSON反序列化
    static void from_json(const nlohmann::json& j, MainConfig& config);

    // 配置验证
    bool validate() const;
};

//BEGIN_FIELD_MAP(Struct)
//    FIELD(Struct, <T>, name)
//END_FIELD_MAP()
