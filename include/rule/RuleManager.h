/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "Rule.h"
#include "RuleGraph.h"

#include <nlohmann/json.hpp>

#include <QJsonObject>
#include <QObject>
#include <QString>

#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

class ConfigManager;
class AppConfig;

// RuleManager - 规则管理器（单例）
// 负责规则加载、启用判定、周期触发、规则间引用与级联计算
class RuleManager : public QObject {
    Q_OBJECT

public:
    // 单例
    /// @brief 获取单例实例
    static RuleManager& instance();

    // 初始化
    /// @brief 初始化规则目录和关键字（从 AppConfig 读取）
    void init();

    // 文件管理
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

    // 规则图（总规则图 + 插件级规则图）
    // 总规则图存放在规则文件的 "graph" 字段（默认 rules.json）；
    // 每个插件的规则图独立落盘为 config/rules/plugin_<插件标识>.json，内容为
    // { version, plugin, kind: "pluginRuleGraph", graph, rules }。
    /// @brief 插件级规则图文件名
    /// @param plugin_id 插件（模块）标识
    /// @return 文件名（如 "plugin_buckshot.json"）
    std::string get_plugin_graph_file(const std::string& plugin_id) const;

    /// @brief 列出已有插件级规则图的插件标识
    /// @return 插件标识列表（按标识排序）
    std::vector<std::string> get_plugin_rule_graphs() const;

    /// @brief 判断某插件是否已有规则图文件
    /// @param plugin_id 插件（模块）标识
    /// @return 规则图文件存在返回 true
    bool has_plugin_rule_graph(const std::string& plugin_id) const;

    /// @brief 读取插件级规则图
    /// @param plugin_id 插件（模块）标识
    /// @return 规则图；文件缺失或损坏时按插件已登记的数值生成默认图（不落盘）
    RuleGraph get_plugin_rule_graph(const std::string& plugin_id) const;

    /// @brief 保存插件级规则图（写 plugin_<插件标识>.json 的 graph 与 rules）
    /// @param plugin_id 插件（模块）标识
    /// @param graph 规则图
    /// @return 保存成功返回 true；成功后重建默认输出节点的引用列表
    bool save_plugin_rule_graph(const std::string& plugin_id, const RuleGraph& graph);

    /// @brief 读取规则文件内的规则图 JSON（"graph" 字段）
    /// @param filename 规则文件名
    /// @return 规则图 JSON；文件缺失、无 "graph" 字段或字段非对象时返回空
    std::optional<nlohmann::json> read_rule_graph_json(const std::string& filename) const;

    /// @brief 写入规则文件内的规则图 JSON（"graph" 字段，保留其它字段）
    /// @param filename 规则文件名
    /// @param graph_json 规则图 JSON
    /// @return 写入成功返回 true
    bool write_rule_graph_json(const std::string& filename, const nlohmann::json& graph_json);

    /// @brief 重建全部插件图默认输出节点的引用列表（总图与插件图里的插件输出节点 → 被引用方）
    /// @return 全部写入成功返回 true
    bool sync_output_references();

    /// @brief 查询某插件输出节点被哪些图引用
    /// @param plugin_id 插件（模块）标识
    /// @return 图标识列表（"top" 为总规则图，"plugin:<插件标识>" 为插件规则图；显示文本见
    /// rule_graph_ref_display）
    std::vector<std::string> get_output_referrers(const std::string& plugin_id) const;

    /// @brief 查询规则图的依赖集合（启用级联：子规则、数值、通道、插件与事件）
    /// @param graph 规则图
    /// @param from_node_id 起点节点 id；0 表示整图
    /// @return 依赖集合；总规则图会并入其插件输出节点对应插件图的依赖（只展开一层）；
    /// 事件取依赖插件声明的事件（"<插件标识>/<事件 ID>"）
    GraphDependency get_graph_dependencies(const RuleGraph& graph, int from_node_id = 0) const;

    /// @brief 把依赖插件声明的事件补进依赖集合（"<插件标识>/<事件 ID>"，去重排序）
    /// @param plugin_events 插件标识 → 已声明的事件 ID 列表
    /// @param dependency 待补齐的依赖集合（读取其 plugins 字段）
    static void fill_dependency_events(
        const std::map<std::string, std::vector<std::string>>& plugin_events,
        GraphDependency& dependency);

    // 多图求值（插件图默认输出 → 总图的插件输出节点 → 通道节点）
    /// @brief 求值全部插件图与总规则图（模块数值变化、通道或图输出启用时调用）
    /// @note 顺序为先插件图、后总规则图；通道节点的命令在解锁后经 rule_command_ready 发出
    void evaluate_graph_outputs();

    /// @brief 插件图默认输出的最近一次求值结果
    /// @param plugin_id 插件（模块）标识
    /// @return 求值结果，无图、无值或尚未求值返回空
    std::optional<int> get_plugin_graph_output(const std::string& plugin_id) const;

    /// @brief 总规则图通道节点的最近一次求值结果
    /// @param channel 通道（"A"/"B"）
    /// @return 求值结果，无通道节点或尚未求值返回空
    std::optional<int> get_graph_channel_output(const std::string& channel) const;

    // 启用级联（启动通道或图输出时连带启用依赖集合中的子规则与事件）
    /// @brief 启用/停用某规则图的输出
    /// @param graph_ref 图标识（"top" 总规则图，"plugin:<插件标识>" 插件规则图）
    /// @param enabled 是否启用
    /// @return 图存在且状态已更新返回 true
    bool set_graph_output_enabled(const std::string& graph_ref, bool enabled);

    /// @brief 查询某规则图的输出是否已启用
    /// @param graph_ref 图标识
    /// @return 已启用返回 true
    bool is_graph_output_enabled(const std::string& graph_ref) const;

    /// @brief 当前级联视为启用的规则名（去重排序）
    std::vector<std::string> get_cascade_enabled_rules() const;

    /// @brief 当前级联视为启用的事件（"<插件标识>/<事件 ID>"，去重排序）
    std::vector<std::string> get_cascade_enabled_events() const;

    // 规则加载（从配置管理器）
    /// @brief 从指定的配置管理器加载规则
    void load_rules(std::shared_ptr<ConfigManager> config_manager);

    /// @brief 重新加载规则（从当前配置管理器）
    void reload_rules();

    // 规则查询
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

    // 规则修改
    /// @brief 设置规则启用状态
    /// @param rule_name 规则名称
    /// @param enabled 是否启用
    void set_rule_enabled(const std::string& rule_name, bool enabled);

    /// @brief 设置规则的通道父级（执行通道唯一性：其余声明同通道的规则父级置空）
    /// @param rule_name 规则名称
    /// @param channel 通道（"A"/"B"/空）
    void set_rule_channel(const std::string& rule_name, const std::string& channel);

    /// @brief 设置规则的值模式表达式（重建规则对象并刷新引用索引）
    /// @param rule_name 规则名称
    /// @param pattern 新的值模式表达式
    void set_rule_value_pattern(const std::string& rule_name, const std::string& pattern);

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

    // 通道启用状态
    /// @brief 设置通道启用状态（通道启用时触发直连规则计算）
    /// @param channel 通道（"A"/"B"）
    /// @param enabled 是否启用
    void set_channel_enabled(const std::string& channel, bool enabled);

    /// @brief 获取通道启用状态
    /// @param channel 通道（"A"/"B"）
    /// @return 启用返回 true
    bool get_channel_enabled(const std::string& channel) const;

    // 计算
    /// @brief 计算指定规则（检查启用 → 解析占位符 → 求值 → 缓存 → 级联推送）
    /// @param rule_name 规则名称
    /// @return 计算结果（可选），未启用或存在空值时返回空
    std::optional<int> compute_rule(const std::string& rule_name);

    /// @brief 手动计算规则并发送命令（供测试/手动触发使用）
    /// @param rule_name 规则名称
    /// @return 计算结果（可选）
    std::optional<int> trigger_rule(const std::string& rule_name);

    // 模板方法（命令生成）
    /// @brief 根据规则名称和参数生成命令（旧式 {} 占位符传参）
    /// @tparam Args 参数类型（int）
    /// @param rule_name 规则名称
    /// @param args 参数列表（数量需匹配规则占位符）
    /// @return JSON 命令对象
    template<typename... Args>
    QJsonObject evaluate_command(const std::string& rule_name, Args... args);

    // 辅助（JSON 文件读写）
    /// @brief 从文件加载 JSON
    nlohmann::json load_json_file(const std::string& filename) const;

signals:
    /// @brief 规则计算完成且父级为通道时发出（携带待下发的命令 JSON）
    /// @param cmd 完整的命令 JSON 对象
    void rule_command_ready(const QJsonObject& cmd);

    /// @brief 规则集发生变化时发出（用于刷新界面）
    void rules_changed();

    /// @brief 启用级联集合发生变化时发出（事件运行时据此重建运行期启用集合）
    void cascade_changed();

    /// @brief 规则计算完成且父级为通道时发出（用于首页通道规则卡片刷新）
    /// @param rule_name 规则名称
    /// @param channel 通道（"A"/"B"）
    /// @param value 计算结果
    void rule_result_changed(const QString& rule_name, const QString& channel, int value);

private:
    // 构造/析构（单例私有）
    RuleManager();
    ~RuleManager() override;

    // 成员变量
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

    std::map<std::string, GraphDependency> active_cascades_; ///< 级联来源（"channel:A" / "graph:<图标识>"）→ 依赖集合
    std::set<std::string> cascade_rules_;                    ///< 级联视为启用的规则名（由 active_cascades_ 派生）
    std::set<std::string> cascade_events_;                   ///< 级联视为启用的事件（同上，写作 <插件标识>/<事件 ID>）
    std::map<std::string, int> plugin_graph_outputs_;        ///< 插件标识 → 插件图默认输出最近求值结果
    std::map<std::string, int> channel_graph_outputs_;       ///< 通道 → 通道节点最近求值结果

    // 私有辅助函数
    std::vector<std::string> plugin_graph_plugins_locked() const;                           ///< 扫描插件图文件（需已持有锁）
    nlohmann::json read_rule_graph_json_locked(const std::string& filename) const;          ///< 读取规则图 JSON（需已持有锁）
    RuleGraph load_plugin_rule_graph_locked(const std::string& plugin_id) const;            ///< 载入插件规则图（需已持有锁）
    RuleGraph load_top_rule_graph_locked() const;                                           ///< 载入总规则图（需已持有锁）
    RuleGraph load_rule_graph_by_ref_locked(const std::string& graph_ref) const;            ///< 按图标识载入规则图（需已持有锁）
    GraphDependency get_graph_dependencies_locked(const RuleGraph& graph,
        int from_node_id) const;                                                            ///< 依赖集合（需已持有锁）
    void apply_cascade_locked(const std::string& key, const RuleGraph& graph, int from_node_id,
        bool enabled, bool& changed);                                                       ///< 更新单个级联来源（需已持有锁）
    void rebuild_cascade_locked();                                                          ///< 由级联来源重算派生集合（需已持有锁）
    bool refresh_cascades_locked();                                                         ///< 图变化后按新图重算已启用来源（需已持有锁）
    void evaluate_graphs_locked(std::vector<QJsonObject>& pending_commands,
        std::vector<ResultEvent>& pending_results);                                          ///< 多图求值（需已持有锁）
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