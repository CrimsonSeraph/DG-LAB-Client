/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "RuleGraphBridge.h"

#include "DebugLog.h"
#include "ModuleManager.h"
#include "RuleManager.h"

#include <QVariantMap>

#include <algorithm>

namespace {

    // 与 style/ComponentStyle.qml 中的节点度量保持一致
    constexpr double kNodeWidth = 170.0;
    constexpr double kHeaderHeight = 28.0;
    constexpr double kPortSpacing = 18.0;
    constexpr double kPortInset = 16.0;

} // namespace

RuleGraphBridge::RuleGraphBridge(QObject* parent)
    : QObject(parent) {
}

void RuleGraphBridge::initialize() {
    auto& rules = RuleManager::instance();
    current_rule_file_ = QString::fromStdString(rules.get_current_rule_file());
    if (current_rule_file_.isEmpty()) {
        const auto files = rules.get_available_rule_files();
        if (!files.empty()) {
            current_rule_file_ = QString::fromStdString(files.front());
            try {
                rules.load_rule_file(files.front());
            }
            catch (const std::exception& e) {
                LOG_MODULE("RuleGraphBridge", "initialize", LOG_ERROR, "加载规则文件失败: " << e.what());
            }
        }
    }
    if (!current_rule_file_.isEmpty()) {
        graph_.load(current_rule_file_);
    }
    rebuild_module_values();
    emit graphChanged();
    emit ruleFilesChanged();
    LOG_MODULE("RuleGraphBridge", "initialize", LOG_INFO, "规则图编辑器桥接已就绪");
}

QVariantMap RuleGraphBridge::node_to_map(const GraphNode& node) const {
    QVariantMap item;
    item.insert(QStringLiteral("id"), node.id);
    item.insert(QStringLiteral("type"), graph_node_type_to_string(node.type));
    item.insert(QStringLiteral("name"), node.name);
    item.insert(QStringLiteral("ruleName"), node.rule_name);
    item.insert(QStringLiteral("valueId"), node.value_id);
    item.insert(QStringLiteral("label"), node.label);
    item.insert(QStringLiteral("op"), node.op);
    item.insert(QStringLiteral("advanced"), node.advanced);
    item.insert(QStringLiteral("expression"), node.expression);
    item.insert(QStringLiteral("channel"), node.channel);
    item.insert(QStringLiteral("mode"), node.mode);
    item.insert(QStringLiteral("enabled"), node.enabled);
    item.insert(QStringLiteral("x"), node.x);
    item.insert(QStringLiteral("y"), node.y);
    item.insert(QStringLiteral("inputCount"), graph_.input_count(node.id));

    QStringList labels;
    const int inputs = graph_.input_count(node.id);
    for (int i = 0; i < inputs; ++i) {
        labels.append(graph_.input_label(node.id, i));
    }
    item.insert(QStringLiteral("inputLabels"), labels);
    item.insert(QStringLiteral("width"), kNodeWidth);
    item.insert(QStringLiteral("headerHeight"), kHeaderHeight);
    item.insert(QStringLiteral("portSpacing"), kPortSpacing);
    item.insert(QStringLiteral("portInset"), kPortInset);
    return item;
}

QVariantList RuleGraphBridge::nodes() const {
    QVariantList list;
    for (const auto& node : graph_.nodes()) {
        list.append(node_to_map(node));
    }
    return list;
}

QPointF RuleGraphBridge::input_port_position(const GraphNode& node, int port) const {
    return QPointF(node.x, node.y + kHeaderHeight + kPortInset + port * kPortSpacing);
}

QPointF RuleGraphBridge::output_port_position(const GraphNode& node) const {
    return QPointF(node.x + kNodeWidth, node.y + kHeaderHeight / 2.0);
}

QVariantList RuleGraphBridge::edges() const {
    QVariantList list;
    for (const auto& edge : graph_.edges()) {
        const GraphNode* from = graph_.find_node(edge.from);
        const GraphNode* to = graph_.find_node(edge.to);
        if (from == nullptr || to == nullptr) {
            continue;
        }
        const QPointF from_point = output_port_position(*from);
        const QPointF to_point = input_port_position(*to, edge.port);
        QVariantMap item;
        item.insert(QStringLiteral("id"), edge.id);
        item.insert(QStringLiteral("from"), edge.from);
        item.insert(QStringLiteral("to"), edge.to);
        item.insert(QStringLiteral("port"), edge.port);
        item.insert(QStringLiteral("fromX"), from_point.x());
        item.insert(QStringLiteral("fromY"), from_point.y());
        item.insert(QStringLiteral("toX"), to_point.x());
        item.insert(QStringLiteral("toY"), to_point.y());
        list.append(item);
    }
    return list;
}

QVariantList RuleGraphBridge::rule_files() const {
    QVariantList list;
    for (const auto& file : RuleManager::instance().get_available_rule_files()) {
        list.append(QString::fromStdString(file));
    }
    return list;
}

void RuleGraphBridge::rebuild_module_values() {
    module_values_.clear();
    auto& modules = ModuleManager::instance();
    for (const auto& module_name : modules.get_module_names()) {
        const Module* module = modules.get_module(module_name);
        if (module == nullptr) {
            continue;
        }
        for (const auto& value : module->get_values()) {
            QVariantMap item;
            item.insert(QStringLiteral("valueId"), QString::fromStdString(value.get_id()));
            item.insert(QStringLiteral("label"), QString::fromStdString(value.get_name()));
            item.insert(QStringLiteral("moduleName"), QString::fromStdString(module_name));
            module_values_.append(item);
        }
    }
    emit moduleValuesChanged();
}

QVariantList RuleGraphBridge::module_values() const {
    return module_values_;
}

void RuleGraphBridge::refreshModuleValues() {
    rebuild_module_values();
}

QVariantMap RuleGraphBridge::selected_node() const {
    const GraphNode* node = graph_.find_node(selected_node_id_);
    return node == nullptr ? QVariantMap() : node_to_map(*node);
}

void RuleGraphBridge::mark_dirty() {
    if (!dirty_) {
        dirty_ = true;
        emit dirtyChanged();
    }
}

void RuleGraphBridge::loadRuleFile(const QString& file_name) {
    if (file_name.isEmpty()) {
        return;
    }
    try {
        RuleManager::instance().load_rule_file(file_name.toStdString());
    }
    catch (const std::exception& e) {
        emit statusMessage(QStringLiteral("加载规则文件失败: ") + QString::fromUtf8(e.what()));
        return;
    }
    current_rule_file_ = file_name;
    graph_.load(file_name);
    selected_node_id_ = 0;
    dirty_ = false;
    emit dirtyChanged();
    emit selectionChanged();
    emit graphChanged();
    emit statusMessage(QStringLiteral("已载入规则文件：") + file_name);
}

bool RuleGraphBridge::save() {
    if (current_rule_file_.isEmpty()) {
        return false;
    }
    const bool ok = graph_.save(current_rule_file_);
    dirty_ = !ok;
    emit dirtyChanged();
    emit graphChanged();
    emit statusMessage(ok ? QStringLiteral("规则图已保存") : QStringLiteral("规则图保存失败"));
    return ok;
}

int RuleGraphBridge::addRuleNode(double x, double y) {
    GraphNode node;
    node.type = GraphNodeType::Rule;
    QString candidate;
    do {
        candidate = QStringLiteral("rule_%1").arg(rule_node_counter_++);
    } while ([this, &candidate]() {
        for (const auto& existing : graph_.nodes()) {
            if (existing.type == GraphNodeType::Rule && existing.rule_name == candidate) {
                return true;
            }
        }
        return false;
    }());
    node.rule_name = candidate;
    node.name = node.rule_name;
    node.expression = QStringLiteral("{}");
    node.mode = 1;
    node.x = x;
    node.y = y;
    const int id = graph_.add_node(node);
    selected_node_id_ = id;
    mark_dirty();
    emit selectionChanged();
    emit graphChanged();
    return id;
}

int RuleGraphBridge::addModuleNode(const QString& value_id, double x, double y) {
    QString label = value_id;
    for (const auto& item : module_values_) {
        const QVariantMap map = item.toMap();
        if (map.value(QStringLiteral("valueId")).toString() == value_id) {
            label = map.value(QStringLiteral("label")).toString();
            break;
        }
    }
    GraphNode node;
    node.type = GraphNodeType::Module;
    node.value_id = value_id;
    node.label = label;
    node.name = label;
    node.x = x;
    node.y = y;
    const int id = graph_.add_node(node);
    selected_node_id_ = id;
    mark_dirty();
    emit selectionChanged();
    emit graphChanged();
    return id;
}

int RuleGraphBridge::addOperatorNode(const QString& op, double x, double y) {
    GraphNode node;
    node.type = GraphNodeType::Operator;
    node.op = op;
    node.name = op;
    node.x = x;
    node.y = y;
    const int id = graph_.add_node(node);
    selected_node_id_ = id;
    mark_dirty();
    emit selectionChanged();
    emit graphChanged();
    return id;
}

int RuleGraphBridge::addAdvancedNode(const QString& advanced, double x, double y) {
    GraphNode node;
    node.type = GraphNodeType::Advanced;
    node.advanced = advanced;
    if (advanced == QStringLiteral("abs")) {
        node.name = QStringLiteral("绝对值");
    }
    else if (advanced == QStringLiteral("square")) {
        node.name = QStringLiteral("平方");
    }
    else if (advanced == QStringLiteral("sqrt")) {
        node.name = QStringLiteral("开根号");
    }
    else {
        node.name = QStringLiteral("自定义表达式");
        node.expression = QStringLiteral("{}");
    }
    node.x = x;
    node.y = y;
    const int id = graph_.add_node(node);
    selected_node_id_ = id;
    mark_dirty();
    emit selectionChanged();
    emit graphChanged();
    return id;
}

int RuleGraphBridge::addChannelNode(const QString& channel, double x, double y) {
    GraphNode node;
    node.type = GraphNodeType::Channel;
    node.channel = (channel == QStringLiteral("B")) ? QStringLiteral("B") : QStringLiteral("A");
    node.name = QStringLiteral("通道 %1").arg(node.channel);
    node.x = x;
    node.y = y;
    const int id = graph_.add_node(node);
    selected_node_id_ = id;
    mark_dirty();
    emit selectionChanged();
    emit graphChanged();
    return id;
}

void RuleGraphBridge::moveNode(int id, double x, double y) {
    if (graph_.move_node(id, x, y)) {
        mark_dirty();
        emit graphChanged();
    }
}

void RuleGraphBridge::deleteNode(int id) {
    if (graph_.remove_node(id)) {
        if (selected_node_id_ == id) {
            selected_node_id_ = 0;
            emit selectionChanged();
        }
        mark_dirty();
        emit graphChanged();
    }
}

void RuleGraphBridge::selectNode(int id) {
    if (selected_node_id_ == id) {
        return;
    }
    selected_node_id_ = id;
    emit selectionChanged();
}

void RuleGraphBridge::renameRuleNode(int id, const QString& name) {
    GraphNode* node = graph_.find_node(id);
    if (node == nullptr || node->type != GraphNodeType::Rule || name.isEmpty()) {
        return;
    }
    for (const auto& existing : graph_.nodes()) {
        if (existing.id != id && existing.type == GraphNodeType::Rule && existing.rule_name == name) {
            emit statusMessage(QStringLiteral("规则名已存在：") + name);
            return;
        }
    }
    node->rule_name = name;
    node->name = name;
    mark_dirty();
    emit selectionChanged();
    emit graphChanged();
}

void RuleGraphBridge::setRuleMode(int id, int mode) {
    GraphNode* node = graph_.find_node(id);
    if (node != nullptr && node->type == GraphNodeType::Rule) {
        node->mode = std::clamp(mode, 0, 4);
        mark_dirty();
        emit selectionChanged();
        emit graphChanged();
    }
}

void RuleGraphBridge::setRuleEnabled(int id, bool enabled) {
    GraphNode* node = graph_.find_node(id);
    if (node != nullptr && node->type == GraphNodeType::Rule) {
        node->enabled = enabled;
        mark_dirty();
        emit selectionChanged();
        emit graphChanged();
    }
}

void RuleGraphBridge::setRuleExpression(int id, const QString& expression) {
    GraphNode* node = graph_.find_node(id);
    if (node == nullptr) {
        return;
    }
    if (node->type == GraphNodeType::Rule || (node->type == GraphNodeType::Advanced
            && node->advanced == QStringLiteral("expression"))) {
        node->expression = expression;
        mark_dirty();
        emit selectionChanged();
        emit graphChanged();
    }
}

void RuleGraphBridge::connectNodes(int from, int to, int port) {
    if (graph_.connect_nodes(from, to, port) > 0) {
        mark_dirty();
        emit graphChanged();
    }
}

void RuleGraphBridge::deleteEdge(int edge_id) {
    if (graph_.disconnect_edge(edge_id)) {
        mark_dirty();
        emit graphChanged();
    }
}

void RuleGraphBridge::copySelection() {
    clipboard_.clear();
    const GraphNode* node = graph_.find_node(selected_node_id_);
    if (node != nullptr) {
        clipboard_.push_back(*node);
        emit statusMessage(QStringLiteral("已复制节点：") + node->name);
    }
}

void RuleGraphBridge::pasteClipboard(double offset_x, double offset_y) {
    for (const auto& source : clipboard_) {
        GraphNode node = source;
        node.x += offset_x;
        node.y += offset_y;
        if (node.type == GraphNodeType::Rule) {
            node.rule_name = QStringLiteral("rule_%1").arg(rule_node_counter_++);
            node.name = node.rule_name;
        }
        const int id = graph_.add_node(node);
        selected_node_id_ = id;
    }
    if (!clipboard_.empty()) {
        mark_dirty();
        emit selectionChanged();
        emit graphChanged();
    }
}
