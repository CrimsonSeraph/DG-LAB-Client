/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "ConfigManager.h"

#include "DebugLog.h"
#include "DefaultConfigs.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QtGlobal>

#include <chrono>
#include <fstream>
#include <set>
#include <sstream>

// 配置文件监听状态：watcher 与去抖定时器由 ConfigManager 独占持有
struct ConfigWatchState {
    QFileSystemWatcher* watcher = nullptr; ///< 文件与目录监听器
    QTimer* timer = nullptr;               ///< 去抖定时器（合并连续写盘事件）
    QString file_path;                     ///< 被监听的配置文件
    QString dir_path;                      ///< 配置文件所在目录（文件被删除后靠它重建监听）
};

namespace {

    constexpr int kReloadDebounceMs = 300;                     ///< 外部写盘去抖窗口（毫秒）
    constexpr auto kSelfWriteWindow = std::chrono::seconds(5); ///< 自身写盘的判定时间窗

    // 三方合并：只有双方都改过的叶子保留内存值，其余取磁盘值；外部新增的键并入内存
    void merge_three_way(nlohmann::json& ours, const nlohmann::json& base, const nlohmann::json& theirs) {
        if (!ours.is_object() || !theirs.is_object()) {
            return;
        }
        const nlohmann::json base_object = base.is_object() ? base : nlohmann::json::object();
        std::set<std::string> keys;
        for (auto it = ours.begin(); it != ours.end(); ++it) {
            keys.insert(it.key());
        }
        for (auto it = theirs.begin(); it != theirs.end(); ++it) {
            keys.insert(it.key());
        }
        for (const std::string& key : keys) {
            const nlohmann::json::const_iterator base_it = base_object.find(key);
            const bool base_has = base_it != base_object.end();
            nlohmann::json::iterator our_it = ours.find(key);
            const bool ours_has = our_it != ours.end();
            const nlohmann::json::const_iterator their_it = theirs.find(key);
            const bool theirs_has = their_it != theirs.end();

            if (!theirs_has) {
                // 磁盘侧没有该键：内存没改过就跟着删除，内存新增或改过的保留
                if (ours_has && base_has && *our_it == *base_it) {
                    ours.erase(our_it);
                }
                continue;
            }
            if (!ours_has) {
                // 外部新增的键：并入内存，保存时不再丢失
                ours[key] = *their_it;
                continue;
            }
            if (*our_it == *their_it) {
                continue;
            }
            if (base_has && *our_it == *base_it) {
                // 只有磁盘改过：采用磁盘值（外部手改在保存时同样生效）
                *our_it = *their_it;
                continue;
            }
            if (base_has && *their_it == *base_it) {
                // 只有内存改过：保留内存值
                continue;
            }
            if ((*our_it).is_object() && (*their_it).is_object()) {
                // 双方都改过同一子树：递归合并，叶子冲突保留内存值（应用内动作优先）
                merge_three_way(*our_it, base_has ? *base_it : nlohmann::json::object(), *their_it);
            }
        }
    }

} // namespace

// 内部辅助函数（非成员，用于复用写入逻辑）
static void write_default_config_to_file(const nlohmann::json& config, const std::string& path) {
    try {
        std::ofstream out_file(path);
        if (out_file.is_open()) {
            out_file << config.dump(4);
            out_file.flush();
            LOG_MODULE("ConfigManager", "write_default_config", LOG_INFO, "默认配置已写入文件: " << path);
        }
        else {
            LOG_MODULE("ConfigManager", "write_default_config", LOG_WARN, "无法写入默认配置文件: " << path);
        }
    }
    catch (const std::exception& e) {
        LOG_MODULE("ConfigManager", "write_default_config", LOG_ERROR, "写入默认配置文件异常: " << e.what());
    }
    catch (...) {
        LOG_MODULE("ConfigManager", "write_default_config", LOG_ERROR, "写入默认配置文件未知异常");
    }
}

// 构造/析构（public）

ConfigManager::ConfigManager(const std::string& path)
    : config_path_(path) {
    DebugLog::instance().set_log_level("ConfigManager", LOG_DEBUG);
}

ConfigManager::~ConfigManager() {
    if (watch_ != nullptr && QCoreApplication::instance() == nullptr) {
        // 静态析构阶段 QApplication 已销毁：不再触碰 Qt 对象，监听器交进程退出回收
        static_cast<void>(watch_.release());
        return;
    }
    watch_changes(false);
}

// 加载、重读与保存（public）

bool ConfigManager::load() {
    LOG_MODULE("ConfigManager", "load", LOG_INFO, "开始加载配置文件: " << config_path_);
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (loaded_) {
        LOG_MODULE("ConfigManager", "load", LOG_DEBUG, "配置已加载，跳过: " << config_path_);
        return true;
    }

    std::string raw;
    nlohmann::json parsed;
    const DiskReadResult result = read_disk_config(raw, parsed);
    if (result != DiskReadResult::kOk) {
        // 文件缺失或损坏：沿用既有语义，落盘一份默认配置
        LOG_MODULE("ConfigManager", "load",
            result == DiskReadResult::kMissing ? LOG_INFO : LOG_ERROR,
            "配置文件不可用，使用默认配置: " << config_path_);
        config_ = get_default_config();
        loaded_ = true;
        write_default_config_to_file(config_, config_path_);
        remember_synced_content(config_.dump(4));
        return result == DiskReadResult::kMissing;
    }

    config_ = std::move(parsed);
    remember_synced_content(raw);
    if (!validate()) {
        LOG_MODULE("ConfigManager", "load", LOG_WARN, "配置验证失败，使用默认配置覆盖: " << config_path_);
        config_ = get_default_config();
        write_default_config_to_file(config_, config_path_);
        remember_synced_content(config_.dump(4));
    }
    else {
        LOG_MODULE("ConfigManager", "load", LOG_DEBUG, "配置验证通过");
    }

    loaded_ = true;
    LOG_MODULE("ConfigManager", "load", LOG_INFO, "配置加载成功: " << config_path_);
    return true;
}

bool ConfigManager::reload() {
    LOG_MODULE("ConfigManager", "reload", LOG_INFO, "开始重读配置文件: " << config_path_);
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    std::string raw;
    nlohmann::json parsed;
    const DiskReadResult result = read_disk_config(raw, parsed);
    if (result == DiskReadResult::kMissing) {
        LOG_MODULE("ConfigManager", "reload", LOG_WARN, "配置文件不存在，保留上一份有效配置: " << config_path_);
        return false;
    }
    if (result == DiskReadResult::kInvalid) {
        LOG_MODULE("ConfigManager", "reload", LOG_WARN, "配置 JSON 无法解析，保留上一份有效配置: " << config_path_);
        return false;
    }
    if (!validate_json(parsed)) {
        LOG_MODULE("ConfigManager", "reload", LOG_WARN, "配置校验未通过，保留上一份有效配置: " << config_path_);
        return false;
    }

    config_ = std::move(parsed);
    remember_synced_content(raw);
    loaded_ = true;
    LOG_MODULE("ConfigManager", "reload", LOG_INFO, "配置重读成功: " << config_path_);
    notify_listeners();
    return true;
}

bool ConfigManager::save() {
    LOG_MODULE("ConfigManager", "save", LOG_DEBUG, "开始保存配置文件: " << config_path_);
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    try {
        // 写盘前先对齐磁盘：外部新增的键并入内存，避免用旧值整份覆盖
        align_with_disk_before_save();

        std::ofstream file(config_path_);
        if (!file.is_open()) {
            LOG_MODULE("ConfigManager", "save", LOG_ERROR, "无法打开文件进行写入: " << config_path_);
            return false;
        }

        const std::string payload = config_.dump(4);
        file << payload;
        file.flush();
        mark_self_write(payload);
        LOG_MODULE("ConfigManager", "save", LOG_DEBUG, "配置数据已写入文件");

        notify_listeners();
        LOG_MODULE("ConfigManager", "save", LOG_INFO, "配置保存成功: " << config_path_);
        return true;
    }
    catch (const std::exception& e) {
        LOG_MODULE("ConfigManager", "save", LOG_ERROR, "保存配置失败: " << e.what());
        return false;
    }
}

void ConfigManager::watch_changes(bool enabled) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (!enabled) {
        if (!watch_) {
            return;
        }
        delete watch_->timer;
        delete watch_->watcher;
        watch_.reset();
        LOG_MODULE("ConfigManager", "watch_changes", LOG_INFO, "已停止配置文件监听: " << config_path_);
        return;
    }

    if (watch_) {
        LOG_MODULE("ConfigManager", "watch_changes", LOG_DEBUG, "配置文件监听已开启，跳过: " << config_path_);
        return;
    }
    if (QCoreApplication::instance() == nullptr) {
        LOG_MODULE("ConfigManager", "watch_changes", LOG_WARN, "没有 QCoreApplication，跳过配置文件监听: " << config_path_);
        return;
    }

    auto state = std::make_unique<ConfigWatchState>();
    state->file_path = QString::fromStdString(config_path_);
    state->dir_path = QFileInfo(state->file_path).absolutePath();
    state->watcher = new QFileSystemWatcher();
    state->timer = new QTimer();
    state->timer->setSingleShot(true);
    state->timer->setInterval(kReloadDebounceMs);

    QFileSystemWatcher* watcher = state->watcher;
    QTimer* timer = state->timer;
    const QString file_path = state->file_path;
    const QString dir_path = state->dir_path;

    // 路径事件：把被删除的 watch 补回来，再重启去抖定时器（连续保存只重读一次）
    auto on_path_event = [watcher, timer, file_path, dir_path]() {
        if (QFileInfo::exists(file_path) && !watcher->files().contains(file_path)) {
            watcher->addPath(file_path);
        }
        if (QFileInfo::exists(dir_path) && !watcher->directories().contains(dir_path)) {
            watcher->addPath(dir_path);
        }
        timer->start();
    };
    QObject::connect(watcher, &QFileSystemWatcher::fileChanged, timer,
        [on_path_event](const QString&) { on_path_event(); });
    QObject::connect(watcher, &QFileSystemWatcher::directoryChanged, timer,
        [on_path_event](const QString&) { on_path_event(); });
    QObject::connect(timer, &QTimer::timeout, timer, [this]() { handle_debounced_event(); });

    // 目录与文件都监听：编辑器「先删后建」的保存方式会丢掉文件级 watch，目录事件负责重建
    if (!state->watcher->addPath(state->dir_path) || !state->watcher->addPath(state->file_path)) {
        LOG_MODULE("ConfigManager", "watch_changes", LOG_WARN,
            "配置文件监听部分未生效（文件可能尚不存在）: " << config_path_);
    }
    watch_ = std::move(state);
    LOG_MODULE("ConfigManager", "watch_changes", LOG_INFO,
        "已开启配置文件监听，去抖 " << kReloadDebounceMs << " ms: " << config_path_);
}

bool ConfigManager::watching_changes() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return watch_ != nullptr;
}

// 批量操作（public）

bool ConfigManager::update(const nlohmann::json& patch) {
    LOG_MODULE("ConfigManager", "update", LOG_DEBUG, "开始批量更新配置，补丁内容: " << patch.dump());
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    try {
        // merge_patch 无返回值，直接应用
        config_.merge_patch(patch);
        LOG_MODULE("ConfigManager", "update", LOG_DEBUG, "批量更新配置成功");
        notify_listeners();
        return true;
    }
    catch (const std::exception& e) {
        LOG_MODULE("ConfigManager", "update", LOG_ERROR, "批量更新配置失败: " << e.what());
        return false;
    }
}

bool ConfigManager::remove(const std::string& key_path) {
    LOG_MODULE("ConfigManager", "remove", LOG_DEBUG, "开始删除配置项: " << key_path);
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    try {
        auto keys = split_key_path(key_path);
        nlohmann::json* current = &config_;

        for (size_t i = 0; i < keys.size() - 1; ++i) {
            if (!current->contains(keys[i])) {
                LOG_MODULE("ConfigManager", "remove", LOG_WARN, "键路径不存在: " << key_path << " (在 " << keys[i] << " 处)");
                return false;
            }
            current = &(*current)[keys[i]];
        }

        bool erased = current->erase(keys.back()) > 0;
        if (erased) {
            LOG_MODULE("ConfigManager", "remove", LOG_DEBUG, "删除配置项成功: " << key_path);
        }
        else {
            LOG_MODULE("ConfigManager", "remove", LOG_WARN, "删除配置项失败（键不存在）: " << key_path);
        }
        return erased;
    }
    catch (const std::exception& e) {
        LOG_MODULE("ConfigManager", "remove", LOG_ERROR, "删除配置项时异常 [" << key_path << "]: " << e.what());
        return false;
    }
}

void ConfigManager::add_listener(std::function<void(const nlohmann::json&)> listener) {
    LOG_MODULE("ConfigManager", "add_listener", LOG_DEBUG, "添加配置监听器，当前数量: " << observers_.size());
    observers_.push_back(std::move(listener));
    LOG_MODULE("ConfigManager", "add_listener", LOG_DEBUG, "监听器添加完成，现在共 " << observers_.size() << " 个");
}

// 验证（public）

bool ConfigManager::validate() const {
    return validate_json(config_);
}

// 保护方法（protected）

nlohmann::json ConfigManager::get_default_config() const {
    return DefaultConfigs::get_default_config("");
}

bool ConfigManager::validate_json(const nlohmann::json& candidate) const {
    LOG_MODULE("ConfigManager", "validate_json", LOG_DEBUG, "开始验证配置");
    if (!candidate.is_object()) {
        LOG_MODULE("ConfigManager", "validate_json", LOG_ERROR, "配置不是 JSON 对象");
        return false;
    }

    const std::vector<std::string> required = {"version", "DGLABClient"};
    for (const auto& field : required) {
        const nlohmann::json::const_iterator it = candidate.find(field);
        if (it == candidate.end() || !it->is_string()) {
            LOG_MODULE("ConfigManager", "validate_json", LOG_ERROR, "缺少必需字段: " << field);
            return false;
        }
    }

    const nlohmann::json::const_iterator app_it = candidate.find("app");
    if (app_it != candidate.end() && app_it->is_object()) {
        const nlohmann::json::const_iterator websocket_it = app_it->find("websocket");
        if (websocket_it != app_it->end() && websocket_it->is_object()) {
            const nlohmann::json::const_iterator port_it = websocket_it->find("port");
            if (port_it != websocket_it->end() && port_it->is_number()) {
                const int port = static_cast<int>(port_it->get<double>());
                if (port < 1 || port > 65535) {
                    LOG_MODULE("ConfigManager", "validate_json", LOG_WARN, "端口号无效: " << port);
                    return false;
                }
            }
        }
    }

    LOG_MODULE("ConfigManager", "validate_json", LOG_DEBUG, "配置验证通过");
    return true;
}

// 私有辅助函数实现（private）

std::vector<std::string> ConfigManager::split_key_path(const std::string& key_path) const {
    // 注意: 调用此函数前必须已持有 mutex_，因此内部不再加锁
    auto it = split_cache_.find(key_path);
    if (it != split_cache_.end()) {
        return it->second;
    }

    std::vector<std::string> result;
    std::string current;
    for (char c : key_path) {
        if (c == '.') {
            if (!current.empty()) {
                result.push_back(current);
                current.clear();
            }
        }
        else {
            current += c;
        }
    }
    if (!current.empty()) {
        result.push_back(current);
    }

    split_cache_[key_path] = result;
    return result;
}

void ConfigManager::notify_listeners() const {
    LOG_MODULE("ConfigManager", "notify_listeners", LOG_DEBUG, "开始通知配置监听器，共 " << observers_.size() << " 个");
    for (const auto& listener : observers_) {
        try {
            listener(config_);
        }
        catch (const std::exception& e) {
            LOG_MODULE("ConfigManager", "notify_listeners", LOG_ERROR, "监听器执行异常: " << e.what());
        }
    }
    LOG_MODULE("ConfigManager", "notify_listeners", LOG_DEBUG, "配置监听器通知完成");
}

ConfigManager::DiskReadResult ConfigManager::read_disk_config(std::string& raw, nlohmann::json& parsed) const {
    std::ifstream file(config_path_);
    if (!file.is_open()) {
        return DiskReadResult::kMissing;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    raw = buffer.str();
    try {
        parsed = nlohmann::json::parse(raw);
    }
    catch (const std::exception& e) {
        LOG_MODULE("ConfigManager", "read_disk_config", LOG_ERROR, "配置 JSON 解析失败: " << e.what());
        return DiskReadResult::kInvalid;
    }
    return DiskReadResult::kOk;
}

void ConfigManager::remember_synced_content(const std::string& raw) const {
    synced_content_ = raw;
    has_self_write_ = false;
}

void ConfigManager::mark_self_write(const std::string& payload) const {
    synced_content_ = payload;
    self_write_content_ = payload;
    self_write_at_ = std::chrono::steady_clock::now();
    has_self_write_ = true;
}

bool ConfigManager::consume_self_write_event() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!has_self_write_) {
        return false;
    }
    if (std::chrono::steady_clock::now() - self_write_at_ > kSelfWriteWindow) {
        has_self_write_ = false;
        return false;
    }

    std::string raw;
    nlohmann::json parsed;
    // 磁盘内容仍等于最近一次自写内容 → 这次事件是自己的保存引起的，丢弃避免重读循环
    if (read_disk_config(raw, parsed) != DiskReadResult::kOk || raw != self_write_content_) {
        has_self_write_ = false;
        return false;
    }
    has_self_write_ = false;
    LOG_MODULE("ConfigManager", "consume_self_write_event", LOG_DEBUG, "忽略自身写盘触发的事件: " << config_path_);
    return true;
}

void ConfigManager::align_with_disk_before_save() {
    std::string raw;
    nlohmann::json parsed;
    if (read_disk_config(raw, parsed) != DiskReadResult::kOk) {
        // 文件缺失或损坏：按内存原样写回
        return;
    }
    if (raw == synced_content_) {
        return;
    }

    nlohmann::json base = nlohmann::json::object();
    if (!synced_content_.empty()) {
        base = nlohmann::json::parse(synced_content_, nullptr, false);
        if (base.is_discarded()) {
            base = nlohmann::json::object();
        }
    }
    merge_three_way(config_, base, parsed);
    LOG_MODULE("ConfigManager", "align_with_disk_before_save", LOG_INFO,
        "写盘前已并入外部改动（外部新增的键不会丢失）: " << config_path_);
}

bool ConfigManager::disk_content_matches_synced() const {
    std::string raw;
    nlohmann::json parsed;
    return read_disk_config(raw, parsed) == DiskReadResult::kOk && raw == synced_content_;
}

void ConfigManager::handle_debounced_event() {
    if (consume_self_write_event()) {
        return;
    }
    // 目录级事件把同目录其它配置文件的写盘也报过来：本文件内容没变就不重读
    if (disk_content_matches_synced()) {
        return;
    }
    if (!reload()) {
        LOG_MODULE("ConfigManager", "handle_debounced_event", LOG_WARN,
            "重读失败，保留上一份有效配置: " << config_path_);
    }
}
