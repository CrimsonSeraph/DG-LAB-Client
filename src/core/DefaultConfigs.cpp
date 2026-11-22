// Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
// SPDX-License-Identifier: GPL-3.0-only

#include "DefaultConfigs.h"

#include "AppVersion.h"
#include "DebugLog.h"

#include <nlohmann/json.hpp>
#include <string>

// 默认配置工厂实现

// 手排的嵌套 JSON 字面量，保持现状，禁止 clang-format 重排
// clang-format off
nlohmann::json DefaultConfigs::get_default_config(const std::string& config_name) {
    if (config_name == "main") {
        return {
            {"__priority", 0},
            {"app", {
                {"name", "DG-LAB-Client"},
                {"version", APP_VERSION},
                {"debug", false},
                {"log", {
                    {"console_level", 0},
                    {"only_type_info", false},
                    {"ui_log_level", 0},
                    {"file_enabled", false},
                    {"file_level", 0},
                    {"file_max_mb", 5}
                }},
                {"module", {
                    {"path", "./module"},
                    {"scan_load", false}
                }},
                {"update", {
                    {"manifest_url", "https://github.com/CrimsonSeraph/DG-LAB-Client/releases/latest/download/manifest.json"}
                }}
            }},
            {"python", {
                {"path", "python"}
            }},
            {"rule", {
                {"path", "./config/rules"},
                {"key", "rule"}
            }},
            {"wave", {
                {"path", "./config/waves"}
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
                    {"ip","127.0.0.1"},
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
                    {"theme", "classic"},
                    {"font_size", 16},
                    // 托盘图标折叠区提示只弹一次（Windows 11 无法程序化强制显示托盘图标）
                    {"tray_hint_shown", false}
                }},
                {"log", {
                    {"auto", {
                        {"export_level", 0},
                        {"export_only_level", false},
                        {"export_level_above", true},
                        {"export_dir", "./log"},
                        {"export_retain_count", 1},
                        {"export_max_size", 5242880}
                    }},
                    {"manual", {
                        {"export_level", 0},
                        {"export_only_level", false},
                        {"export_level_above", true},
                        {"export_dir", "./log/handle"}
                    }}
                }},
                {"output", {
                    {"default_device", ""}
                }},
                {"update", {
                    // 启动静默检查更新的上次执行时间（Unix 秒），一天内不再重复提示
                    {"last_check", 0}
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
// clang-format on
