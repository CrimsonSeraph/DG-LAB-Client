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
