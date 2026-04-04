#include "RuleManager.h"
#include "AppConfig.h"
#include "ConfigManager.h"
#include "DebugLog.h"

#include <fstream>
#include <filesystem>

RuleManager& RuleManager::instance() {
    static RuleManager instance;
    return instance;
}

namespace fs = std::filesystem;

void RuleManager::init() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& config = AppConfig::instance();
    std::string rule_path = config.get_value<std::string>("rule.path", "./config/rules");
    std::string rule_key = config.get_value<std::string>("rule.key", "rule");
    rules_dir_ = rule_path;
    keyword_ = rule_key;
    // 确保目录存在
    try {
        fs::create_directories(rules_dir_);
    }
    catch (const std::exception& e) {
        LOG_MODULE("RuleManager", "init", LOG_ERROR, "创建目录失败: " << e.what());
    }
    scan_directory();
}

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
        if (json.contains("rules")) {
            parse_config(json["rules"]);
            current_file_ = filename;
            LOG_MODULE("RuleManager", "load_rule_file", LOG_INFO, "已加载规则文件: " << filename);
        }
    }
    catch (const std::exception& e) {
        LOG_MODULE("RuleManager", "load_rule_file", LOG_ERROR, "加载失败: " << e.what());
        throw;
    }
}

bool RuleManager::create_rule_file(const std::string& filename, const nlohmann::json& rulesContent) {
    if (filename == "rules.json") {
        LOG_MODULE("RuleManager", "create_rule_file", LOG_WARN, "不能创建默认规则文件 rules.json");
        return false;
    }
    std::string fullPath = get_full_path(filename);
    if (fs::exists(fullPath)) {
        LOG_MODULE("RuleManager", "create_rule_file", LOG_WARN, "文件已存在: " << filename);
        return false;
    }
    nlohmann::json j;
    j["rules"] = rulesContent;
    if (save_json_file(filename, j)) {
        scan_directory();
        return true;
    }
    return false;
}

bool RuleManager::modify_rule_file(const std::string& filename, const nlohmann::json& rulesContent) {
    if (filename.empty()) return false;
    std::string fullPath = get_full_path(filename);
    if (!fs::exists(fullPath)) {
        LOG_MODULE("RuleManager", "modify_rule_file", LOG_WARN, "文件不存在: " << filename);
        return false;
    }
    nlohmann::json j;
    // 保留原有其他字段，只替换 rules
    try {
        j = load_json_file(filename);
    }
    catch (...) {}
    j["rules"] = rulesContent;
    if (save_json_file(filename, j)) {
        // 如果当前加载的就是这个文件，则重新解析规则
        if (current_file_ == filename) {
            parse_config(rulesContent);
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
    std::string fullPath = get_full_path(filename);
    if (!fs::exists(fullPath)) return false;
    if (fs::remove(fullPath)) {
        // 如果当前加载的文件被删除，则重新加载默认文件
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
    nlohmann::json rulesJson;
    for (const auto& [name, rule] : rules_) {
        rulesJson[name] = rule.getPattern();
    }
    return modify_rule_file(current_file_, rulesJson);
}

void RuleManager::load_rules(std::shared_ptr<ConfigManager> configManager) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!configManager) {
        LOG_MODULE("RuleManager", "load_rules", LOG_ERROR, "ConfigManager 为空");
        return;
    }
    config_manager_ = configManager;
    reload_rules();
}

void RuleManager::reload_rules() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!config_manager_) {
        LOG_MODULE("RuleManager", "reload_rules", LOG_WARN, "ConfigManager 未初始化");
        return;
    }
    auto rulesJson = config_manager_->get<nlohmann::json>("rules");
    if (!rulesJson.has_value()) {
        LOG_MODULE("RuleManager", "reload_rules", LOG_WARN, "配置中没有 'rules' 字段");
        return;
    }

    parse_config(rulesJson.value());
}

std::vector<std::string> RuleManager::get_rule_names() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> names;
    names.reserve(rules_.size());
    for (const auto& [name, _] : rules_) {
        names.push_back(name);
    }
    return names;
}

std::string RuleManager::get_rule_display_string(const std::string& ruleName) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = rules_.find(ruleName);
    if (it == rules_.end()) {
        return "";
    }
    return it->second.get_display_string();
}

std::string RuleManager::get_rule_pattern(const std::string& ruleName) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = rules_.find(ruleName);
    return it != rules_.end() ? it->second.getPattern() : "";
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

void RuleManager::scan_directory() {
    available_files_.clear();
    if (!fs::exists(rules_dir_)) return;
    for (const auto& entry : fs::directory_iterator(rules_dir_)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            std::string filename = entry.path().filename().string();
            if (filename == "rules.json") continue; // 默认文件单独处理
            if (filename.find(keyword_) != std::string::npos) {
                available_files_.push_back(filename);
            }
        }
    }
}

std::string RuleManager::get_full_path(const std::string& filename) const {
    return (rules_dir_ + "/" + filename);
}

bool RuleManager::save_json_file(const std::string& filename, const nlohmann::json& content) const {
    std::string fullPath = get_full_path(filename);
    std::ofstream file(fullPath);
    if (!file.is_open()) return false;
    file << content.dump(4);
    return true;
}

void RuleManager::parse_config(const nlohmann::json& config) {
    rules_.clear();
    for (auto& [key, value] : config.items()) {
        if (value.is_string()) {
            std::string pattern = value.get<std::string>();
            rules_.emplace(key, Rule(key, pattern));
            LOG_MODULE("RuleManager", "parse_config", LOG_DEBUG,
                "加载规则: " << key << " = " << pattern);
        }
        else {
            LOG_MODULE("RuleManager", "parse_config", LOG_WARN,
                "规则 " << key << " 的值不是字符串，已忽略");
        }
    }
}
