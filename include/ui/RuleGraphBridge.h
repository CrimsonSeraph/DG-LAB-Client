/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "RuleGraph.h"

#include <QObject>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include <vector>

// RuleGraphBridge - 规则可视化编辑器桥接（QML 上下文属性 ruleGraph）
// 视图交互（拖动 / 连线 / 右键 / 快捷键）在 QML，图的增删改、复制粘贴、写回
// 全部在本类完成；QML 只调用 Q_INVOKABLE 并绑定属性。
class RuleGraphBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList nodes READ nodes NOTIFY graphChanged)
    Q_PROPERTY(QVariantList edges READ edges NOTIFY graphChanged)
    Q_PROPERTY(QVariantList ruleFiles READ rule_files NOTIFY ruleFilesChanged)
    Q_PROPERTY(QString currentRuleFile READ current_rule_file NOTIFY graphChanged)
    Q_PROPERTY(QVariantList moduleValues READ module_values NOTIFY moduleValuesChanged)
    Q_PROPERTY(int selectedNodeId READ selected_node_id NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap selectedNode READ selected_node NOTIFY selectionChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY dirtyChanged)
    Q_PROPERTY(QVariantList pluginGraphs READ plugin_graphs NOTIFY pluginGraphsChanged)
    Q_PROPERTY(QString currentGraphPlugin READ current_graph_plugin NOTIFY graphChanged)
    Q_PROPERTY(QVariantList outputReferences READ output_references NOTIFY graphChanged)

public:
    explicit RuleGraphBridge(QObject* parent = nullptr);

    /// @brief 初始化：载入当前规则文件对应的规则图
    void initialize();

    QVariantList nodes() const;
    QVariantList edges() const;
    QVariantList rule_files() const;
    QString current_rule_file() const { return current_rule_file_; }
    QVariantList module_values() const;
    int selected_node_id() const { return selected_node_id_; }
    QVariantMap selected_node() const;
    bool dirty() const { return dirty_; }
    QVariantList plugin_graphs() const;
    QString current_graph_plugin() const { return current_graph_plugin_; }
    QVariantList output_references() const;

    Q_INVOKABLE void loadRuleFile(const QString& file_name);
    Q_INVOKABLE bool save();
    Q_INVOKABLE void refreshModuleValues();

    // 插件级规则图（数据与求值侧；编辑器界面在后续批次接入）
    /// @brief 载入某插件的规则图（文件缺失时按插件数值生成默认图，保存后落盘）
    /// @param plugin_id 插件（模块）标识
    Q_INVOKABLE void loadPluginGraph(const QString& plugin_id);
    /// @brief 当前图的依赖集合（rules / valueIds / channels / plugins / events），整图
    Q_INVOKABLE QVariantMap graphDependencies() const;
    /// @brief 当前图中某节点上游的依赖集合
    /// @param from_node_id 起点节点 id
    Q_INVOKABLE QVariantMap graphDependencies(int from_node_id) const;
    /// @brief 某插件输出节点被哪些图引用（显示文本，如 "总规则图"、"buckshot 规则图"）
    /// @param plugin_id 插件（模块）标识
    Q_INVOKABLE QStringList outputReferrers(const QString& plugin_id) const;

    // 节点编辑
    Q_INVOKABLE int addRuleNode(double x, double y);
    Q_INVOKABLE int addModuleNode(const QString& value_id, double x, double y);
    Q_INVOKABLE int addOperatorNode(const QString& op, double x, double y);
    Q_INVOKABLE int addAdvancedNode(const QString& advanced, double x, double y);
    Q_INVOKABLE int addChannelNode(const QString& channel, double x, double y);
    Q_INVOKABLE void moveNode(int id, double x, double y);
    Q_INVOKABLE void deleteNode(int id);
    Q_INVOKABLE void selectNode(int id);
    Q_INVOKABLE void renameRuleNode(int id, const QString& name);
    Q_INVOKABLE void setRuleMode(int id, int mode);
    Q_INVOKABLE void setRuleEnabled(int id, bool enabled);
    Q_INVOKABLE void setRuleExpression(int id, const QString& expression);

    // 连线
    Q_INVOKABLE void connectNodes(int from, int to, int port);
    Q_INVOKABLE void deleteEdge(int edge_id);

    // 剪贴板
    Q_INVOKABLE void copySelection();
    Q_INVOKABLE void pasteClipboard(double offset_x, double offset_y);

signals:
    void graphChanged();
    void ruleFilesChanged();
    void moduleValuesChanged();
    void selectionChanged();
    void dirtyChanged();
    void pluginGraphsChanged();
    void statusMessage(const QString& message);

private:
    void rebuild_module_values();
    void mark_dirty();
    QVariantMap node_to_map(const GraphNode& node) const;
    QPointF input_port_position(const GraphNode& node, int port) const;
    QPointF output_port_position(const GraphNode& node) const;

    RuleGraph graph_;
    QString current_rule_file_;
    QString current_graph_plugin_; ///< 当前图所属插件（空表示总规则图）
    QVariantList module_values_;
    int selected_node_id_ = 0;
    bool dirty_ = false;
    std::vector<GraphNode> clipboard_;
    int rule_node_counter_ = 1;
};
