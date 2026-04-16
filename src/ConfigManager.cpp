/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "ConfigManager.h"

#include "DebugLog.h"
#include "DefaultConfigs.h"

#include <algorithm>
#include <fstream>

// ============================================
// 内部辅助函数（非成员，用于复用写入逻辑）
// ============================================
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

// ============================================
// 构造/析构（public）
// ============================================

ConfigManager::ConfigManager(const std::string& path)
    : config_path_(path) {
    DebugLog::instance().set_log_level("ConfigManager", LOG_DEBUG);
}

// ============================================
// 加载与保存（public）
// ============================================

bool ConfigManager::load() {
    LOG_MODULE("ConfigManager", "load", LOG_INFO, "开始加载配置文件: " << config_path_);
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (loaded_) {
        LOG_MODULE("ConfigManager", "load", LOG_DEBUG, "配置已加载，跳过: " << config_path_);
        return true;
    }

    try {
        std::ifstream file(config_path_);
        if (!file.is_open()) {
            LOG_MODULE("ConfigManager", "load", LOG_INFO, "配置文件不存在，创建默认配置: " << config_path_);
            config_ = get_default_config();
            loaded_ = true;
            // 写入默认配置到文件
            write_default_config_to_file(config_, config_path_);
            return true;
        }

        LOG_MODULE("ConfigManager", "load", LOG_DEBUG, "开始解析 JSON 文件: " << config_path_);
        config_ = nlohmann::json::parse(file);
        LOG_MODULE("ConfigManager", "load", LOG_DEBUG, "JSON 解析成功");

        if (!validate()) {
            LOG_MODULE("ConfigManager", "load", LOG_WARN, "配置验证失败，使用默认配置覆盖: " << config_path_);
            config_ = get_default_config();
            write_default_config_to_file(config_, config_path_);
        }
        else {
            LOG_MODULE("ConfigManager", "load", LOG_DEBUG, "配置验证通过");
        }

        loaded_ = true;
        LOG_MODULE("ConfigManager", "load", LOG_INFO, "配置加载成功: " << config_path_);
        return true;
    }
    catch (const std::exception& e) {
        LOG_MODULE("ConfigManager", "load", LOG_ERROR, "加载配置时异常: " << e.what() << "，使用默认配置: " << config_path_);
        config_ = get_default_config();
        loaded_ = true;
        write_default_config_to_file(config_, config_path_);
        return false;
    }
}

bool ConfigManager::save() const {
    LOG_MODULE("ConfigManager", "save", LOG_DEBUG, "开始保存配置文件: " << config_path_);
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    try {
        std::ofstream file(config_path_);
        if (!file.is_open()) {
            LOG_MODULE("ConfigManager", "save", LOG_ERROR, "无法打开文件进行写入: " << config_path_);
            return false;
        }

        file << config_.dump(4);
        file.flush();
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

// ============================================
// 批量操作（public）
// ============================================

bool ConfigManager::update(const nlohmann::json& patch) {
    LOG_MODULE("ConfigManager", "update", LOG_DEBUG, "开始批量更新配置，补丁内容: " << patch.dump());
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    try {
        config_.merge_patch(patch);   // merge_patch 无返回值，直接应用
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

// ============================================
// 验证（public）
// ============================================

bool ConfigManager::validate() const {
    LOG_MODULE("ConfigManager", "validate", LOG_DEBUG, "开始验证配置");
    const std::vector<std::string> required = { "version", "DGLABClient" };

    for (const auto& field : required) {
        if (!get<std::string>(field).has_value()) {
            LOG_MODULE("ConfigManager", "validate", LOG_ERROR, "缺少必需字段: " << field);
            return false;
        }
    }

    auto port = get<int>("app.server_port");
    if (port.has_value() && (port.value() < 1 || port.value() > 65535)) {
        LOG_MODULE("ConfigManager", "validate", LOG_WARN, "端口号无效: " << port.value());
        return false;
    }

    LOG_MODULE("ConfigManager", "validate", LOG_DEBUG, "配置验证通过");
    return true;
}

// ============================================
// 保护方法（protected）
// ============================================

nlohmann::json ConfigManager::get_default_config() const {
    return DefaultConfigs::get_default_config("");
}

// ============================================
// 私有辅助函数实现（private）
// ============================================

std::vector<std::string> ConfigManager::split_key_path(const std::string& key_path) const {
    // 注意：调用此函数前必须已持有 mutex_，因此内部不再加锁
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
