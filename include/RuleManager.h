#pragma once

#include "Rule.h"

#include <nlohmann/json.hpp>

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

class ConfigManager;
class AppConfig;

// ============================================
// RuleManager - 规则管理器（单例）
// ============================================
class RuleManager {
public:
    // -------------------- 单例 --------------------
    static RuleManager& instance();

    // -------------------- 初始化 --------------------
    /// @brief 初始化规则目录和关键字（从 AppConfig 读取）
    void init();

    // -------------------- 文件管理 --------------------
    /// @brief 获取所有可用的规则文件（不含默认的 rules.json）
    std::vector<std::string> get_available_rule_files() const;

    /// @brief 加载指定的规则文件
    /// @param filename 文件名（如 "rules.json" 或 "rule_custom.json"）
    void load_rule_file(const std::string& filename);

    /// @brief 创建新的规则文件（不能是 "rules.json"）
    bool create_rule_file(const std::string& filename, const nlohmann::json& rules_content);

    /// @brief 修改规则文件（保存规则内容到指定文件）
    bool modify_rule_file(const std::string& filename, const nlohmann::json& rules_content);

    /// @brief 删除规则文件（不能删除 "rules.json"）
    bool delete_rule_file(const std::string& filename);

    /// @brief 保存当前加载的规则到当前文件
    bool save_current_rule_file();

    /// @brief 获取当前加载的规则文件名
    inline std::string get_current_rule_file() const { return current_file_; }

    // -------------------- 规则加载（从配置管理器）--------------------
    /// @brief 从指定的配置管理器加载规则
    void load_rules(std::shared_ptr<ConfigManager> config_manager);

    /// @brief 重新加载规则（从当前配置管理器）
    void reload_rules();

    // -------------------- 规则查询 --------------------
    std::vector<std::string> get_rule_names() const;
    std::string get_rule_display_string(const std::string& rule_name) const;
    std::vector<std::string> get_all_rule_display_strings() const;
    std::string get_rule_channel(const std::string& rule_name) const;
    int get_rule_mode(const std::string& rule_name) const;
    std::string get_rule_value_pattern(const std::string& rule_name) const;

    // -------------------- 模板方法（命令生成）--------------------
    /// @brief 根据规则名称和参数生成命令
    /// @tparam Args 参数类型（int）
    /// @param rule_name 规则名称
    /// @param args 参数列表（数量需匹配规则占位符）
    /// @return JSON 命令对象
    template<typename... Args>
    QJsonObject evaluate_command(const std::string& rule_name, Args... args);

    // -------------------- 辅助（JSON 文件读写）--------------------
    /// @brief 从文件加载 JSON
    nlohmann::json load_json_file(const std::string& filename) const;

private:
    RuleManager() = default;

    // -------------------- 成员变量 --------------------
    std::unordered_map<std::string, Rule> rules_;      ///< 规则映射
    mutable std::mutex mutex_;                         ///< 保护规则映射
    std::shared_ptr<ConfigManager> config_manager_;    ///< 配置管理器（用于从配置加载）
    std::string rules_dir_;                            ///< 规则目录
    std::string keyword_;                              ///< 规则文件关键字
    std::string current_file_;                         ///< 当前加载的文件名
    std::vector<std::string> available_files_;         ///< 可用规则文件列表

    // -------------------- 私有辅助函数 --------------------
    void scan_directory();                             ///< 扫描目录获取可用文件
    std::string get_full_path(const std::string& filename) const;  ///< 获取完整路径
    bool save_json_file(const std::string& filename, const nlohmann::json& content) const;  ///< 保存 JSON 文件
    void parse_config(const nlohmann::json& config);   ///< 解析规则配置
};

#include "RuleManager_impl.hpp"
