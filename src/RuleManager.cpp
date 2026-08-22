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

// ============================================
// 单例（public）
// ============================================

RuleManager& RuleManager::instance() {
    static RuleManager manager;
    return manager;
}

// ============================================
// 构造/析构（private）
// ============================================

RuleManager::RuleManager()
    : QObject(nullptr) {
    // 监听模块数值变化，触发值模式中引用该数值的规则计算（级联触发）
    connect(&ModuleManager::instance(), &ModuleManager::value_changed,
        this, &RuleManager::on_module_value_changed);
}

RuleManager::~RuleManager() = default;

// ============================================
// 初始化（public）
// ============================================

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

// ============================================
// 文件管理（public）
// ============================================

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

// ============================================
// 规则加载（从配置管理器）（public）
// ============================================

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

// ============================================
// 规则查询（public）
// ============================================

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

// ============================================
// 规则修改（public）
// ============================================

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

// ============================================
// 通道启用状态（public）
// ============================================

void RuleManager::set_channel_enabled(const std::string& channel, bool enabled) {
    std::vector<QJsonObject> pending_commands;
    std::vector<ResultEvent> pending_results;
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
    }
    for (const auto& cmd : pending_commands) {
        emit rule_command_ready(cmd);
    }
    for (const auto& [rule_name, ch, value] : pending_results) {
        emit rule_result_changed(QString::fromStdString(rule_name),
            QString::fromStdString(ch), value);
    }
}

bool RuleManager::get_channel_enabled(const std::string& channel) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = channel_enabled_.find(Rule::normalize_channel(channel));
    return it != channel_enabled_.end() && it->second;
}

// ============================================
// 计算（public）
// ============================================

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

// ============================================
// 辅助（JSON 文件读写）（public）
// ============================================

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

// ============================================
// private slots 实现
// ============================================

void RuleManager::on_module_value_changed(const QString& module_name, const QString& value_id,
    int new_value) {
    // 模块数值变化 → 触发值模式中引用该数值的规则计算
    std::vector<QJsonObject> pending_commands;
    std::vector<ResultEvent> pending_results;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = id_users_.find(value_id.toStdString());
        if (it == id_users_.end()) {
            return;
        }
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
    for (const auto& cmd : pending_commands) {
        emit rule_command_ready(cmd);
    }
    for (const auto& [rule_name2, ch, value] : pending_results) {
        emit rule_result_changed(QString::fromStdString(rule_name2),
            QString::fromStdString(ch), value);
    }
}

// ============================================
// 私有辅助函数实现（private）
// ============================================

void RuleManager::scan_directory() {
    available_files_.clear();
    if (!fs::exists(rules_dir_)) return;
    for (const auto& entry : fs::directory_iterator(rules_dir_)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            std::string filename = entry.path().filename().string();
            if (filename == "rules.json") continue;
            if (filename.find(keyword_) != std::string::npos) {
                available_files_.push_back(filename);
            }
        }
    }
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
        LOG_MODULE("RuleManager", "compute_rule_locked", LOG_DEBUG,
            "规则 " << rule_name << " 未启用，跳过计算");
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
        LOG_MODULE("RuleManager", "compute_rule_locked", LOG_DEBUG,
            "规则 " << rule_name << " 存在空值，本次计算被忽略");
        return std::nullopt;
    }

    // 缓存计算结果
    last_results_[rule.get_index()] = result.value();
    LOG_MODULE("RuleManager", "compute_rule_locked", LOG_DEBUG,
        "规则 " << rule_name << " 计算结果: " << result.value());

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

    // 通道父级：通过调用函数将结果发送给 Python 端
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
            LOG_MODULE("RuleManager", "resolve_placeholder_locked", LOG_DEBUG,
                "数值 " << placeholder.id << " 无数据，按空值处理");
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