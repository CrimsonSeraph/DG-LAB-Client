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

class DeviceController;

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
    Q_PROPERTY(QVariantList channelDeviceOptions READ channel_device_options NOTIFY channelDeviceOptionsChanged)

public:
    explicit RuleGraphBridge(QObject* parent = nullptr);

    /// @brief 初始化：载入当前规则文件对应的规则图
    void initialize();

    /// @brief 注入设备控制器（通道节点的目标设备下拉与「跟随默认设备」解析都取自它）
    /// @param device 设备控制器，可为空
    void attach_device(DeviceController* device);

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
    QVariantList channel_device_options() const;

    Q_INVOKABLE void loadRuleFile(const QString& file_name);
    Q_INVOKABLE bool save();
    Q_INVOKABLE void refreshModuleValues();

    // 插件级规则图
    /// @brief 载入某插件的规则图（文件缺失时按插件数值生成默认图，保存后落盘）
    /// @param plugin_id 插件（模块）标识；空字符串表示回到总规则图
    Q_INVOKABLE void loadPluginGraph(const QString& plugin_id);
    /// @brief 当前图的依赖集合（rules / valueIds / channels / plugins / events），整图
    Q_INVOKABLE QVariantMap graphDependencies() const;
    /// @brief 当前图中某节点上游的依赖集合
    /// @param from_node_id 起点节点 id
    Q_INVOKABLE QVariantMap graphDependencies(int from_node_id) const;
    /// @brief 某插件输出节点被哪些图引用（显示文本，如 "总规则图"、"gsi 规则图"）
    /// @param plugin_id 插件（模块）标识
    Q_INVOKABLE QStringList outputReferrers(const QString& plugin_id) const;

    // 节点编辑
    Q_INVOKABLE int addRuleNode(double x, double y);
    Q_INVOKABLE int addModuleNode(const QString& value_id, double x, double y);
    Q_INVOKABLE int addOperatorNode(const QString& op, double x, double y);
    Q_INVOKABLE int addAdvancedNode(const QString& advanced, double x, double y);
    Q_INVOKABLE int addChannelNode(const QString& channel, double x, double y);
    /// @brief 在当前图（仅总规则图）添加插件输出节点
    /// @param plugin_id 插件（模块）标识
    /// @param x 节点世界坐标 X
    /// @param y 节点世界坐标 Y
    /// @return 节点 id；插件标识为空或当前图是插件规则图时返回 0，同一插件已有节点时返回该节点 id
    Q_INVOKABLE int addPluginOutputNode(const QString& plugin_id, double x, double y);
    Q_INVOKABLE void moveNode(int id, double x, double y);
    Q_INVOKABLE void deleteNode(int id);
    Q_INVOKABLE void selectNode(int id);
    Q_INVOKABLE void renameRuleNode(int id, const QString& name);
    Q_INVOKABLE void setRuleMode(int id, int mode);
    Q_INVOKABLE void setRuleEnabled(int id, bool enabled);
    Q_INVOKABLE void setRuleExpression(int id, const QString& expression);
    /// @brief 通道节点目标设备下拉的当前下标
    /// @param target_device 节点的 targetDevice（空表示跟随默认设备）
    /// @return 在 channelDeviceOptions 中的下标，找不到返回 0（跟随默认设备）
    Q_INVOKABLE int channelDeviceIndex(const QString& target_device) const;
    /// @brief 按 channelDeviceOptions 下标设置通道节点的目标设备（0 = 跟随默认设备）
    /// @param node_id 节点 id
    /// @param index channelDeviceOptions 的下标
    Q_INVOKABLE void setChannelDeviceTargetByIndex(int node_id, int index);

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
    void channelDeviceOptionsChanged();
    void statusMessage(const QString& message);

private:
    void rebuild_module_values();
    void mark_dirty();
    /// @brief 把当前默认设备写进图（通道节点「跟随默认设备」的唯一入边判定用）
    void apply_default_device();
    /// @brief 通道节点的目标设备显示文本
    /// @param target_device 节点的 targetDevice（空表示跟随默认设备）
    QString device_label_for(const QString& target_device) const;
    QVariantMap node_to_map(const GraphNode& node) const;
    QPointF input_port_position(const GraphNode& node, int port) const;
    QPointF output_port_position(const GraphNode& node) const;

    RuleGraph graph_;
    DeviceController* device_ = nullptr; ///< 设备控制器（设备目标列表与默认设备）
    QString current_rule_file_;
    QString current_graph_plugin_; ///< 当前图所属插件（空表示总规则图）
    QVariantList module_values_;
    int selected_node_id_ = 0;
    bool dirty_ = false;
    std::vector<GraphNode> clipboard_;
    int rule_node_counter_ = 1;
};
