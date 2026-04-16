/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "RuleManager.h"

#include "AppConfig.h"
#include "ConfigManager.h"
#include "DebugLog.h"

#include <filesystem>
#include <fstream>
#include <mutex>

namespace fs = std::filesystem;

// ============================================
// 单例（public）
// ============================================

RuleManager& RuleManager::instance() {
    static RuleManager instance;
    return instance;
}

// ============================================
// 初始化（public）
// ============================================

void RuleManager::init() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& config = AppConfig::instance();
    std::string rule_path = config.get_value<std::string>("rule.path", "./config/rules");
    std::string rule_key = config.get_value<std::string>("rule.key", "rule");
    rules_dir_ = rule_path;
    keyword_ = rule_key;

    try {
        fs::create_directories(rules_dir_);
    }
    catch (const std::exception& e) {
        LOG_MODULE("RuleManager", "init", LOG_ERROR, "创建目录失败: " << e.what());
    }
    scan_directory();
}

// ============================================
// 文件管理（public）
// ============================================

std::vector<std::string> RuleManager::get_available_rule_files() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return available_files_;
}

void RuleManager::load_rule_file(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        auto json = load_json_file(filename);
        if (!json.contains("rules")) {
            LOG_MODULE("RuleManager", "load_rule_file", LOG_WARN, "文件缺少 'rules' 字段: " << filename);
            return;
        }
        parse_config(json["rules"]);
        current_file_ = filename;
        LOG_MODULE("RuleManager", "load_rule_file", LOG_INFO, "已加载规则文件: " << filename);
    }
    catch (const std::exception& e) {
        LOG_MODULE("RuleManager", "load_rule_file", LOG_ERROR, "加载失败: " << e.what());
        throw;
    }
}

bool RuleManager::create_rule_file(const std::string& filename, const nlohmann::json& rules_content) {
    if (filename == "rules.json") {
        LOG_MODULE("RuleManager", "create_rule_file", LOG_WARN, "不能创建默认规则文件 rules.json");
        return false;
    }
    std::string full_path = get_full_path(filename);
    if (fs::exists(full_path)) {
        LOG_MODULE("RuleManager", "create_rule_file", LOG_WARN, "文件已存在: " << filename);
        return false;
    }
    nlohmann::json j;
    j["rules"] = rules_content;
    if (save_json_file(filename, j)) {
        scan_directory();
        return true;
    }
    return false;
}

bool RuleManager::modify_rule_file(const std::string& filename, const nlohmann::json& rules_content) {
    if (filename.empty()) return false;
    std::string full_path = get_full_path(filename);
    if (!fs::exists(full_path)) {
        LOG_MODULE("RuleManager", "modify_rule_file", LOG_WARN, "文件不存在: " << filename);
        return false;
    }
    nlohmann::json j;
    try {
        j = load_json_file(filename);
    }
    catch (...) {}
    j["rules"] = rules_content;
    if (save_json_file(filename, j)) {
        if (current_file_ == filename) {
            parse_config(rules_content);
        }
        return true;
    }
    return false;
}

bool RuleManager::delete_rule_file(const std::string& filename) {
    if (filename == "rules.json") {
        LOG_MODULE("RuleManager", "delete_rule_file", LOG_WARN, "不能删除默认规则文件 rules.json");
        return false;
    }
    std::string full_path = get_full_path(filename);
    if (!fs::exists(full_path)) return false;
    if (fs::remove(full_path)) {
        if (current_file_ == filename) {
            try {
                load_rule_file("rules.json");
            }
            catch (...) {}
        }
        scan_directory();
        return true;
    }
    return false;
}

bool RuleManager::save_current_rule_file() {
    if (current_file_.empty()) {
        LOG_MODULE("RuleManager", "save_current_rule_file", LOG_WARN, "没有当前加载的规则文件");
        return false;
    }
    nlohmann::json rules_json;
    for (const auto& [name, rule] : rules_) {
        rules_json[name] = {
            {"channel", rule.get_channel()},
            {"mode", rule.get_mode()},
            {"valuePattern", rule.get_value_pattern()}
        };
    }
    return modify_rule_file(current_file_, rules_json);
}

// ============================================
// 规则加载（从配置管理器）（public）
// ============================================

void RuleManager::load_rules(std::shared_ptr<ConfigManager> config_manager) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!config_manager) {
        LOG_MODULE("RuleManager", "load_rules", LOG_ERROR, "ConfigManager 为空");
        return;
    }
    config_manager_ = config_manager;
    reload_rules();
}

void RuleManager::reload_rules() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!config_manager_) {
        LOG_MODULE("RuleManager", "reload_rules", LOG_WARN, "ConfigManager 未初始化");
        return;
    }
    auto rules_json = config_manager_->get<nlohmann::json>("rules");
    if (!rules_json.has_value()) {
        LOG_MODULE("RuleManager", "reload_rules", LOG_WARN, "配置中没有 'rules' 字段");
        return;
    }
    parse_config(rules_json.value());
}

// ============================================
// 规则查询（public）
// ============================================

std::vector<std::string> RuleManager::get_rule_names() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> names;
    names.reserve(rules_.size());
    for (const auto& [name, _] : rules_) {
        names.push_back(name);
    }
    return names;
}

std::string RuleManager::get_rule_display_string(const std::string& rule_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = rules_.find(rule_name);
    return it != rules_.end() ? it->second.get_display_string() : "";
}

std::vector<std::string> RuleManager::get_all_rule_display_strings() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    result.reserve(rules_.size());
    for (const auto& [name, rule] : rules_) {
        result.push_back(rule.get_display_string());
    }
    return result;
}

std::string RuleManager::get_rule_channel(const std::string& rule_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = rules_.find(rule_name);
    return it != rules_.end() ? it->second.get_channel() : "";
}

int RuleManager::get_rule_mode(const std::string& rule_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = rules_.find(rule_name);
    return it != rules_.end() ? it->second.get_mode() : -1;
}

std::string RuleManager::get_rule_value_pattern(const std::string& rule_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = rules_.find(rule_name);
    return it != rules_.end() ? it->second.get_value_pattern() : "";
}

// ============================================
// 辅助（JSON 文件读写）（public）
// ============================================

nlohmann::json RuleManager::load_json_file(const std::string& filename) const {
    std::string full_path = get_full_path(filename);
    std::ifstream file(full_path);
    if (!file.is_open()) {
        throw std::runtime_error("无法打开文件: " + full_path);
    }
    nlohmann::json j;
    file >> j;
    return j;
}

// ============================================
// 私有辅助函数实现（private）
// ============================================

void RuleManager::scan_directory() {
    available_files_.clear();
    if (!fs::exists(rules_dir_)) return;
    for (const auto& entry : fs::directory_iterator(rules_dir_)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            std::string filename = entry.path().filename().string();
            if (filename == "rules.json") continue;
            if (filename.find(keyword_) != std::string::npos) {
                available_files_.push_back(filename);
            }
        }
    }
}

std::string RuleManager::get_full_path(const std::string& filename) const {
    return rules_dir_ + "/" + filename;
}

bool RuleManager::save_json_file(const std::string& filename, const nlohmann::json& content) const {
    std::string full_path = get_full_path(filename);
    std::ofstream file(full_path);
    if (!file.is_open()) return false;
    file << content.dump(4);
    return true;
}

void RuleManager::parse_config(const nlohmann::json& config) {
    rules_.clear();
    for (auto& [key, value] : config.items()) {
        try {
            std::string channel = "";
            int mode = 0;
            std::string value_pattern;

            if (value.is_string()) {
                channel = "";
                mode = 1;
                value_pattern = value.get<std::string>();
            }
            else if (value.is_object()) {
                channel = value.value("channel", "");
                mode = value.value("mode", 1);
                value_pattern = value.value("valuePattern", "");
                if (value_pattern.empty()) {
                    LOG_MODULE("RuleManager", "parse_config", LOG_WARN,
                        "规则 " << key << " 缺少 valuePattern，已跳过");
                    continue;
                }
            }
            else {
                LOG_MODULE("RuleManager", "parse_config", LOG_WARN,
                    "规则 " << key << " 格式无效，已跳过");
                continue;
            }

            rules_.emplace(key, Rule(key, channel, mode, value_pattern));
            LOG_MODULE("RuleManager", "parse_config", LOG_DEBUG,
                "加载规则: " << key << " [ch=" << channel << ", mode=" << mode
                << ", pattern=" << value_pattern << "]");
        }
        catch (const std::exception& e) {
            LOG_MODULE("RuleManager", "parse_config", LOG_ERROR,
                "解析规则 " << key << " 失败: " << e.what());
        }
    }
}
