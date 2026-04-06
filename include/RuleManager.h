#pragma once

#include "Rule.h"

#include <nlohmann/json.hpp>

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

class ConfigManager;
class AppConfig; // 通过 AppConfig 获取配置

class RuleManager {
public:
    static RuleManager& instance();

    // 初始化规则目录和关键字
    void init();
    // 扫描目录，获取所有可用的规则文件（不含默认的 rules.json）
    std::vector<std::string> get_available_rule_files() const;
    // 加载指定的规则文件（文件名，如 "rules.json" 或 "rule_custom.json"）
    void load_rule_file(const std::string& filename);
    // 创建新的规则文件（不能是 "rules.json"）
    bool create_rule_file(const std::string& filename, const nlohmann::json& rulesContent);
    // 修改规则文件（保存当前内存中的规则到指定文件）
    bool modify_rule_file(const std::string& filename, const nlohmann::json& rulesContent);
    // 删除规则文件（不能删除 "rules.json"）
    bool delete_rule_file(const std::string& filename);
    // 保存当前加载的规则文件
    bool save_current_rule_file();
    // 获取当前加载的规则文件名
    inline std::string get_current_rule_file() const { return current_file_; }
    // 从指定的配置管理器加载规则
    void load_rules(std::shared_ptr<ConfigManager> configManager);
    // 重新加载（如配置变化时调用）
    void reload_rules();
    // 获取所有规则名称
    std::vector<std::string> get_rule_names() const;
    // 获取指定规则的显示字符串（用于 UI）
    std::string get_rule_display_string(const std::string& ruleName) const;
    // 获取所有规则的显示字符串数组
    std::vector<std::string> get_all_rule_display_strings() const;
    // 获取指定规则的通道
    std::string get_rule_channel(const std::string& ruleName) const;
    // 获取指定规则的模式
    int get_rule_mode(const std::string& ruleName) const;
    // 获取指定规则的值计算式
    std::string get_rule_value_pattern(const std::string& ruleName) const;
    // 生成所需指令
    template<typename... Args>
    QJsonObject evaluateCommand(const std::string& ruleName, Args... args);
    nlohmann::json load_json_file(const std::string& filename) const;

private:
    RuleManager() = default;
    std::unordered_map<std::string, Rule> rules_;
    mutable std::mutex mutex_;
    std::shared_ptr<ConfigManager> config_manager_;
    std::string rules_dir_;
    std::string keyword_;
    std::string current_file_;
    std::vector<std::string> available_files_;

    void scan_directory();
    std::string get_full_path(const std::string& filename) const;
    bool save_json_file(const std::string& filename, const nlohmann::json& content) const;
    void parse_config(const nlohmann::json& config);
};

#include "RuleManager_impl.hpp"
