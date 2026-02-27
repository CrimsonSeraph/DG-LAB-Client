#include "ConfigStructs.h"
#include "DebugLog.h"

#include <iostream>

// ============================================
// XXXConfig 方法实现
// ============================================

//void XXXConfig::to_json(nlohmann::json& j, const XXXConfig& config) {
//    j = nlohmann::json{
//        {"name", config.name},
//    };
//}

//void XXXConfig::from_json(const nlohmann::json& j, XXXConfig& config) {
//    j.at("name").get_to(config.name);
//}

//bool XXXConfig::validate() const {
//    if (name.empty()) {
//        LOG_MODULE("ConfigStructs", "validate", LOG_WARN, "name 不能为空！");
//        return false;
//    }
//    return true;
//}

void MainConfig::to_json(nlohmann::json& j, const MainConfig& config) {
    j = nlohmann::json{
        {"app.name", config.app_name_},
        {"app.version", config.app_version_},
        {"app.debug", config.debug_mode_},
        {"app.log.level", config.log_level_},
        {"app.log.only_type_info", config.is_only_type_info_},
        {"python.path", config.python_path_},
    };
}

void MainConfig::from_json(const nlohmann::json& j, MainConfig& config) {
    // 必需字段
    j.at("app.name").get_to(config.app_name_);
    j.at("app.version").get_to(config.app_version_);
    j.at("app.debug").get_to(config.debug_mode_);
    j.at("app.log.level").get_to(config.log_level_);
    j.at("app.log.only_type_info").get_to(config.is_only_type_info_);
    j.at("python.path").get_to(config.python_path_);
}

bool MainConfig::validate() const {
    if (app_name_.empty()) {
        return false;
    }
    else if (app_version_.empty()) {
        return false;
    }
    else if (python_path_.empty()) {
        return false;
    }
    return true;
}
