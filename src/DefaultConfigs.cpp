#include "DefaultConfigs.h"

#include "DebugLog.h"

#include <nlohmann/json.hpp>
#include <string>

// ============================================
// 默认配置工厂实现
// ============================================

nlohmann::json DefaultConfigs::get_default_config(const std::string& config_name) {
    if (config_name == "main") {
        return {
            {"__priority", 0},
            {"app", {
                {"name", "DG-LAB-Client"},
                {"version", "0.2.1"},
                {"debug", false},
                {"log", {
                    {"console_level", 0},
                    {"only_type_info", false},
                    {"ui_log_level", 0}
                }}
            }},
            {"python", {
                {"path", "python"},
                {"bridge_path", "./python/Bridge.py"}
            }},
            {"rule", {
                {"path", "./config/rules"},
                {"key", "rule"}
            }},
            {"version", "1.0"},
            {"DGLABClient", "DG-LAB-Client"}
        };
    }
    else if (config_name == "system") {
        return {
            {"__priority", 1},
            {"app", {
                {"websocket", {
                    {"port", 9999}
                }}
            }},
            {"version", "1.0"},
            {"DGLABClient", "DG-LAB-Client"}
        };
    }
    else if (config_name == "user") {
        return {
            {"__priority", 2},
            {"app", {
                {"ui", {
                    {"is_light", true},
                    {"font_size", 16}
                }}
            }},
            {"version", "1.0"},
            {"DGLABClient", "DG-LAB-Client"}
        };
    }
    else {
        // 通用默认配置
        return {
            {"__priority", 0},
            {"app", {}},
            {"version", "1.0"},
            {"DGLABClient", "DG-LAB-Client"}
        };
    }
}
