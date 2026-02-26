#include "ConfigManager.h"
#include "DefaultConfigs.h"
#include "DebugLog.h"

#include <fstream>
#include <iostream>
#include <algorithm>

ConfigManager::ConfigManager(const std::string& path)
    : config_path_(path) {
    DebugLog::Instance().set_log_level("ConfigManager", LOG_DEBUG);
    LOG_MODULE("ConfigManager", "ConfigManager", LOG_DEBUG, "创建 ConfigManager 对象，配置文件路径: " << path);
}

bool ConfigManager::load() {
    LOG_MODULE("ConfigManager", "load", LOG_INFO, "开始加载配置文件: " << config_path_);
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (loaded_) {
        LOG_MODULE("ConfigManager", "load", LOG_DEBUG, "配置已加载，跳过加载: " << config_path_);
        return true;  // 已经加载过了，直接返回
    }

    try {
        std::ifstream file(config_path_);
        if (!file.is_open()) {
            LOG_MODULE("ConfigManager", "load", LOG_INFO, "配置文件不存在，创建默认配置: " << config_path_);
            // 文件不存在，创建默认配置
            config_ = get_default_config();
            loaded_ = true;
            LOG_MODULE("ConfigManager", "load", LOG_INFO, "创建默认配置成功: " << config_path_);
            {
                // 不持有锁，保存配置数据
                auto temp_config = config_;
                file.close();  // 确保文件关闭

                // 创建文件并写入配置
                std::ofstream out_file(config_path_);
                if (out_file.is_open()) {
                    out_file << temp_config.dump(4);
                    out_file.flush();
                    LOG_MODULE("ConfigManager", "load", LOG_INFO, "默认配置已写入文件: " << config_path_);
                }
                else {
                    LOG_MODULE("ConfigManager", "load", LOG_WARN, "无法写入默认配置文件: " << config_path_);
                }
            }
            return true;
        }

        LOG_MODULE("ConfigManager", "load", LOG_DEBUG, "开始解析 JSON 文件: " << config_path_);
        config_ = nlohmann::json::parse(file);
        LOG_MODULE("ConfigManager", "load", LOG_DEBUG, "JSON 解析成功");

        // 验证配置
        LOG_MODULE("ConfigManager", "load", LOG_DEBUG, "开始验证配置");
        if (!validate()) {
            LOG_MODULE("ConfigManager", "load", LOG_WARN, "配置验证失败，将使用默认配置覆盖: " << config_path_);
            config_ = get_default_config();
            // 不持有锁，保存配置数据
            {
                auto temp_config = config_;
                std::ofstream out_file(config_path_);
                if (out_file.is_open()) {
                    out_file << temp_config.dump(4);
                    out_file.flush();
                    LOG_MODULE("ConfigManager", "load", LOG_INFO, "默认配置已写入文件: " << config_path_);
                }
            }
        }
        else {
            LOG_MODULE("ConfigManager", "load", LOG_DEBUG, "配置验证通过");
        }

        loaded_ = true;
        LOG_MODULE("ConfigManager", "load", LOG_INFO, "配置加载成功: " << config_path_);
        return true;
    }
    catch (const std::exception& e) {
        LOG_MODULE("ConfigManager", "load", LOG_ERROR, "加载配置时发生异常: " << e.what() << "，将使用默认配置: " << config_path_);
        config_ = get_default_config();
        // 不持有锁，保存配置数据
        {
            auto temp_config = config_;
            std::ofstream out_file(config_path_);
            if (out_file.is_open()) {
                out_file << temp_config.dump(4);
                out_file.flush();
                LOG_MODULE("ConfigManager", "load", LOG_INFO, "默认配置已写入文件: " << config_path_);
            }
        }
        loaded_ = true;
        return false;
    }
}

bool ConfigManager::save() const {
    LOG_MODULE("ConfigManager", "save", LOG_INFO, "开始保存配置文件: " << config_path_);
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

        // 通知监听器
        notify_listeners();

        LOG_MODULE("ConfigManager", "save", LOG_INFO, "配置保存成功: " << config_path_);
        return true;

    }
    catch (const std::exception& e) {
        LOG_MODULE("ConfigManager", "save", LOG_ERROR, "保存配置失败: " << e.what());
        return false;
    }
}

bool ConfigManager::update(const nlohmann::json& patch) {
    LOG_MODULE("ConfigManager", "update", LOG_INFO, "开始批量更新配置，补丁内容: " << patch.dump());
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    try {
        config_.merge_patch(patch);
        LOG_MODULE("ConfigManager", "update", LOG_INFO, "批量更新配置成功");
        return true;
    }
    catch (const std::exception& e) {
        LOG_MODULE("ConfigManager", "update", LOG_ERROR, "批量更新配置失败: " << e.what());
        return false;
    }
}

bool ConfigManager::remove(const std::string& key_path) {
    LOG_MODULE("ConfigManager", "remove", LOG_INFO, "开始删除配置项: " << key_path);
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    try {
        auto keys = split_key_path(key_path);
        LOG_MODULE("ConfigManager", "remove", LOG_DEBUG, "解析后的键路径: " << key_path << " -> 共 " << keys.size() << " 级");
        nlohmann::json* current = &config_;

        for (size_t i = 0; i < keys.size() - 1; ++i) {
            if (!current->contains(keys[i])) {
                LOG_MODULE("ConfigManager", "remove", LOG_WARN, "键路径不存在: " << key_path << " (在 " << keys[i] << " 处)");
                return false;  // 路径不存在
            }
            current = &(*current)[keys[i]];
        }

        bool erased = current->erase(keys.back()) > 0;
        if (erased) {
            LOG_MODULE("ConfigManager", "remove", LOG_INFO, "删除配置项成功: " << key_path);
        }
        else {
            LOG_MODULE("ConfigManager", "remove", LOG_WARN, "删除配置项失败（键不存在）: " << key_path);
        }
        return erased;

    }
    catch (const std::exception& e) {
        LOG_MODULE("ConfigManager", "remove", LOG_ERROR, "删除配置项时发生异常 [" << key_path << "]: " << e.what());
        return false;
    }
}

void ConfigManager::add_listener(std::function<void(const nlohmann::json&)> listener) {
    LOG_MODULE("ConfigManager", "add_listener", LOG_DEBUG, "添加配置监听器，当前监听器数量: " << observers_.size());
    observers_.push_back(std::move(listener));
    LOG_MODULE("ConfigManager", "add_listener", LOG_DEBUG, "监听器添加完成，现在共有 " << observers_.size() << " 个监听器");
}

bool ConfigManager::validate() const {
    LOG_MODULE("ConfigManager", "validate", LOG_DEBUG, "开始验证配置");
    // 基础验证：确保必需字段存在
    const std::vector<std::string> required = {
        "version",
        "DGLABClient"
    };

    for (const auto& field : required) {
        if (!get<std::string>(field).has_value()) {
            LOG_MODULE("ConfigManager", "validate", LOG_ERROR, "缺少必需字段: " << field);
            return false;
        }
    }

    // 类型验证示例
    auto port = get<int>("app.server_port");
    if (port.has_value() && (port.value() < 1 || port.value() > 65535)) {
        LOG_MODULE("ConfigManager", "validate", LOG_WARN, "端口号无效: " << port.value());
        return false;
    }

    LOG_MODULE("ConfigManager", "validate", LOG_DEBUG, "配置验证通过");
    return true;
}

nlohmann::json ConfigManager::get_default_config() const {
    return DefaultConfigs::get_default_config("");
}

std::vector<std::string> ConfigManager::split_key_path(const std::string& key_path) const {
    LOG_MODULE("ConfigManager", "split_key_path", LOG_DEBUG, "分割键路径: " << key_path);
    std::vector<std::string> result;
    std::string current;

    // 尝试从缓存获取
    auto it = split_cache_.find(key_path);
    if (it != split_cache_.end()) {
        LOG_MODULE("ConfigManager", "split_key_path", LOG_DEBUG, "使用缓存分割键路径结果");
        return it->second;
    }

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

    // 记录分割结果
    std::stringstream ss;
    for (size_t i = 0; i < result.size(); ++i) {
        if (i > 0) ss << ".";
        ss << result[i];
    }

    // 存入缓存
    split_cache_[key_path] = result;
    LOG_MODULE("ConfigManager", "split_key_path", LOG_DEBUG, "键路径分割完成: " << key_path << " -> [" << ss.str() << "] (已缓存)");
    return result;
}

void ConfigManager::notify_listeners() const {
    LOG_MODULE("ConfigManager", "notify_listeners", LOG_DEBUG, "开始通知配置监听器，共 " << observers_.size() << " 个");
    for (const auto& listener : observers_) {
        try {
            listener(config_);
        }
        catch (const std::exception& e) {
            LOG_MODULE("ConfigManager", "notify_listeners", LOG_ERROR, "监听器执行时发生异常: " << e.what());
        }
    }
    LOG_MODULE("ConfigManager", "notify_listeners", LOG_DEBUG, "配置监听器通知完成");
}
