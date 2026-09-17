/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "RuleGraph.h"

#include "AppConfig.h"
#include "DebugLog.h"
#include "ModuleManager.h"
#include "RuleManager.h"

#include <QDir>
#include <QFile>
#include <QList>
#include <QSet>
#include <QVector>

#include <algorithm>
#include <map>

namespace {

    struct PlaceholderInfo {
        int pos = 0;
        int len = 0;
        QString inner;
    };

    /// @brief 解析顶层 {} 占位符（不处理嵌套）
    QVector<PlaceholderInfo> parse_placeholders(const QString& text) {
        QVector<PlaceholderInfo> list;
        int depth = 0;
        int start = -1;
        for (int i = 0; i < text.size(); ++i) {
            const QChar c = text.at(i);
            if (c == QLatin1Char('{')) {
                if (depth == 0) {
                    start = i;
                }
                ++depth;
            }
            else if (c == QLatin1Char('}')) {
                --depth;
                if (depth == 0 && start >= 0) {
                    PlaceholderInfo info;
                    info.pos = start;
                    info.len = i - start + 1;
                    info.inner = text.mid(start + 1, i - start - 1);
                    list.append(info);
                    start = -1;
                }
            }
        }
        return list;
    }

    /// @brief 从 {id:xxx(名称)} 中取出显示名
    QString label_from_placeholder(const QString& inner) {
        if (inner.startsWith(QStringLiteral("id:"))) {
            const QString body = inner.mid(3);
            const int open = body.indexOf(QLatin1Char('('));
            if (open > 0) {
                return body.mid(open + 1, body.length() - open - 2);
            }
            return body;
        }
        if (inner.startsWith(QStringLiteral("rule:"))) {
            bool ok = false;
            const int index = inner.mid(5).toInt(&ok);
            if (ok) {
                return QString::fromStdString(RuleManager::instance().get_rule_name_by_index(index));
            }
        }
        return QString();
    }

} // namespace

QString graph_node_type_to_string(GraphNodeType type) {
    switch (type) {
    case GraphNodeType::Rule: return QStringLiteral("rule");
    case GraphNodeType::Module: return QStringLiteral("module");
    case GraphNodeType::Operator: return QStringLiteral("operator");
    case GraphNodeType::Advanced: return QStringLiteral("advanced");
    case GraphNodeType::Channel: return QStringLiteral("channel");
    }
    return QStringLiteral("rule");
}

GraphNodeType graph_node_type_from_string(const QString& text) {
    if (text == QStringLiteral("module")) return GraphNodeType::Module;
    if (text == QStringLiteral("operator")) return GraphNodeType::Operator;
    if (text == QStringLiteral("advanced")) return GraphNodeType::Advanced;
    if (text == QStringLiteral("channel")) return GraphNodeType::Channel;
    return GraphNodeType::Rule;
}

GraphNode* RuleGraph::find_node(int id) {
    for (auto& node : nodes_) {
        if (node.id == id) {
            return &node;
        }
    }
    return nullptr;
}

const GraphNode* RuleGraph::find_node(int id) const {
    for (const auto& node : nodes_) {
        if (node.id == id) {
            return &node;
        }
    }
    return nullptr;
}

QString RuleGraph::placeholder_text(int node_id, int port) const {
    const GraphNode* node = find_node(node_id);
    if (node == nullptr || node->type != GraphNodeType::Rule) {
        return QString();
    }
    const auto placeholders = parse_placeholders(node->expression);
    if (port < 0 || port >= placeholders.size()) {
        return QString();
    }
    return placeholders.at(port).inner;
}

int RuleGraph::input_count(int id) const {
    const GraphNode* node = find_node(id);
    if (node == nullptr) {
        return 0;
    }
    switch (node->type) {
    case GraphNodeType::Rule:
        return static_cast<int>(parse_placeholders(node->expression).size());
    case GraphNodeType::Operator:
        return 2;
    case GraphNodeType::Advanced:
        if (node->advanced == QStringLiteral("expression")) {
            return std::max(1, static_cast<int>(parse_placeholders(node->expression).size()));
        }
        return 1;
    default:
        return 0;
    }
}

QString RuleGraph::input_label(int id, int port) const {
    const GraphNode* node = find_node(id);
    if (node == nullptr) {
        return QString();
    }
    if (node->type == GraphNodeType::Rule) {
        const QString inner = placeholder_text(id, port);
        if (inner.isEmpty()) {
            return QStringLiteral("输入%1").arg(port + 1);
        }
        const QString label = label_from_placeholder(inner);
        return label.isEmpty() ? inner : label;
    }
    if (node->type == GraphNodeType::Operator) {
        return port == 0 ? QStringLiteral("A") : QStringLiteral("B");
    }
    if (node->type == GraphNodeType::Advanced) {
        return QStringLiteral("输入%1").arg(port + 1);
    }
    return QString();
}

int RuleGraph::add_node(const GraphNode& node) {
    GraphNode copy = node;
    copy.id = next_node_id_++;
    nodes_.push_back(copy);
    return copy.id;
}

bool RuleGraph::remove_node(int id) {
    const auto it = std::find_if(nodes_.begin(), nodes_.end(), [id](const GraphNode& n) { return n.id == id; });
    if (it == nodes_.end()) {
        return false;
    }
    nodes_.erase(it);
    edges_.erase(std::remove_if(edges_.begin(), edges_.end(),
                     [id](const GraphEdge& e) { return e.from == id || e.to == id; }),
        edges_.end());
    return true;
}

bool RuleGraph::move_node(int id, double x, double y) {
    GraphNode* node = find_node(id);
    if (node == nullptr) {
        return false;
    }
    node->x = x;
    node->y = y;
    return true;
}

int RuleGraph::edge_at_port(int to, int port) const {
    for (const auto& edge : edges_) {
        if (edge.to == to && edge.port == port) {
            return edge.id;
        }
    }
    return -1;
}

int RuleGraph::source_at_port(int to, int port) const {
    for (const auto& edge : edges_) {
        if (edge.to == to && edge.port == port) {
            return edge.from;
        }
    }
    return -1;
}

bool RuleGraph::has_edge(int from, int to, int port) const {
    return source_at_port(to, port) == from;
}

bool RuleGraph::would_create_cycle(int from, int to) const {
    // 从 from 沿反向边向上游回溯，若遇到 to 则成环
    QSet<int> visited;
    QList<int> stack;
    stack.append(from);
    while (!stack.isEmpty()) {
        const int current = stack.takeLast();
        if (current == to) {
            return true;
        }
        if (visited.contains(current)) {
            continue;
        }
        visited.insert(current);
        for (const auto& edge : edges_) {
            if (edge.to == current) {
                stack.append(edge.from);
            }
        }
    }
    return false;
}

int RuleGraph::connect_nodes(int from, int to, int port) {
    if (from == to || find_node(from) == nullptr || find_node(to) == nullptr) {
        return -1;
    }
    if (would_create_cycle(from, to)) {
        LOG_MODULE("RuleGraph", "connect_nodes", LOG_WARN, "忽略会形成环的连线");
        return -1;
    }
    const GraphNode* target = find_node(to);
    // 通道输出只允许一个来源
    if (target != nullptr && target->type == GraphNodeType::Channel) {
        port = 0;
        edges_.erase(std::remove_if(edges_.begin(), edges_.end(),
                         [to](const GraphEdge& e) { return e.to == to; }),
            edges_.end());
    }
    else {
        disconnect_port(to, port);
    }
    GraphEdge edge;
    edge.id = next_edge_id_++;
    edge.from = from;
    edge.to = to;
    edge.port = port;
    edges_.push_back(edge);
    return edge.id;
}

bool RuleGraph::disconnect_edge(int edge_id) {
    const auto it = std::find_if(edges_.begin(), edges_.end(), [edge_id](const GraphEdge& e) { return e.id == edge_id; });
    if (it == edges_.end()) {
        return false;
    }
    edges_.erase(it);
    return true;
}

bool RuleGraph::disconnect_port(int to, int port) {
    const int edge_id = edge_at_port(to, port);
    return edge_id >= 0 ? disconnect_edge(edge_id) : false;
}

int RuleGraph::rule_index_of(const QString& rule_name) const {
    QStringList names;
    for (const auto& node : nodes_) {
        if (node.type == GraphNodeType::Rule) {
            names.append(node.rule_name);
        }
    }
    names.sort();
    const int index = names.indexOf(rule_name);
    return index < 0 ? -1 : index + 1;
}

QString RuleGraph::source_formula(int node_id, int port, int depth) const {
    const int source = source_at_port(node_id, port);
    if (source <= 0) {
        return QStringLiteral("{}");
    }
    return formula_of(source, depth + 1);
}

QString RuleGraph::formula_of(int node_id, int depth) const {
    if (depth > 32) {
        return QStringLiteral("{}");
    }
    const GraphNode* node = find_node(node_id);
    if (node == nullptr) {
        return QStringLiteral("{}");
    }
    switch (node->type) {
    case GraphNodeType::Module:
        return QStringLiteral("{id:%1(%2)}")
            .arg(node->value_id, node->label.isEmpty() ? node->value_id : node->label);
    case GraphNodeType::Rule: {
        const int index = rule_index_of(node->rule_name);
        return index > 0 ? QStringLiteral("{rule:%1}").arg(index) : QStringLiteral("{}");
    }
    case GraphNodeType::Operator: {
        const QString a = source_formula(node_id, 0, depth);
        const QString b = source_formula(node_id, 1, depth);
        return QStringLiteral("Math.floor((%1) %2 (%3))").arg(a, node->op, b);
    }
    case GraphNodeType::Advanced: {
        const QString a = source_formula(node_id, 0, depth);
        if (node->advanced == QStringLiteral("abs")) {
            return QStringLiteral("Math.floor(Math.abs(%1))").arg(a);
        }
        if (node->advanced == QStringLiteral("square")) {
            return QStringLiteral("Math.floor((%1) * (%1))").arg(a);
        }
        if (node->advanced == QStringLiteral("sqrt")) {
            return QStringLiteral("Math.floor(Math.sqrt(%1))").arg(a);
        }
        QString expression = node->expression;
        const auto placeholders = parse_placeholders(expression);
        for (int i = placeholders.size() - 1; i >= 0; --i) {
            expression.replace(placeholders.at(i).pos, placeholders.at(i).len,
                source_formula(node_id, i, depth));
        }
        return QStringLiteral("Math.floor(%1)").arg(expression);
    }
    default:
        return QStringLiteral("{}");
    }
}

void RuleGraph::import_from_rules() {
    nodes_.clear();
    edges_.clear();
    next_node_id_ = 1;
    next_edge_id_ = 1;

    auto& rules = RuleManager::instance();
    auto& modules = ModuleManager::instance();

    std::map<std::string, int> module_nodes;
    std::map<std::string, int> rule_nodes;

    const auto names = rules.get_rule_names();
    int rule_row = 0;
    for (const auto& name : names) {
        GraphNode node;
        node.type = GraphNodeType::Rule;
        node.rule_name = QString::fromStdString(name);
        node.name = node.rule_name;
        node.expression = QString::fromStdString(rules.get_rule_value_pattern(name));
        node.mode = rules.get_rule_mode(name);
        node.enabled = rules.get_rule_enabled(name);
        node.x = 420;
        node.y = 60 + rule_row * 150;
        ++rule_row;
        rule_nodes[name] = add_node(node);
    }

    // 占位符中的模块引用 -> 模块源节点 + 连线
    for (const auto& [name, rule_node_id] : rule_nodes) {
        const GraphNode* node = find_node(rule_node_id);
        const auto placeholders = parse_placeholders(node->expression);
        for (int i = 0; i < placeholders.size(); ++i) {
            const QString inner = placeholders.at(i).inner;
            if (inner.startsWith(QStringLiteral("id:"))) {
                const QString body = inner.mid(3);
                const int open = body.indexOf(QLatin1Char('('));
                const QString value_id = (open > 0) ? body.left(open) : body;
                const QString label = (open > 0) ? body.mid(open + 1, body.length() - open - 2) : value_id;

                const std::string key = value_id.toStdString();
                int source_id = 0;
                if (module_nodes.count(key) > 0) {
                    source_id = module_nodes[key];
                }
                else {
                    GraphNode module_node;
                    module_node.type = GraphNodeType::Module;
                    module_node.value_id = value_id;
                    module_node.label = label;
                    module_node.name = label;
                    const std::string module_name = modules.find_module_by_value_id(key);
                    module_node.x = 40;
                    module_node.y = 60 + static_cast<int>(module_nodes.size()) * 70;
                    source_id = add_node(module_node);
                    module_nodes[key] = source_id;
                }
                connect_nodes(source_id, rule_node_id, i);
            }
        }
    }

    // 父级关系 -> 连线（父级是消费方）
    for (const auto& [name, rule_node_id] : rule_nodes) {
        QStringList channels;
        std::vector<int> consumers;
        for (const auto& parent : rules.get_rule_parents(name)) {
            if (parent.type == ParentType::CHANNEL) {
                channels.append(QString::fromStdString(parent.channel));
            }
            else if (parent.type == ParentType::RULE && parent.rule_index > 0) {
                consumers.push_back(parent.rule_index);
            }
        }
        for (const auto& channel : channels) {
            GraphNode channel_node;
            channel_node.type = GraphNodeType::Channel;
            channel_node.channel = channel;
            channel_node.name = QStringLiteral("通道 %1").arg(channel);
            channel_node.x = 840;
            channel_node.y = (channel == QStringLiteral("A")) ? 60 : 220;
            // 通道节点复用
            int channel_id = 0;
            for (const auto& existing : nodes_) {
                if (existing.type == GraphNodeType::Channel && existing.channel == channel) {
                    channel_id = existing.id;
                    break;
                }
            }
            if (channel_id == 0) {
                channel_id = add_node(channel_node);
            }
            connect_nodes(rule_node_id, channel_id, 0);
        }
        for (const int consumer_index : consumers) {
            const std::string consumer_name = rules.get_rule_name_by_index(consumer_index);
            if (rule_nodes.count(consumer_name) == 0) {
                continue;
            }
            const int consumer_id = rule_nodes[consumer_name];
            // 找到消费方模式中引用本规则的端口
            const GraphNode* consumer = find_node(consumer_id);
            int port = 0;
            const auto placeholders = parse_placeholders(consumer->expression);
            for (int i = 0; i < placeholders.size(); ++i) {
                if (placeholders.at(i).inner == QStringLiteral("rule:%1").arg(rule_index_of(QString::fromStdString(name)))) {
                    port = i;
                    break;
                }
            }
            connect_nodes(rule_node_id, consumer_id, port);
        }
    }
    LOG_MODULE("RuleGraph", "import_from_rules", LOG_INFO,
        "已从规则集导入 " << nodes_.size() << " 个节点");
}

nlohmann::json RuleGraph::to_rules_json() const {
    nlohmann::json rules = nlohmann::json::object();

    for (const auto& node : nodes_) {
        if (node.type != GraphNodeType::Rule) {
            continue;
        }
        QString pattern = node.expression;
        const auto placeholders = parse_placeholders(pattern);
        // 从后往前替换，避免位置偏移
        for (int i = placeholders.size() - 1; i >= 0; --i) {
            const int source = source_at_port(node.id, i);
            if (source <= 0) {
                continue;
            }
            const GraphNode* source_node = find_node(source);
            if (source_node == nullptr || source_node->type == GraphNodeType::Channel) {
                continue;
            }
            pattern.replace(placeholders.at(i).pos, placeholders.at(i).len, formula_of(source, 1));
        }

        nlohmann::json parents = nlohmann::json::array();
        // 通道父级
        for (const auto& edge : edges_) {
            if (edge.from != node.id) {
                continue;
            }
            const GraphNode* target = find_node(edge.to);
            if (target == nullptr) {
                continue;
            }
            if (target->type == GraphNodeType::Channel) {
                parents.push_back(target->channel.toStdString());
            }
            else if (target->type == GraphNodeType::Rule) {
                const int index = rule_index_of(target->rule_name);
                if (index > 0) {
                    parents.push_back(index);
                }
            }
        }

        nlohmann::json entry;
        entry["enabled"] = node.enabled;
        entry["parents"] = parents;
        entry["mode"] = node.mode;
        entry["valuePattern"] = pattern.toStdString();
        rules[node.rule_name.toStdString()] = entry;
    }
    return rules;
}

nlohmann::json RuleGraph::to_json() const {
    nlohmann::json json;
    json["version"] = "1.0";
    json["nextNodeId"] = next_node_id_;
    json["nextEdgeId"] = next_edge_id_;

    nlohmann::json node_array = nlohmann::json::array();
    for (const auto& node : nodes_) {
        node_array.push_back({ { "id", node.id },
            { "type", graph_node_type_to_string(node.type).toStdString() },
            { "name", node.name.toStdString() },
            { "ruleName", node.rule_name.toStdString() },
            { "valueId", node.value_id.toStdString() },
            { "label", node.label.toStdString() },
            { "op", node.op.toStdString() },
            { "advanced", node.advanced.toStdString() },
            { "expression", node.expression.toStdString() },
            { "channel", node.channel.toStdString() },
            { "mode", node.mode },
            { "enabled", node.enabled },
            { "x", node.x },
            { "y", node.y } });
    }
    json["nodes"] = node_array;

    nlohmann::json edge_array = nlohmann::json::array();
    for (const auto& edge : edges_) {
        edge_array.push_back({ { "id", edge.id }, { "from", edge.from }, { "to", edge.to }, { "port", edge.port } });
    }
    json["edges"] = edge_array;
    return json;
}

void RuleGraph::from_json(const nlohmann::json& json) {
    nodes_.clear();
    edges_.clear();
    next_node_id_ = json.value("nextNodeId", 1);
    next_edge_id_ = json.value("nextEdgeId", 1);

    if (json.contains("nodes") && json["nodes"].is_array()) {
        for (const auto& item : json["nodes"]) {
            GraphNode node;
            node.id = item.value("id", 0);
            node.type = graph_node_type_from_string(QString::fromStdString(item.value("type", "rule")));
            node.name = QString::fromStdString(item.value("name", ""));
            node.rule_name = QString::fromStdString(item.value("ruleName", ""));
            node.value_id = QString::fromStdString(item.value("valueId", ""));
            node.label = QString::fromStdString(item.value("label", ""));
            node.op = QString::fromStdString(item.value("op", "+"));
            node.advanced = QString::fromStdString(item.value("advanced", "abs"));
            node.expression = QString::fromStdString(item.value("expression", ""));
            node.channel = QString::fromStdString(item.value("channel", "A"));
            node.mode = item.value("mode", 1);
            node.enabled = item.value("enabled", true);
            node.x = item.value("x", 0.0);
            node.y = item.value("y", 0.0);
            if (node.id <= 0) {
                continue;
            }
            nodes_.push_back(node);
            next_node_id_ = std::max(next_node_id_, node.id + 1);
        }
    }

    if (json.contains("edges") && json["edges"].is_array()) {
        for (const auto& item : json["edges"]) {
            GraphEdge edge;
            edge.id = item.value("id", 0);
            edge.from = item.value("from", 0);
            edge.to = item.value("to", 0);
            edge.port = item.value("port", 0);
            if (edge.id <= 0 || edge.from <= 0 || edge.to <= 0) {
                continue;
            }
            edges_.push_back(edge);
            next_edge_id_ = std::max(next_edge_id_, edge.id + 1);
        }
    }
}

QString RuleGraph::sidecar_path(const QString& rule_file) {
    const std::string dir = AppConfig::instance().get_value<std::string>("rule.path", "./config/rules");
    return QString::fromStdString(dir) + QLatin1Char('/') + rule_file + QStringLiteral(".graph.json");
}

bool RuleGraph::load(const QString& rule_file) {
    QFile sidecar(sidecar_path(rule_file));
    if (sidecar.exists() && sidecar.open(QIODevice::ReadOnly)) {
        const QByteArray data = sidecar.readAll();
        sidecar.close();
        try {
            from_json(nlohmann::json::parse(data.toStdString()));
            LOG_MODULE("RuleGraph", "load", LOG_INFO, "已载入规则图侧车文件");
            return true;
        }
        catch (const std::exception& e) {
            LOG_MODULE("RuleGraph", "load", LOG_WARN, "侧车文件解析失败，改为从规则集导入: " << e.what());
        }
    }
    import_from_rules();
    return true;
}

bool RuleGraph::save(const QString& rule_file) {
    QFile sidecar(sidecar_path(rule_file));
    if (sidecar.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        sidecar.write(QString::fromStdString(to_json().dump(2)).toUtf8());
        sidecar.close();
    }
    const nlohmann::json rules = to_rules_json();
    const bool ok = RuleManager::instance().modify_rule_file(rule_file.toStdString(), rules);
    LOG_MODULE("RuleGraph", "save", ok ? LOG_INFO : LOG_ERROR,
        ok ? "规则图已写回规则文件" : "规则图写回失败");
    return ok;
}
