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

// ============================================
// RuleGraphBridge - 规则可视化编辑器桥接（QML 上下文属性 ruleGraph）
// ============================================
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

    Q_INVOKABLE void loadRuleFile(const QString& file_name);
    Q_INVOKABLE bool save();
    Q_INVOKABLE void refreshModuleValues();

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
    void statusMessage(const QString& message);

private:
    void rebuild_module_values();
    void mark_dirty();
    QVariantMap node_to_map(const GraphNode& node) const;
    QPointF input_port_position(const GraphNode& node, int port) const;
    QPointF output_port_position(const GraphNode& node) const;

    RuleGraph graph_;
    QString current_rule_file_;
    QVariantList module_values_;
    int selected_node_id_ = 0;
    bool dirty_ = false;
    std::vector<GraphNode> clipboard_;
    int rule_node_counter_ = 1;
};
