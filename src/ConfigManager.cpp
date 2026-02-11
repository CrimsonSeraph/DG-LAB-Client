#include "ConfigManager.h"

#include <fstream>
#include <iostream>
#include <algorithm>

ConfigManager::ConfigManager(const std::string& path)
    : config_path_(path) {
}

bool ConfigManager::load() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (loaded_) {
        return true;  // 已经加载过了，直接返回
    }

    try {
        std::ifstream file(config_path_);
        if (!file.is_open()) {
            // 文件不存在，创建默认配置
            config_ = get_default_config();
            loaded_ = true;
            std::cout << "创建默认配置: " << config_path_ << std::endl;
            {
                // 不持有锁，保存配置数据
                auto temp_config = config_;
                file.close();  // 确保文件关闭

                // 创建文件并写入配置
                std::ofstream out_file(config_path_);
                if (out_file.is_open()) {
                    out_file << temp_config.dump(4);
                    out_file.flush();
                }
            }
            return true;
        }

        config_ = nlohmann::json::parse(file);

        // 验证配置
        if (!validate()) {
            std::cerr << "配置验证失败，使用默认配置: " << config_path_ << std::endl;
            config_ = get_default_config();
            // 不持有锁，保存配置数据
            {
                auto temp_config = config_;
                std::ofstream out_file(config_path_);
                if (out_file.is_open()) {
                    out_file << temp_config.dump(4);
                    out_file.flush();
                }
            }
        }

        loaded_ = true;
        std::cout << "配置加载成功: " << config_path_ << std::endl;
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "加载配置失败: " << e.what() << std::endl;
        config_ = get_default_config();
        // 不持有锁，保存配置数据
        {
            auto temp_config = config_;
            std::ofstream out_file(config_path_);
            if (out_file.is_open()) {
                out_file << temp_config.dump(4);
                out_file.flush();
            }
        }
        loaded_ = true;
        return false;
    }
}

bool ConfigManager::save() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    try {
        std::ofstream file(config_path_);
        if (!file.is_open()) {
            std::cerr << "无法打开文件: " << config_path_ << std::endl;
            return false;
        }

        file << config_.dump(4);
        file.flush();

        // 通知监听器
        notify_listeners();

        std::cout << "配置保存成功: " << config_path_ << std::endl;
        return true;

    }
    catch (const std::exception& e) {
        std::cerr << "保存配置失败: " << e.what() << std::endl;
        return false;
    }
}

bool ConfigManager::update(const nlohmann::json& patch) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    try {
        config_.merge_patch(patch);
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "批量更新配置失败: " << e.what() << std::endl;
        return false;
    }
}

bool ConfigManager::remove(const std::string& key_path) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    try {
        auto keys = split_key_path(key_path);
        nlohmann::json* current = &config_;

        for (size_t i = 0; i < keys.size() - 1; ++i) {
            if (!current->contains(keys[i])) {
                return false;  // 路径不存在
            }
            current = &(*current)[keys[i]];
        }

        return current->erase(keys.back()) > 0;

    }
    catch (const std::exception& e) {
        std::cerr << "删除配置失败 [" << key_path << "]: " << e.what() << std::endl;
        return false;
    }
}

void ConfigManager::add_listener(std::function<void(const nlohmann::json&)> listener) {
    observers_.push_back(std::move(listener));
}

bool ConfigManager::validate() const {
    // 基础验证：确保必需字段存在
    const std::vector<std::string> required = {
        "version",
        "DGLABClient"
    };

    for (const auto& field : required) {
        if (!get<std::string>(field).has_value()) {
            std::cerr << "缺少必需字段: " << field << std::endl;
            return false;
        }
    }

    // 类型验证示例
    auto port = get<int>("server.port");
    if (port.has_value() && (port.value() < 1 || port.value() > 65535)) {
        std::cerr << "端口号无效: " << port.value() << std::endl;
        return false;
    }

    return true;
}

nlohmann::json ConfigManager::get_default_config() const {
    return nlohmann::json{
        //{"version", "1.0.0"},
        //{"app", {
        //    {"name", "DGLABClient"},
        //    {"debug", false},
        //    {"log_level", "info"}
        //}},
        //{"server", {
        //    {"port", 8080},
        //    {"threads", 4},
        //    {"timeout", 30}
        //}},
    };
}

std::vector<std::string> ConfigManager::split_key_path(const std::string& key_path) const {
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

    return result;
}

void ConfigManager::notify_listeners() const {
    for (const auto& listener : observers_) {
        try {
            listener(config_);
        }
        catch (const std::exception& e) {
            std::cerr << "监听器错误: " << e.what() << std::endl;
        }
    }
}
