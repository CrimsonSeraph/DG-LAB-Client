/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "RuleGraphBridge.h"

#include "DebugLog.h"
#include "DeviceController.h"
#include "ModuleManager.h"
#include "RuleManager.h"

#include <QMap>
#include <QVariantMap>

#include <algorithm>

namespace {

    // 与 style/ComponentStyle.qml 中的节点度量保持一致
    constexpr double kNodeWidth = 170.0;
    constexpr double kHeaderHeight = 28.0;
    constexpr double kPortSpacing = 18.0;
    constexpr double kPortInset = 16.0;

    /// @brief 插件显示名（未加载时退回插件标识）
    /// @param plugin_id 插件（模块）标识
    QString plugin_display_name(const QString& plugin_id) {
        for (const auto& info : ModuleManager::instance().get_plugins()) {
            if (QString::fromStdString(info.module_name) != plugin_id) {
                continue;
            }
            const QString display_name = QString::fromStdString(info.display_name);
            return display_name.isEmpty() ? plugin_id : display_name;
        }
        return plugin_id;
    }

    /// @brief 字符串列表转换（依赖集合输出）
    QStringList to_qstring_list(const std::vector<std::string>& values) {
        QStringList list;
        for (const auto& value : values) {
            list.append(QString::fromStdString(value));
        }
        return list;
    }

} // namespace

RuleGraphBridge::RuleGraphBridge(QObject* parent)
    : QObject(parent) {
}

void RuleGraphBridge::attach_device(DeviceController* device) {
    device_ = device;
    if (device_ != nullptr) {
        // 设备上下线 / 默认设备切换后刷新目标设备下拉（离线项带「离线」）
        connect(device_, &DeviceController::outputDevicesChanged, this,
            &RuleGraphBridge::channelDeviceOptionsChanged);
        connect(device_, &DeviceController::defaultDeviceChanged, this,
            &RuleGraphBridge::channelDeviceOptionsChanged);
    }
    apply_default_device();
    emit channelDeviceOptionsChanged();
    emit graphChanged();
}

void RuleGraphBridge::apply_default_device() {
    graph_.set_default_device(device_ == nullptr ? QString() : device_->default_device_address());
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
    apply_default_device();
    rebuild_module_values();
    emit graphChanged();
    emit ruleFilesChanged();
    emit pluginGraphsChanged();
    LOG_MODULE("RuleGraphBridge", "initialize", LOG_INFO, "规则图编辑器桥接已就绪");
}

QVariantList RuleGraphBridge::nodes() const {
    QVariantList list;
    for (const auto& node : graph_.nodes()) {
        list.append(node_to_map(node));
    }
    return list;
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

QVariantList RuleGraphBridge::module_values() const {
    return module_values_;
}

QVariantList RuleGraphBridge::plugin_graphs() const {
    auto& modules = ModuleManager::instance();
    auto& rules = RuleManager::instance();

    QStringList plugins;
    QMap<QString, QString> labels;
    for (const auto& info : modules.get_plugins()) {
        if (info.module_name.empty()) {
            continue;
        }
        const QString plugin_id = QString::fromStdString(info.module_name);
        if (!labels.contains(plugin_id)) {
            labels.insert(plugin_id, QString::fromStdString(info.display_name));
            plugins.append(plugin_id);
        }
    }
    // 已有图文件但对应插件未加载时也要列出
    for (const auto& plugin_id : rules.get_plugin_rule_graphs()) {
        const QString id = QString::fromStdString(plugin_id);
        if (!plugins.contains(id)) {
            plugins.append(id);
            labels.insert(id, id);
        }
    }
    plugins.sort();

    QVariantList list;
    for (const auto& plugin_id : plugins) {
        QVariantMap item;
        item.insert(QStringLiteral("plugin"), plugin_id);
        item.insert(QStringLiteral("label"), labels.value(plugin_id, plugin_id));
        item.insert(QStringLiteral("hasGraph"),
            rules.has_plugin_rule_graph(plugin_id.toStdString()));
        list.append(item);
    }
    return list;
}

QVariantList RuleGraphBridge::output_references() const {
    QVariantList list;
    for (const auto& reference : graph_.output_references()) {
        QVariantMap item;
        item.insert(QStringLiteral("graph"), reference);
        item.insert(QStringLiteral("label"), rule_graph_ref_display(reference));
        item.insert(QStringLiteral("plugin"), rule_graph_ref_plugin(reference));
        list.append(item);
    }
    return list;
}

QVariantList RuleGraphBridge::channel_device_options() const {
    QVariantList list;
    // 第 0 项固定是「跟随默认设备」：目标设备字段留空，求值时按当时的默认设备解析
    QVariantMap follow;
    follow.insert(QStringLiteral("address"), QString());
    follow.insert(QStringLiteral("name"), device_label_for(QString()));
    follow.insert(QStringLiteral("online"), device_ != nullptr && !device_->default_device_address().isEmpty());
    list.append(follow);
    if (device_ == nullptr) {
        return list;
    }
    for (const QVariant& item : device_->outputDeviceTargets()) {
        const QVariantMap target = item.toMap();
        QVariantMap entry;
        entry.insert(QStringLiteral("address"), target.value(QStringLiteral("address")));
        entry.insert(QStringLiteral("name"), target.value(QStringLiteral("name")));
        entry.insert(QStringLiteral("online"), target.value(QStringLiteral("online")));
        list.append(entry);
    }
    return list;
}

QString RuleGraphBridge::device_label_for(const QString& target_device) const {
    const QString device = rule_graph_normalize_device(target_device);
    // 跟随默认设备：文案带上当前默认设备，避免用户不知道会打在哪台设备上
    if (device.isEmpty()) {
        const QString fallback = device_ == nullptr ? QString() : device_->default_device_address();
        return fallback.isEmpty() ? QStringLiteral("跟随默认设备（当前无默认设备）")
                                  : QStringLiteral("跟随默认设备（%1）").arg(fallback);
    }
    if (device_ != nullptr) {
        for (const QVariant& item : device_->outputDeviceTargets()) {
            const QVariantMap target = item.toMap();
            if (rule_graph_normalize_device(target.value(QStringLiteral("address")).toString()) == device) {
                return target.value(QStringLiteral("name")).toString();
            }
        }
    }
    return device + QStringLiteral(" · 离线");
}

int RuleGraphBridge::channelDeviceIndex(const QString& target_device) const {
    const QString device = rule_graph_normalize_device(target_device);
    const QVariantList options = channel_device_options();
    for (int index = 0; index < options.size(); ++index) {
        const QString address = rule_graph_normalize_device(
            options.at(index).toMap().value(QStringLiteral("address")).toString());
        if (address == device) {
            return index;
        }
    }
    // 目标设备已不在列表（记忆被清掉等）：退回「跟随默认设备」，不回写模型
    return 0;
}

void RuleGraphBridge::setChannelDeviceTargetByIndex(int node_id, int index) {
    GraphNode* node = graph_.find_node(node_id);
    if (node == nullptr || node->type != GraphNodeType::Channel) {
        return;
    }
    const QVariantList options = channel_device_options();
    if (index < 0 || index >= options.size()) {
        return;
    }
    const QString address = rule_graph_normalize_device(
        options.at(index).toMap().value(QStringLiteral("address")).toString());
    if (node->target_device == address) {
        return;
    }
    node->target_device = address;
    mark_dirty();
    emit selectionChanged();
    emit graphChanged();
    emit statusMessage(address.isEmpty() ? QStringLiteral("通道 %1 已改为跟随默认设备").arg(node->channel)
                                         : QStringLiteral("通道 %1 的目标设备已设为 %2").arg(node->channel, address));
}

QVariantMap RuleGraphBridge::selected_node() const {
    const GraphNode* node = graph_.find_node(selected_node_id_);
    return node == nullptr ? QVariantMap() : node_to_map(*node);
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
    current_graph_plugin_.clear();
    graph_.load(file_name);
    apply_default_device();
    selected_node_id_ = 0;
    dirty_ = false;
    emit dirtyChanged();
    emit selectionChanged();
    emit graphChanged();
    emit statusMessage(QStringLiteral("已载入规则文件：") + file_name);
}

bool RuleGraphBridge::save() {
    if (current_graph_plugin_.isEmpty() && current_rule_file_.isEmpty()) {
        return false;
    }
    const bool ok = current_graph_plugin_.isEmpty()
        ? graph_.save(current_rule_file_)
        : RuleManager::instance().save_plugin_rule_graph(current_graph_plugin_.toStdString(), graph_);
    dirty_ = !ok;
    emit dirtyChanged();
    emit graphChanged();
    emit pluginGraphsChanged();
    emit statusMessage(ok ? QStringLiteral("规则图已保存") : QStringLiteral("规则图保存失败"));
    return ok;
}

void RuleGraphBridge::loadPluginGraph(const QString& plugin_id) {
    // 空标识表示回到总规则图：从规则文件重载，丢弃插件图上的未保存改动
    if (plugin_id.isEmpty()) {
        if (current_rule_file_.isEmpty()) {
            graph_ = RuleGraph();
            graph_.import_from_rules();
        }
        else {
            graph_.load(current_rule_file_);
        }
        current_graph_plugin_.clear();
        selected_node_id_ = 0;
        dirty_ = false;
        emit dirtyChanged();
        emit selectionChanged();
        emit graphChanged();
        emit statusMessage(QStringLiteral("已返回总规则图"));
        return;
    }
    graph_ = RuleManager::instance().get_plugin_rule_graph(plugin_id.toStdString());
    apply_default_device();
    current_graph_plugin_ = plugin_id;
    selected_node_id_ = 0;
    dirty_ = false;
    emit dirtyChanged();
    emit selectionChanged();
    emit graphChanged();
    emit statusMessage(QStringLiteral("已载入插件规则图：") + plugin_id);
}

QVariantMap RuleGraphBridge::graphDependencies() const {
    return graphDependencies(0);
}

QVariantMap RuleGraphBridge::graphDependencies(int from_node_id) const {
    const GraphDependency dependency = RuleManager::instance().get_graph_dependencies(graph_, from_node_id);
    QVariantMap result;
    result.insert(QStringLiteral("rules"), to_qstring_list(dependency.rules));
    result.insert(QStringLiteral("valueIds"), to_qstring_list(dependency.value_ids));
    result.insert(QStringLiteral("channels"), to_qstring_list(dependency.channels));
    result.insert(QStringLiteral("plugins"), to_qstring_list(dependency.plugins));
    result.insert(QStringLiteral("events"), to_qstring_list(dependency.events));
    return result;
}

QStringList RuleGraphBridge::outputReferrers(const QString& plugin_id) const {
    QStringList labels;
    for (const auto& reference : RuleManager::instance().get_output_referrers(plugin_id.toStdString())) {
        labels.append(rule_graph_ref_display(QString::fromStdString(reference)));
    }
    return labels;
}

void RuleGraphBridge::refreshModuleValues() {
    rebuild_module_values();
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
    // 缺省跟随默认设备：target_device 留空，求值时按当时的默认设备解析
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

int RuleGraphBridge::addPluginOutputNode(const QString& plugin_id, double x, double y) {
    if (plugin_id.isEmpty()) {
        return 0;
    }
    // 插件输出节点只属于总规则图：插件图里的出口是默认输出节点
    if (!current_graph_plugin_.isEmpty()) {
        emit statusMessage(QStringLiteral("插件规则图内不能添加插件输出节点"));
        return 0;
    }
    // 同一插件只有一个输出节点：已存在时选中它，不重复添加
    for (const auto& existing : graph_.nodes()) {
        if (existing.type == GraphNodeType::PluginOutput && existing.plugin_id == plugin_id) {
            selected_node_id_ = existing.id;
            emit selectionChanged();
            emit statusMessage(QStringLiteral("插件输出节点已存在：") + existing.name);
            return existing.id;
        }
    }
    GraphNode node;
    node.type = GraphNodeType::PluginOutput;
    node.plugin_id = plugin_id;
    node.references = QStringList{ rule_graph_plugin_ref(plugin_id) };
    node.name = QStringLiteral("%1 输出").arg(plugin_display_name(plugin_id));
    node.x = x;
    node.y = y;
    const int id = graph_.add_node(node);
    selected_node_id_ = id;
    mark_dirty();
    emit selectionChanged();
    emit graphChanged();
    emit statusMessage(QStringLiteral("已添加插件输出节点：") + node.name);
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
    if (node->type == GraphNodeType::Rule || (node->type == GraphNodeType::Advanced && node->advanced == QStringLiteral("expression"))) {
        node->expression = expression;
        mark_dirty();
        emit selectionChanged();
        emit graphChanged();
    }
}

void RuleGraphBridge::connectNodes(int from, int to, int port) {
    // 唯一入边按「设备 + 通道」判定：先把当前默认设备写进图，跟随默认设备的节点才能落成同一作用域
    apply_default_device();
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

void RuleGraphBridge::mark_dirty() {
    if (!dirty_) {
        dirty_ = true;
        emit dirtyChanged();
    }
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
    item.insert(QStringLiteral("targetDevice"), node.target_device);
    item.insert(QStringLiteral("deviceLabel"), device_label_for(node.target_device));
    item.insert(QStringLiteral("mode"), node.mode);
    item.insert(QStringLiteral("enabled"), node.enabled);
    item.insert(QStringLiteral("x"), node.x);
    item.insert(QStringLiteral("y"), node.y);
    item.insert(QStringLiteral("pluginId"), node.plugin_id);
    item.insert(QStringLiteral("references"), node.references);
    QStringList reference_labels;
    for (const auto& reference : node.references) {
        reference_labels.append(rule_graph_ref_display(reference));
    }
    item.insert(QStringLiteral("referenceLabels"), reference_labels);
    item.insert(QStringLiteral("inputCount"), graph_.input_count(node.id));

    QStringList labels;
    const int inputs = graph_.input_count(node.id);
    for (int i = 0; i < inputs; ++i) {
        labels.append(graph_.input_label(node.id, i));
    }
    item.insert(QStringLiteral("inputLabels"), labels);
    // 节点占用的行数（高度公式的唯一来源）：输入端口 + 通道节点的目标设备行 + 默认输出节点的引用行
    int rows = std::max(1, graph_.input_count(node.id));
    if (node.type == GraphNodeType::Channel) {
        rows += 1;
    }
    if (node.type == GraphNodeType::DefaultOutput) {
        rows += static_cast<int>(node.references.size());
    }
    item.insert(QStringLiteral("rowCount"), rows);
    item.insert(QStringLiteral("width"), kNodeWidth);
    item.insert(QStringLiteral("headerHeight"), kHeaderHeight);
    item.insert(QStringLiteral("portSpacing"), kPortSpacing);
    item.insert(QStringLiteral("portInset"), kPortInset);
    return item;
}

QPointF RuleGraphBridge::input_port_position(const GraphNode& node, int port) const {
    return QPointF(node.x, node.y + kHeaderHeight + kPortInset + port * kPortSpacing);
}

QPointF RuleGraphBridge::output_port_position(const GraphNode& node) const {
    return QPointF(node.x + kNodeWidth, node.y + kHeaderHeight / 2.0);
}

