/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QString>
#include <QStringList>

#include <nlohmann/json.hpp>

#include <vector>

// ============================================
// 规则图模型（可视化规则编辑器）
// ============================================
// 节点类型：
//   Rule          规则节点：输入端数量 = valuePattern 中 {} 的个数，带内容的 {} 显示名称
//   Module        模块源节点：来自某个已启用模块的数值
//   Operator      基础运算节点：+ - * /（结果向下取整）
//   Advanced      高级节点：abs / square / sqrt / expression（自定义表达式）
//   Channel       通道输出节点：A / B（单输入；同一通道只允许一个来源）
//
// 连线方向：源（输出） -> 消费端（输入端口）。规则节点被其它节点引用时写为 {rule:序号}，
// 模块源节点写为 {id:值ID(名称)}，运算符/高级节点生成 JS 子表达式（valuePattern 由
// QJSEngine 求值，天然支持 Math.*）。
enum class GraphNodeType {
    Rule,
    Module,
    Operator,
    Advanced,
    Channel
};

QString graph_node_type_to_string(GraphNodeType type);
GraphNodeType graph_node_type_from_string(const QString& text);

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
};

struct GraphEdge {
    int id = 0;
    int from = 0;
    int to = 0;
    int port = 0;
};

class RuleGraph {
public:
    // -------------------- 访问 --------------------
    const std::vector<GraphNode>& nodes() const { return nodes_; }
    const std::vector<GraphEdge>& edges() const { return edges_; }

    GraphNode* find_node(int id);
    const GraphNode* find_node(int id) const;

    /// @brief 节点输入端口数量
    int input_count(int id) const;
    /// @brief 输入端口显示名（占位符内容 / A / B / 输入）
    QString input_label(int id, int port) const;

    // -------------------- 编辑 --------------------
    int add_node(const GraphNode& node);
    bool remove_node(int id);
    bool move_node(int id, double x, double y);
    /// @brief 连接两个节点（自动替换目标端口上的既有连线）
    int connect_nodes(int from, int to, int port);
    bool disconnect_edge(int edge_id);
    bool disconnect_port(int to, int port);
    bool has_edge(int from, int to, int port) const;
    int edge_at_port(int to, int port) const;
    int source_at_port(int to, int port) const;

    // -------------------- 导入 / 写回 --------------------
    /// @brief 从当前 RuleManager 规则集导入（无侧车文件时使用）
    void import_from_rules();
    /// @brief 生成规则 JSON（rules 对象）
    nlohmann::json to_rules_json() const;

    nlohmann::json to_json() const;
    void from_json(const nlohmann::json& json);

    /// @brief 侧车文件路径（保存节点位置与辅助节点）
    static QString sidecar_path(const QString& rule_file);
    /// @brief 载入规则图：优先侧车文件，否则从规则集导入
    bool load(const QString& rule_file);
    /// @brief 保存：写侧车文件 + 写回规则文件并重载
    bool save(const QString& rule_file);

private:
    QString formula_of(int node_id, int depth) const;
    QString source_formula(int node_id, int port, int depth) const;
    QString placeholder_text(int node_id, int port) const;
    bool would_create_cycle(int from, int to) const;
    int rule_index_of(const QString& rule_name) const;

    std::vector<GraphNode> nodes_;
    std::vector<GraphEdge> edges_;
    int next_node_id_ = 1;
    int next_edge_id_ = 1;
};
