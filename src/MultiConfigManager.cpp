#include "MultiConfigManager.h"
#include <iostream>
#include <filesystem>
#include <chrono>
#include <thread>

namespace fs = std::filesystem;

void MultiConfigManager::register_config(const std::string& name,
    const std::string& file_path,
    bool auto_reload) {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    if (config_registry_.find(name) != config_registry_.end()) {
        throw std::runtime_error("配置已注册: " + name);
    }

    ConfigInfo info;
    info.file_path = file_path;
    info.auto_reload = auto_reload;
    info.last_mod_time = get_file_mod_time(file_path);

    // 创建但不立即加载
    info.manager = std::make_shared<ConfigManager>(file_path);

    config_registry_[name] = std::move(info);

    std::cout << "注册配置: " << name << " -> " << file_path << std::endl;
}

std::shared_ptr<ConfigManager> MultiConfigManager::get_config(const std::string& name) {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    auto it = config_registry_.find(name);
    if (it == config_registry_.end()) {
        throw std::runtime_error("配置未注册: " + name);
    }

    // 如果是第一次获取，确保加载
    if (!it->second.manager->raw().empty()) {
        it->second.manager->load();
    }

    return it->second.manager;
}

bool MultiConfigManager::load_all() {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    bool all_success = true;

    for (auto& [name, info] : config_registry_) {
        std::cout << "加载配置: " << name << std::endl;
        if (!info.manager->load()) {
            std::cerr << "  失败: " << name << std::endl;
            all_success = false;
        }
        else {
            info.last_mod_time = get_file_mod_time(info.file_path);
        }
    }

    return all_success;
}

bool MultiConfigManager::save_all() {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    bool all_success = true;

    for (auto& [name, info] : config_registry_) {
        std::cout << "保存配置: " << name << std::endl;
        if (!info.manager->save()) {
            std::cerr << "  失败: " << name << std::endl;
            all_success = false;
        }
    }

    return all_success;
}

bool MultiConfigManager::reload(const std::string& name) {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    auto it = config_registry_.find(name);
    if (it == config_registry_.end()) {
        std::cerr << "配置未注册: " << name << std::endl;
        return false;
    }

    bool success = it->second.manager->load();
    if (success) {
        it->second.last_mod_time = get_file_mod_time(it->second.file_path);
    }

    return success;
}

void MultiConfigManager::enable_hot_reload(bool enabled) {
    if (enabled && !hot_reload_enabled_) {
        hot_reload_enabled_ = true;
        start_file_watcher();
    }
    else if (!enabled && hot_reload_enabled_) {
        hot_reload_enabled_ = false;
        stop_file_watcher();
    }
}

std::vector<std::string> MultiConfigManager::get_config_names() const {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    std::vector<std::string> names;
    names.reserve(config_registry_.size());

    for (const auto& [name, _] : config_registry_) {
        names.push_back(name);
    }

    return names;
}

void MultiConfigManager::start_file_watcher() {
    if (running_) return;

    running_ = true;
    file_watcher_thread_ = std::thread([this]() {
        file_watcher_loop();
        });
}

void MultiConfigManager::stop_file_watcher() {
    if (!running_) return;

    running_ = false;
    if (file_watcher_thread_.joinable()) {
        file_watcher_thread_.join();
    }
}

void MultiConfigManager::file_watcher_loop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(2));

        std::lock_guard<std::mutex> lock(registry_mutex_);

        for (auto& [name, info] : config_registry_) {
            if (!info.auto_reload) continue;

            auto current_time = get_file_mod_time(info.file_path);
            if (current_time > info.last_mod_time) {
                std::cout << "检测到文件变化，重新加载: " << name << std::endl;

                try {
                    if (info.manager->load()) {
                        info.last_mod_time = current_time;
                        std::cout << "  重新加载成功" << std::endl;
                    }
                    else {
                        std::cerr << "  重新加载失败" << std::endl;
                    }
                }
                catch (const std::exception& e) {
                    std::cerr << "  重新加载异常: " << e.what() << std::endl;
                }
            }
        }
    }
}

std::time_t MultiConfigManager::get_file_mod_time(const std::string& path) const {
    try {
#ifdef _WIN32
        struct _stat64 file_stat;
        if (_stat64(path.c_str(), &file_stat) == 0)
            return file_stat.st_mtime;
#else
        struct stat file_stat;
        if (stat(path.c_str(), &file_stat) == 0)
            return file_stat.st_mtime;
#endif

        return 0;
    }
    catch (...) {
        return 0;
    }
}
