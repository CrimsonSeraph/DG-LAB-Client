#include "DefaultConfigs.h"
#include "DebugLog.h"

nlohmann::json DefaultConfigs::get_default_config(const std::string& config_name) {
    if (config_name == "main") {
        return {
            {"__priority", 0},
            {"app", {
                {"name", "DG-LAB-Client"},
                {"version", "1.0.0"},
                {"debug", true},
                {"log", {
                    {"level", 0},
                    {"only_type_info", false}
                }}
            }},
            {"python", {
                {"path", "python"}
            }},
            {"version", "1.0"},
            {"DGLABClient", "DG-LAB-Client"}
        };
    }
    else if (config_name == "system") {
        return {
            {"__priority", 1},
            {"app", {}},
            {"version", "1.0"},
            {"DGLABClient", "DG-LAB-Client"}
        };
    }
    else if (config_name == "user") {
        return {
            {"__priority", 2},
            {"app", {}},
            {"version", "1.0"},
            {"DGLABClient", "DG-LAB-Client"}
        };
    }
    else {
        // 通用默认配置
        return {
            {"__priority", "0"},
            {"app", {}},
            {"version", "1.0"},
            {"DGLABClient", "DG-LAB-Client"}
        };
    }
}
