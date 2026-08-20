/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "Rule.h"

#include <nlohmann/json.hpp>

#include <QJsonObject>
#include <QObject>
#include <QString>

#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class ConfigManager;
class AppConfig;

// ============================================
// RuleManager - 规则管理器（单例）
// 负责规则加载、启用判定、周期触发、规则间引用与级联计算
// ============================================
class RuleManager : public QObject {
    Q_OBJECT

public:
    // -------------------- 单例 --------------------
    /// @brief 获取单例实例
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
    /// @brief 获取所有规则名称（按规则序号排序）
    /// @return 规则名称列表
    std::vector<std::string> get_rule_names() const;

    /// @brief 获取规则的显示字符串（含序号、启用状态、模式与值表达式）
    /// @param rule_name 规则名称
    /// @return 显示字符串，规则不存在时返回空字符串
    std::string get_rule_display_string(const std::string& rule_name) const;

    /// @brief 获取所有规则的显示字符串
    /// @return 显示字符串列表
    std::vector<std::string> get_all_rule_display_strings() const;

    /// @brief 获取规则通道（"A"/"B"/空，兼容旧字段）
    /// @param rule_name 规则名称
    /// @return 通道字符串，规则不存在时返回空字符串
    std::string get_rule_channel(const std::string& rule_name) const;

    /// @brief 获取规则模式（0-4）
    /// @param rule_name 规则名称
    /// @return 模式值，规则不存在时返回 -1
    int get_rule_mode(const std::string& rule_name) const;

    /// @brief 获取规则的值计算表达式
    /// @param rule_name 规则名称
    /// @return 表达式字符串，规则不存在时返回空字符串
    std::string get_rule_value_pattern(const std::string& rule_name) const;

    /// @brief 获取规则序号（1 起始，按加载顺序编号）
    /// @param rule_name 规则名称
    /// @return 规则序号，规则不存在返回 -1
    int get_rule_index(const std::string& rule_name) const;

    /// @brief 按规则序号获取规则名称
    /// @param rule_index 规则序号
    /// @return 规则名称，序号无效返回空字符串
    std::string get_rule_name_by_index(int rule_index) const;

    /// @brief 获取规则启用状态（用户设置值）
    /// @param rule_name 规则名称
    /// @return 启用返回 true，规则不存在返回 false
    bool get_rule_enabled(const std::string& rule_name) const;

    /// @brief 获取规则有效启用状态（考虑父级与通道可用性）
    /// @param rule_name 规则名称
    /// @return 有效启用返回 true
    bool is_rule_effectively_enabled(const std::string& rule_name) const;

    /// @brief 获取规则的通道父级列表
    /// @param rule_name 规则名称
    /// @return 通道父级列表
    std::vector<RuleParent> get_rule_parents(const std::string& rule_name) const;

    /// @brief 获取规则父级显示文本（如 "A"、"rule:1,2"、"A;rule:1,2"、"无"）
    /// @param rule_name 规则名称
    /// @return 父级显示文本
    std::string get_rule_parents_display(const std::string& rule_name) const;

    /// @brief 获取规则最近一次计算结果
    /// @param rule_name 规则名称
    /// @return 最近结果（可选），未计算过返回空
    std::optional<int> get_rule_last_result(const std::string& rule_name) const;

    /// @brief 获取规则模式适用性（用于模式列灰显）
    /// @param rule_name 规则名称
    /// @return 0=父级全部为通道（正常显示）；1=父级无通道（不适用）；2=父级混合（部分不适用）
    int get_rule_mode_applicability(const std::string& rule_name) const;

    // -------------------- 规则修改 --------------------
    /// @brief 设置规则启用状态
    /// @param rule_name 规则名称
    /// @param enabled 是否启用
    void set_rule_enabled(const std::string& rule_name, bool enabled);

    /// @brief 设置规则的通道父级（执行通道唯一性：其余声明同通道的规则父级置空）
    /// @param rule_name 规则名称
    /// @param channel 通道（"A"/"B"/空）
    void set_rule_channel(const std::string& rule_name, const std::string& channel);

    /// @brief 获取引用指定规则的规则序号列表（规则父级，由值模式 {rule:xx} 推导）
    /// @param rule_name 规则名称
    /// @return 引用该规则的规则序号列表
    std::vector<int> get_rule_parent_rules(const std::string& rule_name) const;

    /// @brief 在指定规则的值模式中添加对另一规则的引用（{rule:xx}，追加到末尾）
    /// @param rule_name 要修改值模式的规则名称
    /// @param referenced_index 被引用的规则序号
    /// @return 成功返回 true，引用已存在或规则不存在返回 false
    bool add_rule_reference(const std::string& rule_name, int referenced_index);

    /// @brief 从指定规则的值模式中移除对另一规则的引用（{rule:xx}，含前导运算符清理）
    /// @param rule_name 要修改值模式的规则名称
    /// @param referenced_index 被引用的规则序号
    /// @return 成功返回 true，引用不存在或规则不存在返回 false
    bool remove_rule_reference(const std::string& rule_name, int referenced_index);

    // -------------------- 通道启用状态 --------------------
    /// @brief 设置通道启用状态（通道启用时触发直连规则计算）
    /// @param channel 通道（"A"/"B"）
    /// @param enabled 是否启用
    void set_channel_enabled(const std::string& channel, bool enabled);

    /// @brief 获取通道启用状态
    /// @param channel 通道（"A"/"B"）
    /// @return 启用返回 true
    bool get_channel_enabled(const std::string& channel) const;

    // -------------------- 计算 --------------------
    /// @brief 计算指定规则（检查启用 → 解析占位符 → 求值 → 缓存 → 级联推送）
    /// @param rule_name 规则名称
    /// @return 计算结果（可选），未启用或存在空值时返回空
    std::optional<int> compute_rule(const std::string& rule_name);

    /// @brief 手动计算规则并发送命令（供测试/手动触发使用）
    /// @param rule_name 规则名称
    /// @return 计算结果（可选）
    std::optional<int> trigger_rule(const std::string& rule_name);

    // -------------------- 模板方法（命令生成）--------------------
    /// @brief 根据规则名称和参数生成命令（旧式 {} 占位符传参）
    /// @tparam Args 参数类型（int）
    /// @param rule_name 规则名称
    /// @param args 参数列表（数量需匹配规则占位符）
    /// @return JSON 命令对象
    template<typename... Args>
    QJsonObject evaluate_command(const std::string& rule_name, Args... args);

    // -------------------- 辅助（JSON 文件读写）--------------------
    /// @brief 从文件加载 JSON
    nlohmann::json load_json_file(const std::string& filename) const;

signals:
    /// @brief 规则计算完成且父级为通道时发出（命令发送给 Python 端）
    /// @param cmd 完整的命令 JSON 对象
    void rule_command_ready(const QJsonObject& cmd);

    /// @brief 规则集发生变化时发出（用于刷新界面）
    void rules_changed();

    /// @brief 规则计算完成且父级为通道时发出（用于首页通道规则卡片刷新）
    /// @param rule_name 规则名称
    /// @param channel 通道（"A"/"B"）
    /// @param value 计算结果
    void rule_result_changed(const QString& rule_name, const QString& channel, int value);

private:
    // -------------------- 构造/析构（单例私有）--------------------
    RuleManager();
    ~RuleManager() override;

    // -------------------- 成员变量 --------------------
    std::map<int, std::string> index_to_name_;         ///< 规则序号 → 名称（有序）
    std::unordered_map<std::string, Rule> rules_;      ///< 规则映射
    mutable std::mutex mutex_;                         ///< 保护规则映射
    std::shared_ptr<ConfigManager> config_manager_;    ///< 配置管理器（用于从配置加载）
    std::string rules_dir_;                            ///< 规则目录
    std::string keyword_;                              ///< 规则文件关键字
    std::string current_file_;                         ///< 当前加载的文件名
    std::vector<std::string> available_files_;         ///< 可用规则文件列表

    /// @brief 规则结果事件（规则名、通道、计算结果）
    using ResultEvent = std::tuple<std::string, std::string, int>;

    std::map<int, std::vector<int>> referrers_;        ///< 规则序号 → 引用它的规则序号列表（{rule:xx}）
    std::map<std::string, std::vector<int>> id_users_; ///< 数值 ID → 引用它的规则序号列表（{id:xxx}）
    std::map<int, std::optional<int>> last_results_;   ///< 规则序号 → 最近计算结果
    std::map<std::string, bool> channel_enabled_;      ///< 通道启用状态（A/B）
    int compute_depth_ = 0;                            ///< 级联计算深度（防循环引用）
    static constexpr int MAX_COMPUTE_DEPTH = 16;       ///< 最大级联深度

    // -------------------- 私有辅助函数 --------------------
    void scan_directory();                                                                 ///< 扫描目录获取可用文件
    std::string get_full_path(const std::string& filename) const;                          ///< 获取完整路径
    bool save_json_file(const std::string& filename, const nlohmann::json& content) const; ///< 保存 JSON 文件
    void parse_config(const nlohmann::json& config);                                       ///< 解析规则配置
    void rebuild_indexes();                                                                ///< 重建序号映射与引用索引
    void deduplicate_channel_parents();                                                    ///< 通道父级唯一性去重（加载时）
    void deduplicate_channel_parents_keep(const std::string& keep_name,
        const std::string& channel);                                                       ///< 通道父级唯一性去重（手动设置时保留指定规则）
    bool is_rule_effectively_enabled_locked(const std::string& rule_name,
        std::vector<std::string>& visiting) const;                                         ///< 有效启用判定（需已持有锁）
    std::optional<int> compute_rule_locked(const std::string& rule_name,
        std::vector<QJsonObject>& pending_commands,
        std::vector<ResultEvent>& pending_results);                                        ///< 计算规则（需已持有锁，收集待发送命令与结果事件）
    std::optional<int> resolve_placeholder_locked(const Placeholder& placeholder,
        std::vector<QJsonObject>& pending_commands,
        std::vector<ResultEvent>& pending_results);                                        ///< 解析单个占位符（需已持有锁）

private slots:
    /// @brief 模块数值变化时触发值模式中引用该数值的规则计算
    /// @param module_name 模块名称
    /// @param value_id 数值 ID
    /// @param new_value 最新数值
    void on_module_value_changed(const QString& module_name, const QString& value_id,
        int new_value);
};

#include "RuleManager_impl.hpp"