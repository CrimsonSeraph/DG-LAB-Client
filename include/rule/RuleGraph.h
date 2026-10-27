/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QString>
#include <QStringList>

#include <nlohmann/json.hpp>

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

// 规则图模型（可视化规则编辑器 + 规则数据层）
// 节点类型与连线规则见 include/README.md 的「关键协议与约定」。
// 图分两级：
//   - 总规则图（plugin_id 为空）：把一个插件的默认输出当作一个插件输出节点使用；
//   - 插件规则图（plugin_id 为插件/模块标识）：恰好一个默认输出节点作为本图出口，
//     该节点记录「被哪些图引用」，引用文本通过 rule_graph_ref_display 取得。
enum class GraphNodeType {
    Rule,         ///< 规则节点：输入端口数量 = 表达式中的 {} 个数
    Module,       ///< 模块源节点：来自某个已启用模块的数值
    Operator,     ///< 基础运算节点：+ - * /
    Advanced,     ///< 高级节点：abs / square / sqrt / expression
    Channel,      ///< 通道输出节点：A / B
    PluginOutput, ///< 插件输出节点（只属于总规则图）：代表某插件规则图的最终输出，无输入
    DefaultOutput ///< 默认输出节点（只属于插件规则图）：本图唯一出口，单输入
};

QString graph_node_type_to_string(GraphNodeType type);
GraphNodeType graph_node_type_from_string(const QString& text);

// 图标识（引用关系在 JSON 与节点里的存储形式）：
//   总规则图 -> "top"；插件规则图 -> "plugin:<插件标识>"
/// @brief 总规则图的图标识
QString rule_graph_top_ref();
/// @brief 插件规则图的图标识
/// @param plugin_id 插件（模块）标识
QString rule_graph_plugin_ref(const QString& plugin_id);
/// @brief 判断图标识是否为总规则图
/// @param ref 图标识
bool rule_graph_ref_is_top(const QString& ref);
/// @brief 取出图标识中的插件标识（总规则图返回空字符串）
/// @param ref 图标识
QString rule_graph_ref_plugin(const QString& ref);
/// @brief 图标识的显示文本（"总规则图" / "<插件标识> 规则图"），用于引用文本与跳转
/// @param ref 图标识
QString rule_graph_ref_display(const QString& ref);

/// @brief 规则图依赖集合（启用级联：启动通道或图输出时其子规则/事件一并启用）
struct GraphDependency {
    std::vector<std::string> rules;     ///< 上游可达的规则节点名（去重排序）
    std::vector<std::string> value_ids; ///< 上游可达的模块数值 ID（去重排序）
    std::vector<std::string> channels;  ///< 上游可达的通道（去重排序）
    std::vector<std::string> plugins;   ///< 图涉及的插件标识（去重排序）
    std::vector<std::string> events;    ///< 涉及插件声明的事件（"<插件标识>/<事件 ID>"，由 RuleManager 补齐）
};

struct GraphNode {
    int id = 0;
    GraphNodeType type = GraphNodeType::Rule;
    QString name;        ///< 显示名
    QString rule_name;   ///< Rule：规则名
    QString value_id;    ///< Module：数值 ID
    QString label;       ///< Module：数值显示名
    QString op;          ///< Operator：+ - * /
    QString advanced;    ///< Advanced：abs / square / sqrt / expression
    QString expression;  ///< Rule：基础 valuePattern；Advanced：自定义表达式
    QString channel;     ///< Channel：A / B
    int mode = 1;        ///< Rule：模式 0-4
    bool enabled = true; ///< Rule：启用
    double x = 0.0;
    double y = 0.0;
    /// @brief 插件（模块）标识
    /// PluginOutput：该节点代表的插件；DefaultOutput：本图所属插件
    QString plugin_id;
    /// @brief 图标识列表（引用关系，见 rule_graph_*）
    /// PluginOutput：它引用的插件图，恒为一条 "plugin:<plugin_id>"；
    /// DefaultOutput：引用本默认输出的图（"top" 或 "plugin:<插件标识>"），可多条，由
    /// RuleManager::sync_output_references 在保存后重建
    QStringList references;
};

struct GraphEdge {
    int id = 0;
    int from = 0;
    int to = 0;
    int port = 0;
};

class RuleGraph {
public:
    // 访问
    const std::vector<GraphNode>& nodes() const { return nodes_; }
    const std::vector<GraphEdge>& edges() const { return edges_; }

    /// @brief 所属插件标识（空字符串表示总规则图）
    const QString& plugin_id() const { return plugin_id_; }

    /// @brief 设置所属插件标识
    /// @param plugin_id 插件（模块）标识，总规则图为空
    void set_plugin_id(const QString& plugin_id) { plugin_id_ = plugin_id; }

    GraphNode* find_node(int id);
    const GraphNode* find_node(int id) const;

    /// @brief 节点输入端口数量
    int input_count(int id) const;
    /// @brief 输入端口显示名（占位符内容 / A / B / 输入）
    QString input_label(int id, int port) const;

    // 输出节点（数据层不变量：插件图恰好一个默认输出节点，不可手动新增/删除）
    /// @brief 默认输出节点 id
    /// @return 节点 id，图中没有默认输出节点返回 -1
    int default_output_node() const;

    /// @brief 确保插件图恰好一个默认输出节点（缺失则新增，重复则保留首个）
    /// @param plugin_id 插件标识（节点归属，为空时取图自身的插件标识）
    /// @return 默认输出节点 id
    int ensure_default_output(const QString& plugin_id);

    /// @brief 全部插件输出节点 id（总规则图里的插件入口）
    std::vector<int> plugin_output_nodes() const;

    /// @brief 默认输出节点的引用列表（被哪些图引用）
    QStringList output_references() const;

    /// @brief 重设默认输出节点的引用列表
    /// @param references 图标识列表
    void set_output_references(const QStringList& references);

    /// @brief 追加一条引用（已存在时不重复追加）
    /// @param reference 图标识
    void add_output_reference(const QString& reference);

    // 依赖集合（启用级联）
    /// @brief 从某节点向上游回溯的依赖集合
    /// @param from_node_id 起点节点 id；0 表示整图（优先以输出/通道节点为起点）
    /// @return 依赖集合，事件列表由 RuleManager 补齐
    GraphDependency dependencies(int from_node_id = 0) const;

    // 求值（多图求值：插件图默认输出 → 总图的插件输出节点）
    /// @brief 模块数值解析回调
    /// @param value_id 数值 ID
    /// @return 数值，尚未取到（无数据源）时返回空
    using ValueResolver = std::function<std::optional<int>(const QString& value_id)>;

    /// @brief 求值某节点的上游子图
    /// @param node_id 起点节点 id（输出节点或通道节点）；0 表示默认输出节点，总规则图退化为首个通道节点
    /// @param resolve 模块数值解析
    /// @param plugin_outputs 插件输出节点的取值（键为插件标识，由调用方先求值插件图）
    /// @return 求值结果；上游无水值、未连线或不可求值（如插件图里的插件输出节点）时返回空
    /// @note 规则节点按 Rule 的空值语义与模式钳位求值；运算/高级节点沿用 Math.floor 的取整口径
    std::optional<int> evaluate(int node_id, const ValueResolver& resolve,
        const std::map<QString, int>& plugin_outputs = {}) const;

    // 编辑
    /// @brief 新增节点（默认输出节点唯一：已存在时返回既有节点 id）
    int add_node(const GraphNode& node);
    /// @brief 删除节点（默认输出节点不可删除）
    bool remove_node(int id);
    bool move_node(int id, double x, double y);
    /// @brief 连接两个节点（自动替换目标端口上的既有连线；插件输出节点不接受输入）
    int connect_nodes(int from, int to, int port);
    bool disconnect_edge(int edge_id);
    bool disconnect_port(int to, int port);
    bool has_edge(int from, int to, int port) const;
    int edge_at_port(int to, int port) const;
    int source_at_port(int to, int port) const;

    // 导入 / 写回
    /// @brief 从当前 RuleManager 规则集导入（无侧车文件时使用）
    void import_from_rules();
    /// @brief 按插件已登记的数值生成默认插件图（数值源节点 + 默认输出节点）
    /// @param plugin_id 插件（模块）标识
    void import_from_plugin(const QString& plugin_id);
    /// @brief 生成规则 JSON（rules 对象）
    nlohmann::json to_rules_json() const;

    nlohmann::json to_json() const;
    void from_json(const nlohmann::json& json);

    /// @brief 旧版侧车文件路径（保存节点位置与辅助节点，仅用于读入历史文件）
    static QString sidecar_path(const QString& rule_file);
    /// @brief 载入规则图：规则文件的 "graph" 字段 -> 旧版侧车文件 -> 从规则集导入
    bool load(const QString& rule_file);
    /// @brief 保存：写回规则文件的 "graph" 字段 + 写回规则文件并重载，随后重建引用列表
    bool save(const QString& rule_file);

private:
    /// @brief 求值单个节点（递归上游，memo 去重）
    std::optional<int> evaluate_node(int node_id, int depth, const ValueResolver& resolve,
        const std::map<QString, int>& plugin_outputs,
        std::map<int, std::optional<int>>& memo) const;
    /// @brief 输入端口上的取值（未连线或上游无值返回空）
    std::optional<int> input_value(int node_id, int port, int depth, const ValueResolver& resolve,
        const std::map<QString, int>& plugin_outputs,
        std::map<int, std::optional<int>>& memo) const;
    /// @brief 求值纯数值表达式（复用 QJSEngine，与 valuePattern 的口径一致）
    /// @param expression JS 表达式（不含占位符）
    /// @return 结果，求值失败返回空
    static std::optional<int> evaluate_number(const QString& expression);

    QString formula_of(int node_id, int depth) const;
    QString source_formula(int node_id, int port, int depth) const;
    QString placeholder_text(int node_id, int port) const;
    bool would_create_cycle(int from, int to) const;
    int rule_index_of(const QString& rule_name) const;
    /// @brief 默认输出节点唯一性归一（保留首个，丢弃多余的节点与连线）
    void normalize_output_nodes();

    std::vector<GraphNode> nodes_;
    std::vector<GraphEdge> edges_;
    QString plugin_id_;
    int next_node_id_ = 1;
    int next_edge_id_ = 1;
};
