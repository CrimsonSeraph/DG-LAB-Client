/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "RuleManager.h"

#include "AppConfig.h"
#include "ConfigManager.h"
#include "DebugLog.h"
#include "ModuleManager.h"

#include <QJsonObject>
#include <QRegularExpression>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <mutex>

namespace fs = std::filesystem;

namespace {

    /// @brief 合并并去重（依赖集合输出稳定）
    void merge_unique(std::vector<std::string>& target, const std::vector<std::string>& values) {
        target.insert(target.end(), values.begin(), values.end());
        std::sort(target.begin(), target.end());
        target.erase(std::unique(target.begin(), target.end()), target.end());
    }

    /// @brief 规则图中指定通道的通道节点 id（无匹配返回 0）
    int channel_node_of(const RuleGraph& graph, const std::string& channel) {
        for (const auto& node : graph.nodes()) {
            if (node.type == GraphNodeType::Channel
                && Rule::normalize_channel(node.channel.toStdString()) == channel) {
                return node.id;
            }
        }
        return 0;
    }

} // namespace

// 单例（public）

RuleManager& RuleManager::instance() {
    static RuleManager manager;
    return manager;
}

// 构造/析构（private）

RuleManager::RuleManager()
    : QObject(nullptr) {
    // 监听模块数值变化，触发值模式中引用该数值的规则计算（级联触发）
    connect(&ModuleManager::instance(), &ModuleManager::value_changed,
        this, &RuleManager::on_module_value_changed);
}

RuleManager::~RuleManager() = default;

// 初始化（public）

void RuleManager::init() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& config = AppConfig::instance();
    std::string rule_path = config.get_value<std::string>("rule.path", "./config/rules");
    std::string rule_key = config.get_value<std::string>("rule.key", "rule");
    rules_dir_ = rule_path;
    keyword_ = rule_key;

    try {
        fs::create_directories(rules_dir_);
    }
    catch (const std::exception& e) {
        LOG_MODULE("RuleManager", "init", LOG_ERROR, "创建目录失败: " << e.what());
    }
    scan_directory();
}

// 文件管理（public）

std::vector<std::string> RuleManager::get_available_rule_files() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return available_files_;
}

void RuleManager::load_rule_file(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        auto json = load_json_file(filename);
        if (!json.contains("rules")) {
            LOG_MODULE("RuleManager", "load_rule_file", LOG_WARN, "文件缺少 'rules' 字段: " << filename);
            return;
        }
        parse_config(json["rules"]);
        current_file_ = filename;
        LOG_MODULE("RuleManager", "load_rule_file", LOG_INFO, "已加载规则文件: " << filename);
    }
    catch (const std::exception& e) {
        LOG_MODULE("RuleManager", "load_rule_file", LOG_ERROR, "加载失败: " << e.what());
        throw;
    }
}

bool RuleManager::create_rule_file(const std::string& filename, const nlohmann::json& rules_content) {
    if (filename == "rules.json") {
        LOG_MODULE("RuleManager", "create_rule_file", LOG_WARN, "不能创建默认规则文件 rules.json");
        return false;
    }
    std::string full_path = get_full_path(filename);
    if (fs::exists(full_path)) {
        LOG_MODULE("RuleManager", "create_rule_file", LOG_WARN, "文件已存在: " << filename);
        return false;
    }
    nlohmann::json j;
    j["rules"] = rules_content;
    if (save_json_file(filename, j)) {
        scan_directory();
        return true;
    }
    return false;
}

bool RuleManager::modify_rule_file(const std::string& filename, const nlohmann::json& rules_content) {
    if (filename.empty()) return false;
    std::string full_path = get_full_path(filename);
    if (!fs::exists(full_path)) {
        LOG_MODULE("RuleManager", "modify_rule_file", LOG_WARN, "文件不存在: " << filename);
        return false;
    }
    nlohmann::json j;
    try {
        j = load_json_file(filename);
    }
    catch (...) {
    }
    j["rules"] = rules_content;
    if (save_json_file(filename, j)) {
        if (current_file_ == filename) {
            parse_config(rules_content);
        }
        return true;
    }
    return false;
}

bool RuleManager::delete_rule_file(const std::string& filename) {
    if (filename == "rules.json") {
        LOG_MODULE("RuleManager", "delete_rule_file", LOG_WARN, "不能删除默认规则文件 rules.json");
        return false;
    }
    std::string full_path = get_full_path(filename);
    if (!fs::exists(full_path)) return false;
    if (fs::remove(full_path)) {
        if (current_file_ == filename) {
            try {
                load_rule_file("rules.json");
            }
            catch (...) {
            }
        }
        scan_directory();
        return true;
    }
    return false;
}

bool RuleManager::save_current_rule_file() {
    if (current_file_.empty()) {
        LOG_MODULE("RuleManager", "save_current_rule_file", LOG_WARN, "没有当前加载的规则文件");
        return false;
    }
    nlohmann::json rules_json;
    // 按规则序号顺序输出，保证序号稳定
    for (const auto& [index, name] : index_to_name_) {
        auto it = rules_.find(name);
        if (it == rules_.end()) {
            continue;
        }
        const Rule& rule = it->second;
        nlohmann::json parents_json = nlohmann::json::array();
        for (const auto& parent : rule.get_parents()) {
            if (parent.type == ParentType::CHANNEL) {
                parents_json.push_back(parent.channel);
            }
            else if (parent.type == ParentType::RULE) {
                parents_json.push_back(parent.rule_index);
            }
        }
        rules_json[name] = {
            {"enabled", rule.get_enabled()},
            {"parents", parents_json},
            {"mode", rule.get_mode()},
            {"valuePattern", rule.get_value_pattern()}};
    }
    return modify_rule_file(current_file_, rules_json);
}

// 规则图（总规则图 + 插件级规则图）（public）

std::string RuleManager::get_plugin_graph_file(const std::string& plugin_id) const {
    return "plugin_" + plugin_id + ".json";
}

std::vector<std::string> RuleManager::get_plugin_rule_graphs() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return plugin_graph_plugins_locked();
}

bool RuleManager::has_plugin_rule_graph(const std::string& plugin_id) const {
    if (plugin_id.empty()) {
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    return fs::exists(get_full_path(get_plugin_graph_file(plugin_id)));
}

RuleGraph RuleManager::get_plugin_rule_graph(const std::string& plugin_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return load_plugin_rule_graph_locked(plugin_id);
}

bool RuleManager::write_plugin_rule_graph(const std::string& plugin_id, const RuleGraph& graph,
    bool& cascade_dirty) {
    cascade_dirty = false;
    if (plugin_id.empty()) {
        LOG_MODULE("RuleManager", "write_plugin_rule_graph", LOG_WARN, "插件标识为空，无法保存插件规则图");
        return false;
    }
    bool saved = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const std::string filename = get_plugin_graph_file(plugin_id);
        nlohmann::json json;
        try {
            json = load_json_file(filename);
        }
        catch (const std::exception&) {
            // 文件不存在：新建
        }
        if (!json.is_object()) {
            json = nlohmann::json::object();
        }
        nlohmann::json graph_json = graph.to_json();
        graph_json["plugin"] = plugin_id;
        json["version"] = "1.0";
        json["plugin"] = plugin_id;
        json["kind"] = "pluginRuleGraph";
        json["graph"] = graph_json;
        json["rules"] = graph.to_rules_json();
        saved = save_json_file(filename, json);
        if (saved) {
            scan_directory();
            // 图已变化：按新图重算已启用来源的依赖集合（下一次求值同样读新图）
            cascade_dirty = refresh_cascades_locked();
            LOG_MODULE("RuleManager", "write_plugin_rule_graph", LOG_INFO,
                "插件规则图已保存: " << filename);
        }
    }
    return saved;
}

bool RuleManager::save_plugin_rule_graph(const std::string& plugin_id, const RuleGraph& graph) {
    bool cascade_dirty = false;
    const bool saved = write_plugin_rule_graph(plugin_id, graph, cascade_dirty);
    if (saved) {
        sync_output_references();
    }
    if (cascade_dirty) {
        emit cascade_changed();
    }
    return saved;
}

bool RuleManager::save_rule_graphs(const std::string& top_file, const RuleGraph* top_graph,
    const std::map<std::string, RuleGraph>& plugin_graphs,
    std::vector<std::string>& failures) {
    failures.clear();
    bool wrote_any = false;
    bool cascade_dirty = false;
    // 总规则图先写：插件图保存后重建引用列表时会读到总图里的插件输出节点
    if (top_graph != nullptr) {
        bool saved = false;
        if (top_file.empty()) {
            LOG_MODULE("RuleManager", "save_rule_graphs", LOG_WARN, "总规则图没有可写入的规则文件");
        }
        else {
            const bool graph_ok = write_rule_graph_json(top_file, top_graph->to_json());
            const bool rules_ok = modify_rule_file(top_file, top_graph->to_rules_json());
            saved = graph_ok && rules_ok;
        }
        wrote_any = wrote_any || saved;
        if (!saved) {
            failures.push_back(std::string());
        }
    }
    // 插件图按标识排序逐个写；某一张失败不中断其余
    for (const auto& entry : plugin_graphs) {
        bool entry_cascade = false;
        if (write_plugin_rule_graph(entry.first, entry.second, entry_cascade)) {
            wrote_any = true;
        }
        else {
            failures.push_back(entry.first);
        }
        cascade_dirty = cascade_dirty || entry_cascade;
    }
    // 全部写完后统一重建引用列表（总图与插件图互相引用时只读一次最新内容）
    if (wrote_any) {
        sync_output_references();
    }
    if (cascade_dirty) {
        emit cascade_changed();
    }
    LOG_MODULE("RuleManager", "save_rule_graphs", failures.empty() ? LOG_INFO : LOG_ERROR,
        "批量保存规则图: 写入 " << (plugin_graphs.size() + (top_graph != nullptr ? 1 : 0))
            << " 张，失败 " << failures.size() << " 张");
    return failures.empty();
}

std::optional<nlohmann::json> RuleManager::read_rule_graph_json(const std::string& filename) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const nlohmann::json graph_json = read_rule_graph_json_locked(filename);
    if (!graph_json.is_object()) {
        return std::nullopt;
    }
    return graph_json;
}

bool RuleManager::write_rule_graph_json(const std::string& filename, const nlohmann::json& graph_json) {
    if (filename.empty()) {
        return false;
    }
    bool saved = false;
    bool cascade_dirty = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        nlohmann::json json;
        try {
            json = load_json_file(filename);
        }
        catch (const std::exception&) {
            // 文件不存在：新建
        }
        if (!json.is_object()) {
            json = nlohmann::json::object();
        }
        if (!json.contains("version")) {
            json["version"] = "1.0";
        }
        json["graph"] = graph_json;
        saved = save_json_file(filename, json);
        if (saved) {
            scan_directory();
            // 图已变化：按新图重算已启用来源的依赖集合
            cascade_dirty = refresh_cascades_locked();
        }
    }
    if (cascade_dirty) {
        emit cascade_changed();
    }
    return saved;
}

bool RuleManager::sync_output_references() {
    std::lock_guard<std::mutex> lock(mutex_);

    // 目标插件标识 -> 引用它的图标识列表
    std::map<std::string, std::vector<std::string>> referrers;
    const auto collect = [&referrers](const nlohmann::json& graph_json, const std::string& source_ref) {
        if (!graph_json.is_object() || !graph_json.contains("nodes") || !graph_json["nodes"].is_array()) {
            return;
        }
        for (const auto& item : graph_json["nodes"]) {
            if (!item.is_object() || item.value("type", "") != "pluginOutput") {
                continue;
            }
            std::string plugin_id = item.value("pluginId", "");
            // 兼容只写引用列表、未写 pluginId 的图
            if (plugin_id.empty() && item.contains("references") && item["references"].is_array()) {
                for (const auto& reference : item["references"]) {
                    if (!reference.is_string()) {
                        continue;
                    }
                    const QString referenced = rule_graph_ref_plugin(
                        QString::fromStdString(reference.get<std::string>()));
                    if (!referenced.isEmpty()) {
                        plugin_id = referenced.toStdString();
                        break;
                    }
                }
            }
            if (plugin_id.empty()) {
                continue;
            }
            auto& list = referrers[plugin_id];
            if (std::find(list.begin(), list.end(), source_ref) == list.end()) {
                list.push_back(source_ref);
            }
        }
    };

    collect(read_rule_graph_json_locked("rules.json"), rule_graph_top_ref().toStdString());
    const std::vector<std::string> plugins = plugin_graph_plugins_locked();
    for (const auto& plugin_id : plugins) {
        collect(read_rule_graph_json_locked(get_plugin_graph_file(plugin_id)),
            rule_graph_plugin_ref(QString::fromStdString(plugin_id)).toStdString());
    }

    bool ok = true;
    for (const auto& plugin_id : plugins) {
        const std::string filename = get_plugin_graph_file(plugin_id);
        nlohmann::json json;
        try {
            json = load_json_file(filename);
        }
        catch (const std::exception& e) {
            LOG_MODULE("RuleManager", "sync_output_references", LOG_WARN,
                "读取插件规则图失败: " << filename << " (" << e.what() << ")");
            ok = false;
            continue;
        }
        if (!json.contains("graph") || !json["graph"].is_object()) {
            continue;
        }
        nlohmann::json& graph = json["graph"];
        if (!graph.contains("nodes") || !graph["nodes"].is_array()) {
            continue;
        }

        // 引用列表：总规则图在前，插件图按标识排序
        bool has_top = false;
        std::vector<std::string> plugin_refs;
        const auto it = referrers.find(plugin_id);
        if (it != referrers.end()) {
            for (const auto& reference : it->second) {
                if (rule_graph_ref_is_top(QString::fromStdString(reference))) {
                    has_top = true;
                }
                else {
                    plugin_refs.push_back(reference);
                }
            }
        }
        std::sort(plugin_refs.begin(), plugin_refs.end());
        nlohmann::json references = nlohmann::json::array();
        if (has_top) {
            references.push_back(rule_graph_top_ref().toStdString());
        }
        for (const auto& reference : plugin_refs) {
            references.push_back(reference);
        }

        // 只维护首个默认输出节点（重复节点在载入时已归一）
        bool found = false;
        bool changed = false;
        for (auto& node : graph["nodes"]) {
            if (!node.is_object() || node.value("type", "") != "defaultOutput") {
                continue;
            }
            found = true;
            if (!node.contains("references") || node["references"] != references) {
                node["references"] = references;
                changed = true;
            }
            break;
        }
        if (!found) {
            LOG_MODULE("RuleManager", "sync_output_references", LOG_WARN,
                "插件规则图缺少默认输出节点: " << filename);
            continue;
        }
        if (changed && !save_json_file(filename, json)) {
            ok = false;
        }
    }
    return ok;
}

std::vector<std::string> RuleManager::get_output_referrers(const std::string& plugin_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    if (plugin_id.empty()) {
        return result;
    }
    const nlohmann::json graph_json = read_rule_graph_json_locked(get_plugin_graph_file(plugin_id));
    if (!graph_json.is_object() || !graph_json.contains("nodes") || !graph_json["nodes"].is_array()) {
        return result;
    }
    for (const auto& item : graph_json["nodes"]) {
        if (!item.is_object() || item.value("type", "") != "defaultOutput") {
            continue;
        }
        if (!item.contains("references") || !item["references"].is_array()) {
            break;
        }
        for (const auto& reference : item["references"]) {
            if (reference.is_string()) {
                result.push_back(reference.get<std::string>());
            }
        }
        break;
    }
    return result;
}

GraphDependency RuleManager::get_graph_dependencies(const RuleGraph& graph, int from_node_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return get_graph_dependencies_locked(graph, from_node_id);
}

void RuleManager::fill_dependency_events(
    const std::map<std::string, std::vector<std::string>>& plugin_events,
    GraphDependency& dependency) {
    if (dependency.plugins.empty()) {
        return;
    }
    for (const auto& plugin_id : dependency.plugins) {
        const auto it = plugin_events.find(plugin_id);
        if (it == plugin_events.end()) {
            continue;
        }
        for (const auto& event_id : it->second) {
            dependency.events.push_back(plugin_id + "/" + event_id);
        }
    }
    std::sort(dependency.events.begin(), dependency.events.end());
    dependency.events.erase(std::unique(dependency.events.begin(), dependency.events.end()),
        dependency.events.end());
}

// 启用级联（public）

bool RuleManager::set_graph_output_enabled(const std::string& graph_ref, bool enabled) {
    if (graph_ref.empty()) {
        return false;
    }
    bool changed = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const RuleGraph graph = load_rule_graph_by_ref_locked(graph_ref);
        if (graph.nodes().empty()) {
            LOG_MODULE("RuleManager", "set_graph_output_enabled", LOG_WARN,
                "规则图不存在: " << graph_ref);
            return false;
        }
        apply_cascade_locked("graph:" + graph_ref, graph, 0, enabled, changed);
        if (changed) {
            rebuild_cascade_locked();
        }
    }
    LOG_MODULE("RuleManager", "set_graph_output_enabled", LOG_INFO,
        "规则图输出 " << graph_ref << " 启用状态: " << (enabled ? "启用" : "关闭"));
    if (changed && enabled) {
        // 启用时立即刷新一帧（后续周期照旧）
        evaluate_graph_outputs();
    }
    if (changed) {
        emit cascade_changed();
    }
    return true;
}

bool RuleManager::is_graph_output_enabled(const std::string& graph_ref) const {
    if (graph_ref.empty()) {
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    return active_cascades_.find("graph:" + graph_ref) != active_cascades_.end();
}

std::vector<std::string> RuleManager::get_cascade_enabled_rules() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::vector<std::string>(cascade_rules_.begin(), cascade_rules_.end());
}

std::vector<std::string> RuleManager::get_cascade_enabled_events() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::vector<std::string>(cascade_events_.begin(), cascade_events_.end());
}

// 多图求值（public）

void RuleManager::evaluate_graph_outputs() {
    std::vector<QJsonObject> pending_commands;
    std::vector<ResultEvent> pending_results;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        evaluate_graphs_locked(pending_commands, pending_results);
    }
    for (const auto& cmd : pending_commands) {
        emit rule_command_ready(cmd);
    }
    for (const auto& [rule_name, ch, value] : pending_results) {
        emit rule_result_changed(QString::fromStdString(rule_name),
            QString::fromStdString(ch), value);
    }
}

std::optional<int> RuleManager::get_plugin_graph_output(const std::string& plugin_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = plugin_graph_outputs_.find(plugin_id);
    if (it == plugin_graph_outputs_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<int> RuleManager::get_graph_channel_output(const std::string& channel) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = channel_graph_outputs_.find(Rule::normalize_channel(channel));
    if (it == channel_graph_outputs_.end()) {
        return std::nullopt;
    }
    return it->second;
}

// 规则加载（从配置管理器）（public）

void RuleManager::load_rules(std::shared_ptr<ConfigManager> config_manager) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!config_manager) {
        LOG_MODULE("RuleManager", "load_rules", LOG_ERROR, "ConfigManager 为空");
        return;
    }
    config_manager_ = config_manager;
    reload_rules();
}

void RuleManager::reload_rules() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!config_manager_) {
        LOG_MODULE("RuleManager", "reload_rules", LOG_WARN, "ConfigManager 未初始化");
        return;
    }
    auto rules_json = config_manager_->get<nlohmann::json>("rules");
    if (!rules_json.has_value()) {
        LOG_MODULE("RuleManager", "reload_rules", LOG_WARN, "配置中没有 'rules' 字段");
        return;
    }
    parse_config(rules_json.value());
}

// 规则查询（public）

std::vector<std::string> RuleManager::get_rule_names() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> names;
    names.reserve(index_to_name_.size());
    for (const auto& [index, name] : index_to_name_) {
        names.push_back(name);
    }
    return names;
}

std::string RuleManager::get_rule_display_string(const std::string& rule_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = rules_.find(rule_name);
    return it != rules_.end() ? it->second.get_display_string() : "";
}

std::vector<std::string> RuleManager::get_all_rule_display_strings() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    result.reserve(rules_.size());
    for (const auto& [index, name] : index_to_name_) {
        auto it = rules_.find(name);
        if (it != rules_.end()) {
            result.push_back(it->second.get_display_string());
        }
    }
    return result;
}

std::string RuleManager::get_rule_channel(const std::string& rule_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = rules_.find(rule_name);
    if (it == rules_.end()) {
        return "";
    }
    // 返回首个通道父级（兼容旧 channel 字段语义）
    for (const auto& parent : it->second.get_parents()) {
        if (parent.type == ParentType::CHANNEL) {
            return parent.channel;
        }
    }
    return "";
}

int RuleManager::get_rule_mode(const std::string& rule_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = rules_.find(rule_name);
    return it != rules_.end() ? it->second.get_mode() : -1;
}

std::string RuleManager::get_rule_value_pattern(const std::string& rule_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = rules_.find(rule_name);
    return it != rules_.end() ? it->second.get_value_pattern() : "";
}

int RuleManager::get_rule_index(const std::string& rule_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = rules_.find(rule_name);
    return it != rules_.end() ? it->second.get_index() : -1;
}

std::string RuleManager::get_rule_name_by_index(int rule_index) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = index_to_name_.find(rule_index);
    return it != index_to_name_.end() ? it->second : "";
}

bool RuleManager::get_rule_enabled(const std::string& rule_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = rules_.find(rule_name);
    return it != rules_.end() && it->second.get_enabled();
}

bool RuleManager::is_rule_effectively_enabled(const std::string& rule_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> visiting;
    return is_rule_effectively_enabled_locked(rule_name, visiting);
}

std::vector<RuleParent> RuleManager::get_rule_parents(const std::string& rule_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = rules_.find(rule_name);
    return it != rules_.end() ? it->second.get_parents() : std::vector<RuleParent>();
}

std::string RuleManager::get_rule_parents_display(const std::string& rule_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = rules_.find(rule_name);
    if (it == rules_.end()) {
        return "无";
    }
    const Rule& rule = it->second;
    // 通道父级（声明）
    std::vector<std::string> channel_parts;
    for (const auto& parent : rule.get_parents()) {
        if (parent.type == ParentType::CHANNEL) {
            channel_parts.push_back(parent.channel);
        }
    }
    // 规则父级（由值模式 {rule:xx} 推导）
    std::vector<std::string> rule_parts;
    auto ref_it = referrers_.find(rule.get_index());
    if (ref_it != referrers_.end()) {
        for (int ref_index : ref_it->second) {
            rule_parts.push_back("rule:" + std::to_string(ref_index));
        }
    }
    // 组装显示文本：通道在前，规则在后
    std::string result;
    if (channel_parts.empty() && rule_parts.empty()) {
        return "无";
    }
    for (const auto& part : channel_parts) {
        if (!result.empty()) result += ";";
        result += part;
    }
    if (!rule_parts.empty()) {
        if (!result.empty()) result += ";";
        // 超出两个规则显示省略号
        const size_t max_rules = 2;
        if (rule_parts.size() > max_rules) {
            for (size_t i = 0; i < max_rules; ++i) {
                if (i > 0) result += ",";
                result += rule_parts[i];
            }
            result += ",...";
        }
        else {
            for (size_t i = 0; i < rule_parts.size(); ++i) {
                if (i > 0) result += ",";
                result += rule_parts[i];
            }
        }
    }
    return result;
}

int RuleManager::get_rule_mode_applicability(const std::string& rule_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = rules_.find(rule_name);
    if (it == rules_.end()) {
        return 1;
    }
    const Rule& rule = it->second;
    // 统计通道父级数量
    size_t channel_count = 0;
    for (const auto& parent : rule.get_parents()) {
        if (parent.type == ParentType::CHANNEL) {
            ++channel_count;
        }
    }
    // 统计规则父级数量（由值模式 {rule:xx} 推导）
    size_t rule_count = 0;
    auto ref_it = referrers_.find(rule.get_index());
    if (ref_it != referrers_.end()) {
        rule_count = ref_it->second.size();
    }
    if (channel_count > 0 && rule_count == 0) {
        return 0;
    }
    if (channel_count > 0 && rule_count > 0) {
        return 2;
    }
    return 1;
}

std::optional<int> RuleManager::get_rule_last_result(const std::string& rule_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = rules_.find(rule_name);
    if (it == rules_.end()) {
        return std::nullopt;
    }
    auto lit = last_results_.find(it->second.get_index());
    return lit != last_results_.end() ? lit->second : std::nullopt;
}

// 规则修改（public）

void RuleManager::set_rule_enabled(const std::string& rule_name, bool enabled) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = rules_.find(rule_name);
        if (it == rules_.end()) {
            return;
        }
        it->second.set_enabled(enabled);
    }
    emit rules_changed();
}

void RuleManager::set_rule_channel(const std::string& rule_name, const std::string& channel) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string ch = Rule::normalize_channel(channel);
        auto it = rules_.find(rule_name);
        if (it == rules_.end()) {
            return;
        }
        // 收集新的父级列表（保留非通道父级，替换通道父级）
        std::vector<RuleParent> new_parents;
        for (const auto& parent : it->second.get_parents()) {
            if (parent.type != ParentType::CHANNEL) {
                new_parents.push_back(parent);
            }
        }
        if (!ch.empty()) {
            RuleParent parent;
            parent.type = ParentType::CHANNEL;
            parent.channel = ch;
            new_parents.push_back(parent);
        }
        it->second.set_parents(new_parents);
        // 通道唯一性：保留最后设置的规则，其余声明同通道的规则父级置空
        deduplicate_channel_parents_keep(rule_name, ch);
    }
    emit rules_changed();
}

void RuleManager::set_rule_value_pattern(const std::string& rule_name,
    const std::string& pattern) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = rules_.find(rule_name);
        if (it == rules_.end()) {
            return;
        }
        // 重建规则对象（保留序号、启用、父级等字段），并刷新引用索引
        Rule new_rule(rule_name, it->second.get_channel(), it->second.get_mode(), pattern,
            it->second.get_enabled(), it->second.get_parents(), it->second.get_index());
        it->second = std::move(new_rule);
        rebuild_indexes();
    }
    emit rules_changed();
    LOG_MODULE("RuleManager", "set_rule_value_pattern", LOG_DEBUG,
        "规则 " << rule_name << " 值模式已更新: " << pattern);
}

std::vector<int> RuleManager::get_rule_parent_rules(const std::string& rule_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = rules_.find(rule_name);
    if (it == rules_.end()) {
        return {};
    }
    // 规则父级由值模式 {rule:xx} 推导（referrers 反向索引）
    auto ref_it = referrers_.find(it->second.get_index());
    return ref_it != referrers_.end() ? ref_it->second : std::vector<int>();
}

bool RuleManager::add_rule_reference(const std::string& rule_name, int referenced_index) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = rules_.find(rule_name);
        if (it == rules_.end() || referenced_index <= 0) {
            return false;
        }
        std::string pattern = it->second.get_value_pattern();
        std::string token = "{rule:" + std::to_string(referenced_index) + "}";
        if (pattern.find(token) != std::string::npos) {
            return false;
        }
        // 追加规则引用（空模式直接放置，否则用 + 连接）
        if (pattern.empty()) {
            pattern = token;
        }
        else {
            pattern += "+" + token;
        }
        // 重建规则对象（保留其他字段），并刷新索引
        Rule new_rule(rule_name, it->second.get_channel(), it->second.get_mode(), pattern,
            it->second.get_enabled(), it->second.get_parents(), it->second.get_index());
        it->second = std::move(new_rule);
        rebuild_indexes();
    }
    emit rules_changed();
    return true;
}

bool RuleManager::remove_rule_reference(const std::string& rule_name, int referenced_index) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = rules_.find(rule_name);
        if (it == rules_.end() || referenced_index <= 0) {
            return false;
        }
        std::string pattern = it->second.get_value_pattern();
        std::string token = "{rule:" + std::to_string(referenced_index) + "}";
        if (pattern.find(token) == std::string::npos) {
            return false;
        }
        // 移除带前导运算符的引用（如 +{rule:2}），再移除孤立引用
        QString qpattern = QString::fromStdString(pattern);
        QString idx = QString::number(referenced_index);
        qpattern.remove(QRegularExpression("\\+\\{rule:" + idx + "\\}"));
        qpattern.remove(QRegularExpression("\\{rule:" + idx + "\\}"));
        // 清理因移除引用产生的孤立运算符（开头/结尾）
        std::string cleaned = qpattern.toStdString();
        while (!cleaned.empty()
            && (cleaned.front() == '+' || cleaned.front() == '-'
                || cleaned.front() == '*' || cleaned.front() == '/')) {
            cleaned.erase(cleaned.begin());
        }
        while (!cleaned.empty()
            && (cleaned.back() == '+' || cleaned.back() == '-'
                || cleaned.back() == '*' || cleaned.back() == '/')) {
            cleaned.pop_back();
        }
        // 重建规则对象（保留其他字段），并刷新索引
        Rule new_rule(rule_name, it->second.get_channel(), it->second.get_mode(), cleaned,
            it->second.get_enabled(), it->second.get_parents(), it->second.get_index());
        it->second = std::move(new_rule);
        rebuild_indexes();
    }
    emit rules_changed();
    return true;
}

// 通道启用状态（public）

void RuleManager::set_channel_enabled(const std::string& channel, bool enabled) {
    std::vector<QJsonObject> pending_commands;
    std::vector<ResultEvent> pending_results;
    bool cascade_dirty = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string ch = Rule::normalize_channel(channel);
        if (ch.empty()) {
            return;
        }
        channel_enabled_[ch] = enabled;
        LOG_MODULE("RuleManager", "set_channel_enabled", LOG_INFO,
            "通道 " << ch << " 启用状态: " << (enabled ? "启用" : "关闭"));
        if (enabled) {
            // 通道启用时触发直连该通道的规则计算（整条调用链开始运转）
            std::vector<std::string> names;
            for (const auto& [name, rule] : rules_) {
                if (rule.has_parent_channel(ch)) {
                    names.push_back(name);
                }
            }
            for (const auto& name : names) {
                compute_rule_locked(name, pending_commands, pending_results);
            }
        }
        // 启用级联：通道节点的上游依赖（子规则与事件）一并视为启用
        const RuleGraph top = load_top_rule_graph_locked();
        const int channel_node = channel_node_of(top, ch);
        if (channel_node > 0) {
            apply_cascade_locked("channel:" + ch, top, channel_node, enabled, cascade_dirty);
        }
        else {
            // 总规则图里没有该通道节点：不参与级联，行为与未接入级联前一致
            apply_cascade_locked("channel:" + ch, top, 0, false, cascade_dirty);
        }
        if (cascade_dirty) {
            rebuild_cascade_locked();
        }
        // 多图求值：启用时立即产出一帧（与直连规则计算同一时机）
        if (enabled) {
            evaluate_graphs_locked(pending_commands, pending_results);
        }
    }
    for (const auto& cmd : pending_commands) {
        emit rule_command_ready(cmd);
    }
    for (const auto& [rule_name, ch, value] : pending_results) {
        emit rule_result_changed(QString::fromStdString(rule_name),
            QString::fromStdString(ch), value);
    }
    if (cascade_dirty) {
        emit cascade_changed();
    }
}

bool RuleManager::get_channel_enabled(const std::string& channel) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = channel_enabled_.find(Rule::normalize_channel(channel));
    return it != channel_enabled_.end() && it->second;
}

// 计算（public）

std::optional<int> RuleManager::compute_rule(const std::string& rule_name) {
    std::vector<QJsonObject> pending_commands;
    std::vector<ResultEvent> pending_results;
    std::optional<int> result;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        result = compute_rule_locked(rule_name, pending_commands, pending_results);
    }
    // 解锁后统一发送通道命令与结果事件，避免持锁调用外部槽
    for (const auto& cmd : pending_commands) {
        emit rule_command_ready(cmd);
    }
    for (const auto& [rule_name2, ch, value] : pending_results) {
        emit rule_result_changed(QString::fromStdString(rule_name2),
            QString::fromStdString(ch), value);
    }
    return result;
}

std::optional<int> RuleManager::trigger_rule(const std::string& rule_name) {
    std::vector<QJsonObject> pending_commands;
    std::vector<ResultEvent> pending_results;
    std::optional<int> result;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // 手动触发：跳过启用检查直接计算
        auto it = rules_.find(rule_name);
        if (it == rules_.end()) {
            return std::nullopt;
        }
        if (compute_depth_ >= MAX_COMPUTE_DEPTH) {
            LOG_MODULE("RuleManager", "trigger_rule", LOG_WARN,
                "级联计算深度超限，终止: " << rule_name);
            return std::nullopt;
        }
        ++compute_depth_;
        std::vector<std::optional<int>> values;
        values.reserve(it->second.get_placeholders().size());
        for (const auto& ph : it->second.get_placeholders()) {
            values.push_back(resolve_placeholder_locked(ph, pending_commands, pending_results));
        }
        result = it->second.compute_value(values);
        --compute_depth_;
        if (result.has_value()) {
            last_results_[it->second.get_index()] = result.value();
        }
    }
    for (const auto& cmd : pending_commands) {
        emit rule_command_ready(cmd);
    }
    for (const auto& [rule_name2, ch, value] : pending_results) {
        emit rule_result_changed(QString::fromStdString(rule_name2),
            QString::fromStdString(ch), value);
    }
    return result;
}

// 辅助（JSON 文件读写）（public）

nlohmann::json RuleManager::load_json_file(const std::string& filename) const {
    std::string full_path = get_full_path(filename);
    std::ifstream file(full_path);
    if (!file.is_open()) {
        throw std::runtime_error("无法打开文件: " + full_path);
    }
    nlohmann::json j;
    file >> j;
    return j;
}

// private slots 实现

void RuleManager::on_module_value_changed(const QString& module_name, const QString& value_id,
    int new_value) {
    // 模块数值变化 → 触发值模式中引用该数值的规则计算
    std::vector<QJsonObject> pending_commands;
    std::vector<ResultEvent> pending_results;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = id_users_.find(value_id.toStdString());
        if (it != id_users_.end()) {
            std::vector<std::string> names;
            for (int idx : it->second) {
                auto nit = index_to_name_.find(idx);
                if (nit != index_to_name_.end()) {
                    names.push_back(nit->second);
                }
            }
            for (const auto& name : names) {
                std::vector<std::string> visiting;
                // 仅启用且父级可用的规则参与计算
                if (is_rule_effectively_enabled_locked(name, visiting)) {
                    compute_rule_locked(name, pending_commands, pending_results);
                }
            }
        }
        // 多图求值沿用同一周期：模块数值变化即重算插件图与总规则图
        evaluate_graphs_locked(pending_commands, pending_results);
    }
    for (const auto& cmd : pending_commands) {
        emit rule_command_ready(cmd);
    }
    for (const auto& [rule_name2, ch, value] : pending_results) {
        emit rule_result_changed(QString::fromStdString(rule_name2),
            QString::fromStdString(ch), value);
    }
}

// 私有辅助函数实现（private）

std::vector<std::string> RuleManager::plugin_graph_plugins_locked() const {
    std::vector<std::string> plugins;
    if (!fs::exists(rules_dir_)) {
        return plugins;
    }
    const std::string prefix = "plugin_";
    const std::string suffix = ".json";
    for (const auto& entry : fs::directory_iterator(rules_dir_)) {
        if (!entry.is_regular_file() || entry.path().extension() != suffix) {
            continue;
        }
        const std::string filename = entry.path().filename().string();
        if (filename.rfind(prefix, 0) != 0) {
            continue;
        }
        if (filename.size() <= prefix.size() + suffix.size()) {
            continue;
        }
        plugins.push_back(filename.substr(prefix.size(),
            filename.size() - prefix.size() - suffix.size()));
    }
    std::sort(plugins.begin(), plugins.end());
    return plugins;
}

RuleGraph RuleManager::load_plugin_rule_graph_locked(const std::string& plugin_id) const {
    const nlohmann::json graph_json = read_rule_graph_json_locked(get_plugin_graph_file(plugin_id));
    if (graph_json.is_object()) {
        RuleGraph graph;
        graph.from_json(graph_json);
        graph.set_plugin_id(QString::fromStdString(plugin_id));
        graph.ensure_default_output(QString::fromStdString(plugin_id));
        return graph;
    }
    // 没有图文件：按插件已登记的数值生成默认图，保存时才落盘
    RuleGraph graph;
    graph.import_from_plugin(QString::fromStdString(plugin_id));
    return graph;
}

RuleGraph RuleManager::load_top_rule_graph_locked() const {
    const nlohmann::json graph_json = read_rule_graph_json_locked("rules.json");
    RuleGraph graph;
    if (graph_json.is_object()) {
        graph.from_json(graph_json);
    }
    return graph;
}

RuleGraph RuleManager::load_rule_graph_by_ref_locked(const std::string& graph_ref) const {
    const QString ref = QString::fromStdString(graph_ref);
    if (rule_graph_ref_is_top(ref)) {
        return load_top_rule_graph_locked();
    }
    const QString plugin_id = rule_graph_ref_plugin(ref);
    if (plugin_id.isEmpty()) {
        return RuleGraph();
    }
    return load_plugin_rule_graph_locked(plugin_id.toStdString());
}

GraphDependency RuleManager::get_graph_dependencies_locked(const RuleGraph& graph,
    int from_node_id) const {
    GraphDependency dependency = graph.dependencies(from_node_id);
    // 总规则图：插件输出节点代表该插件图的最终输出，把插件图的依赖并入（只展开一层，避免环形引用）
    if (graph.plugin_id().isEmpty() && !dependency.plugins.empty()) {
        const std::vector<std::string> plugins = dependency.plugins;
        for (const auto& plugin_id : plugins) {
            const GraphDependency sub = load_plugin_rule_graph_locked(plugin_id).dependencies(0);
            merge_unique(dependency.rules, sub.rules);
            merge_unique(dependency.value_ids, sub.value_ids);
            merge_unique(dependency.channels, sub.channels);
            merge_unique(dependency.plugins, sub.plugins);
        }
    }
    // 事件来自依赖插件的侧车声明（"<插件标识>/<事件 ID>"）
    std::map<std::string, std::vector<std::string>> plugin_events;
    for (const auto& plugin : ModuleManager::instance().get_plugins()) {
        if (plugin.module_name.empty()) {
            continue;
        }
        std::vector<std::string> event_ids;
        event_ids.reserve(plugin.events.size());
        for (const auto& event : plugin.events) {
            event_ids.push_back(event.id);
        }
        plugin_events[plugin.module_name] = std::move(event_ids);
    }
    fill_dependency_events(plugin_events, dependency);
    return dependency;
}

void RuleManager::apply_cascade_locked(const std::string& key, const RuleGraph& graph,
    int from_node_id, bool enabled, bool& changed) {
    if (!enabled) {
        if (active_cascades_.erase(key) > 0) {
            changed = true;
        }
        return;
    }
    GraphDependency dependency = get_graph_dependencies_locked(graph, from_node_id);
    const auto it = active_cascades_.find(key);
    if (it != active_cascades_.end() && it->second.rules == dependency.rules
        && it->second.events == dependency.events) {
        return;
    }
    // 依赖集合为空时同样登记：级联集合不增加，行为与未启用一致
    active_cascades_[key] = std::move(dependency);
    changed = true;
}

void RuleManager::rebuild_cascade_locked() {
    cascade_rules_.clear();
    cascade_events_.clear();
    for (const auto& [key, dependency] : active_cascades_) {
        cascade_rules_.insert(dependency.rules.begin(), dependency.rules.end());
        cascade_events_.insert(dependency.events.begin(), dependency.events.end());
    }
}

bool RuleManager::refresh_cascades_locked() {
    if (active_cascades_.empty()) {
        return false;
    }
    static const std::string CHANNEL_PREFIX = "channel:";
    static const std::string GRAPH_PREFIX = "graph:";
    bool changed = false;
    std::vector<std::string> keys;
    keys.reserve(active_cascades_.size());
    for (const auto& [key, dependency] : active_cascades_) {
        keys.push_back(key);
    }
    for (const auto& key : keys) {
        if (key.rfind(CHANNEL_PREFIX, 0) == 0) {
            const std::string channel = key.substr(CHANNEL_PREFIX.size());
            const RuleGraph top = load_top_rule_graph_locked();
            const int channel_node = channel_node_of(top, channel);
            if (channel_node <= 0) {
                changed = active_cascades_.erase(key) > 0 ? true : changed;
                continue;
            }
            apply_cascade_locked(key, top, channel_node, true, changed);
            continue;
        }
        if (key.rfind(GRAPH_PREFIX, 0) == 0) {
            const std::string graph_ref = key.substr(GRAPH_PREFIX.size());
            const RuleGraph graph = load_rule_graph_by_ref_locked(graph_ref);
            if (graph.nodes().empty()) {
                changed = active_cascades_.erase(key) > 0 ? true : changed;
                continue;
            }
            apply_cascade_locked(key, graph, 0, true, changed);
        }
    }
    if (changed) {
        rebuild_cascade_locked();
    }
    return changed;
}

void RuleManager::evaluate_graphs_locked(std::vector<QJsonObject>& pending_commands,
    std::vector<ResultEvent>& pending_results) {
    // 模块数值取值：沿用规则占位符的口径（无数据源的数值视为空值）
    const RuleGraph::ValueResolver resolve = [](const QString& value_id) -> std::optional<int> {
        ModuleManager& modules = ModuleManager::instance();
        const std::string module_name = modules.find_module_by_value_id(value_id.toStdString());
        if (module_name.empty()) {
            return std::nullopt;
        }
        const ModuleValue* value = modules.get_value(module_name, value_id.toStdString());
        if (value == nullptr || !value->get_has_value()) {
            return std::nullopt;
        }
        return value->get_last_value();
    };

    // 1) 先插件图：以每个插件图默认输出节点的上游子图求值
    plugin_graph_outputs_.clear();
    std::map<QString, int> plugin_outputs;
    for (const auto& plugin_id : plugin_graph_plugins_locked()) {
        const RuleGraph graph = load_plugin_rule_graph_locked(plugin_id);
        const std::optional<int> value = graph.evaluate(graph.default_output_node(), resolve, {});
        if (!value.has_value()) {
            continue;
        }
        plugin_graph_outputs_[plugin_id] = value.value();
        plugin_outputs[QString::fromStdString(plugin_id)] = value.value();
    }

    // 2) 后总规则图：插件输出节点取上一步的值，通道节点据此得到强度
    channel_graph_outputs_.clear();
    const RuleGraph top = load_top_rule_graph_locked();
    for (const auto& node : top.nodes()) {
        if (node.type != GraphNodeType::Channel) {
            continue;
        }
        const std::string channel = Rule::normalize_channel(node.channel.toStdString());
        if (channel.empty()) {
            continue;
        }
        const std::optional<int> value = top.evaluate(node.id, resolve, plugin_outputs);
        if (!value.has_value()) {
            continue;
        }
        channel_graph_outputs_[channel] = value.value();
        const auto enabled_it = channel_enabled_.find(channel);
        if (enabled_it == channel_enabled_.end() || !enabled_it->second) {
            // 通道未启用：只记录求值结果，不下发强度
            continue;
        }
        QJsonObject cmd;
        cmd["cmd"] = "send_strength";
        cmd["channel"] = QString::fromStdString(channel);
        cmd["mode"] = 2; // 通道节点给出的是绝对强度（设为）
        cmd["value"] = value.value();
        // 目标设备随命令带出：空串 = 跟随默认设备，非空 = 指定设备 MAC。
        // 设备解析与「离线不输出」的判定在 DeviceController（设备名单的唯一持有者）完成
        cmd["device"] = node.target_device;
        pending_commands.push_back(cmd);
        pending_results.emplace_back(node.name.toStdString(), channel, value.value());
    }
}

nlohmann::json RuleManager::read_rule_graph_json_locked(const std::string& filename) const {
    try {
        const nlohmann::json json = load_json_file(filename);
        if (json.is_object() && json.contains("graph") && json["graph"].is_object()) {
            return json["graph"];
        }
    }
    catch (const std::exception&) {
        // 文件不存在或无图字段：由调用方退化处理
    }
    return nlohmann::json();
}

void RuleManager::scan_directory() {
    available_files_.clear();
    if (!fs::exists(rules_dir_)) return;
    const std::string sidecar_suffix = ".graph.json";
    for (const auto& entry : fs::directory_iterator(rules_dir_)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
        const std::string filename = entry.path().filename().string();
        if (filename == "rules.json") continue;
        // 插件规则图与旧版侧车图不是规则文件，不进入规则文件列表
        if (filename.rfind("plugin_", 0) == 0) continue;
        if (filename.size() >= sidecar_suffix.size()
            && filename.compare(filename.size() - sidecar_suffix.size(), sidecar_suffix.size(), sidecar_suffix) == 0) {
            continue;
        }
        if (filename.find(keyword_) != std::string::npos) {
            available_files_.push_back(filename);
        }
    }
    std::sort(available_files_.begin(), available_files_.end());
}

std::string RuleManager::get_full_path(const std::string& filename) const {
    return rules_dir_ + "/" + filename;
}

bool RuleManager::save_json_file(const std::string& filename, const nlohmann::json& content) const {
    std::string full_path = get_full_path(filename);
    std::ofstream file(full_path);
    if (!file.is_open()) return false;
    file << content.dump(4);
    return true;
}

void RuleManager::parse_config(const nlohmann::json& config) {
    rules_.clear();
    int index = 1;
    for (auto& [key, value] : config.items()) {
        try {
            std::string channel = "";
            int mode = 0;
            std::string value_pattern;
            bool enabled = true;
            std::vector<RuleParent> parents;

            if (value.is_string()) {
                channel = "";
                mode = 1;
                value_pattern = value.get<std::string>();
            }
            else if (value.is_object()) {
                channel = value.value("channel", "");
                mode = value.value("mode", 1);
                value_pattern = value.value("valuePattern", "");
                enabled = value.value("enabled", true);
                // 解析父级数组（"A"/"B" 字符串或规则序号整数）
                if (value.contains("parents") && value["parents"].is_array()) {
                    for (const auto& p : value["parents"]) {
                        if (p.is_string()) {
                            std::string ch = Rule::normalize_channel(p.get<std::string>());
                            if (!ch.empty()) {
                                RuleParent parent;
                                parent.type = ParentType::CHANNEL;
                                parent.channel = ch;
                                parents.push_back(parent);
                            }
                        }
                        else if (p.is_number_integer()) {
                            RuleParent parent;
                            parent.type = ParentType::RULE;
                            parent.rule_index = p.get<int>();
                            parents.push_back(parent);
                        }
                    }
                }
                if (value_pattern.empty()) {
                    LOG_MODULE("RuleManager", "parse_config", LOG_WARN,
                        "规则 " << key << " 缺少 valuePattern，已跳过");
                    continue;
                }
            }
            else {
                LOG_MODULE("RuleManager", "parse_config", LOG_WARN,
                    "规则 " << key << " 格式无效，已跳过");
                continue;
            }

            rules_.emplace(key, Rule(key, channel, mode, value_pattern, enabled, parents, index));
            ++index;
            LOG_MODULE("RuleManager", "parse_config", LOG_DEBUG,
                "加载规则: " << key << " [#" << (index - 1)
                             << ", enabled=" << enabled << ", pattern=" << value_pattern << "]");
        }
        catch (const std::exception& e) {
            LOG_MODULE("RuleManager", "parse_config", LOG_ERROR,
                "解析规则 " << key << " 失败: " << e.what());
        }
    }
    // 重建索引后执行通道父级唯一性去重
    rebuild_indexes();
    deduplicate_channel_parents();
    emit rules_changed();
}

void RuleManager::rebuild_indexes() {
    index_to_name_.clear();
    referrers_.clear();
    id_users_.clear();
    last_results_.clear();
    for (const auto& [name, rule] : rules_) {
        index_to_name_[rule.get_index()] = name;
        for (const auto& ph : rule.get_placeholders()) {
            if (ph.type == PlaceholderType::RULE_REF && ph.rule_index > 0) {
                referrers_[ph.rule_index].push_back(rule.get_index());
            }
            else if (ph.type == PlaceholderType::ID_REF && !ph.id.empty()) {
                id_users_[ph.id].push_back(rule.get_index());
            }
        }
    }
    // 排序引用列表，保证级联触发顺序稳定
    for (auto& [key, list] : referrers_) {
        std::sort(list.begin(), list.end());
    }
    for (auto& [key, list] : id_users_) {
        std::sort(list.begin(), list.end());
    }
}

void RuleManager::deduplicate_channel_parents() {
    // 统计每个通道被声明的最小规则序号
    std::map<std::string, int> channel_owner;
    for (const auto& [name, rule] : rules_) {
        for (const auto& parent : rule.get_parents()) {
            if (parent.type == ParentType::CHANNEL) {
                auto it = channel_owner.find(parent.channel);
                if (it == channel_owner.end() || rule.get_index() < it->second) {
                    channel_owner[parent.channel] = rule.get_index();
                }
            }
        }
    }
    // 除序号最小的规则外，其余规则的同通道父级移除（父级清空则视为"无"）
    for (auto& [name, rule] : rules_) {
        std::vector<RuleParent> kept;
        bool changed = false;
        for (const auto& parent : rule.get_parents()) {
            if (parent.type == ParentType::CHANNEL) {
                auto it = channel_owner.find(parent.channel);
                if (it != channel_owner.end() && it->second == rule.get_index()) {
                    kept.push_back(parent);
                }
                else {
                    changed = true;
                    LOG_MODULE("RuleManager", "deduplicate_channel_parents", LOG_WARN,
                        "规则 " << name << " 的通道父级 " << parent.channel
                                << " 与序号更小的规则冲突，已置为无");
                }
            }
            else {
                kept.push_back(parent);
            }
        }
        if (changed) {
            rule.set_parents(kept);
        }
    }
}

void RuleManager::deduplicate_channel_parents_keep(const std::string& keep_name,
    const std::string& channel) {
    // 手动设置通道时：保留最后设置的规则，其余声明同通道的规则父级移除
    if (channel.empty()) {
        return;
    }
    for (auto& [name, rule] : rules_) {
        if (name == keep_name) {
            continue;
        }
        if (!rule.has_parent_channel(channel)) {
            continue;
        }
        std::vector<RuleParent> kept;
        for (const auto& parent : rule.get_parents()) {
            if (parent.type == ParentType::CHANNEL && parent.channel == channel) {
                continue;
            }
            kept.push_back(parent);
        }
        rule.set_parents(kept);
        LOG_MODULE("RuleManager", "deduplicate_channel_parents_keep", LOG_WARN,
            "规则 " << name << " 的通道父级 " << channel << " 被清除（保留 " << keep_name << "）");
    }
}

bool RuleManager::is_rule_effectively_enabled_locked(const std::string& rule_name,
    std::vector<std::string>& visiting) const {
    // 启用级联：依赖集合中的子规则视为启用
    if (cascade_rules_.count(rule_name) != 0) {
        return true;
    }
    auto it = rules_.find(rule_name);
    if (it == rules_.end()) {
        return false;
    }
    const Rule& rule = it->second;
    if (!rule.get_enabled()) {
        return false;
    }
    // 防循环引用
    if (std::find(visiting.begin(), visiting.end(), rule_name) != visiting.end()) {
        return false;
    }
    visiting.push_back(rule_name);

    // 父级 = 通道父级（声明） + 规则父级（值模式引用推导）
    const auto& parents = rule.get_parents();
    auto ref_it = referrers_.find(rule.get_index());
    bool has_parent = !parents.empty() || (ref_it != referrers_.end() && !ref_it->second.empty());
    if (!has_parent) {
        visiting.pop_back();
        return false;
    }
    // 任一通道父级启用 → 启用
    for (const auto& parent : parents) {
        if (parent.type == ParentType::CHANNEL) {
            auto cit = channel_enabled_.find(parent.channel);
            if (cit != channel_enabled_.end() && cit->second) {
                visiting.pop_back();
                return true;
            }
        }
    }
    // 任一规则父级启用 → 启用（递归判定）
    if (ref_it != referrers_.end()) {
        for (int ref_index : ref_it->second) {
            auto nit = index_to_name_.find(ref_index);
            if (nit != index_to_name_.end()
                && is_rule_effectively_enabled_locked(nit->second, visiting)) {
                visiting.pop_back();
                return true;
            }
        }
    }
    visiting.pop_back();
    return false;
}

std::optional<int> RuleManager::compute_rule_locked(const std::string& rule_name,
    std::vector<QJsonObject>& pending_commands, std::vector<ResultEvent>& pending_results) {
    auto it = rules_.find(rule_name);
    if (it == rules_.end()) {
        return std::nullopt;
    }
    Rule& rule = it->second;

    // 有效启用检查（父级可用才参与计算）
    std::vector<std::string> visiting;
    if (!is_rule_effectively_enabled_locked(rule_name, visiting)) {
        return std::nullopt;
    }
    // 级联深度保护（防循环引用导致无限递归）
    if (compute_depth_ >= MAX_COMPUTE_DEPTH) {
        LOG_MODULE("RuleManager", "compute_rule_locked", LOG_WARN,
            "级联计算深度超限，终止: " << rule_name);
        return std::nullopt;
    }

    ++compute_depth_;
    std::vector<std::optional<int>> values;
    values.reserve(rule.get_placeholders().size());
    for (const auto& ph : rule.get_placeholders()) {
        values.push_back(resolve_placeholder_locked(ph, pending_commands, pending_results));
    }
    std::optional<int> result = rule.compute_value(values);
    --compute_depth_;

    if (!result.has_value()) {
        // 存在空值占位符：本次计算被忽略（不推送、不发送）
        return std::nullopt;
    }

    // 缓存计算结果
    last_results_[rule.get_index()] = result.value();

    // 级联推送：结果推送给所有引用本规则的规则（其父级包含本规则）
    auto ref_it = referrers_.find(rule.get_index());
    if (ref_it != referrers_.end()) {
        for (int ref_index : ref_it->second) {
            auto nit = index_to_name_.find(ref_index);
            if (nit == index_to_name_.end()) {
                continue;
            }
            std::vector<std::string> v2;
            // 接收推送的父级若启用且为规则则同样触发计算
            if (is_rule_effectively_enabled_locked(nit->second, v2)) {
                compute_rule_locked(nit->second, pending_commands, pending_results);
            }
        }
    }

    // 通道父级：生成通道指令并交由上层下发
    for (const auto& parent : rule.get_parents()) {
        if (parent.type != ParentType::CHANNEL) {
            continue;
        }
        QJsonObject cmd;
        cmd["cmd"] = "send_strength";
        cmd["channel"] = QString::fromStdString(parent.channel);
        cmd["mode"] = rule.get_mode();
        cmd["value"] = result.value();
        pending_commands.push_back(cmd);
        // 记录结果事件（首页通道规则卡片刷新用）
        pending_results.emplace_back(rule_name, parent.channel, result.value());
    }
    return result.value();
}

std::optional<int> RuleManager::resolve_placeholder_locked(const Placeholder& placeholder,
    std::vector<QJsonObject>& pending_commands, std::vector<ResultEvent>& pending_results) {
    if (placeholder.type == PlaceholderType::ID_REF) {
        // 通过模块管理器查询数值（计算时通过查询模块获取对应数值）
        auto& module_manager = ModuleManager::instance();
        std::string module_name = module_manager.find_module_by_value_id(placeholder.id);
        if (module_name.empty()) {
            LOG_MODULE("RuleManager", "resolve_placeholder_locked", LOG_WARN,
                "数值 ID 不存在: " << placeholder.id);
            return std::nullopt;
        }
        // 数值尚未获取到（无数据源/未接入真实数据）时视为空值，忽略该项
        const ModuleValue* value = module_manager.get_value(module_name, placeholder.id);
        if (!value || !value->get_has_value()) {
            return std::nullopt;
        }
        return module_manager.query_value(module_name, placeholder.id);
    }
    if (placeholder.type == PlaceholderType::RULE_REF) {
        auto nit = index_to_name_.find(placeholder.rule_index);
        if (nit == index_to_name_.end()) {
            LOG_MODULE("RuleManager", "resolve_placeholder_locked", LOG_WARN,
                "规则序号不存在: " << placeholder.rule_index);
            return std::nullopt;
        }
        // 优先使用缓存结果，未计算过则递归计算（级联触发）
        auto lit = last_results_.find(placeholder.rule_index);
        if (lit != last_results_.end() && lit->second.has_value()) {
            return lit->second.value();
        }
        return compute_rule_locked(nit->second, pending_commands, pending_results);
    }
    // EXTERNAL 占位符（{}）：无外部参数来源，视为空值（忽略该项）
    return std::nullopt;
}
