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
    info.priority = 0;  // 默认优先级为0

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

    // 如果 manager 是空的，我们需要创建
    if (!it->second.manager) {
        it->second.manager = std::make_shared<ConfigManager>(it->second.file_path);
        it->second.manager->load();
    }
    // 如果已经存在但未加载，确保加载
    else if (it->second.manager->raw().empty()) {
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
            continue;
        }

        // 从JSON中读取优先级
        auto priority_opt = info.manager->get<int>("__priority");
        info.priority = priority_opt.has_value() ? priority_opt.value() : 0;

        std::cout << "  优先级: " << info.priority << std::endl;

        info.last_mod_time = get_file_mod_time(info.file_path);
    }

    // 检查优先级冲突
    std::string error_msg;
    if (has_priority_conflict(error_msg)) {
        std::cerr << "警告: " << error_msg << std::endl;
         throw std::runtime_error(error_msg);
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

bool MultiConfigManager::has_priority_conflict(std::string& error_msg) const {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    std::set<int> priorities;
    std::map<int, std::string> priority_to_name;

    for (const auto& [name, info] : config_registry_) {
        if (info.manager) {
            int priority = info.priority;

            // 检查重复
            if (priorities.find(priority) != priorities.end()) {
                std::stringstream ss;
                ss << "优先级冲突: 文件 '" << name
                    << "' 和 '" << priority_to_name[priority]
                    << "' 有相同的优先级 " << priority;
                error_msg = ss.str();
                return true;
            }

            priorities.insert(priority);
            priority_to_name[priority] = name;
        }
    }

    return false;
}

std::vector<std::shared_ptr<ConfigManager>> MultiConfigManager::get_sorted_configs() const {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    std::vector<std::pair<int, std::shared_ptr<ConfigManager>>> configs_with_priority;

    for (const auto& [name, info] : config_registry_) {
        if (info.manager) {
            configs_with_priority.emplace_back(info.priority, info.manager);
        }
    }

    // 按优先级从低到高排序（优先级低的先执行）
    std::sort(configs_with_priority.begin(),
        configs_with_priority.end(),
        [](const auto& a, const auto& b) {
            return a.first < b.first;
        });

    // 提取配置管理器
    std::vector<std::shared_ptr<ConfigManager>> sorted_configs;
    sorted_configs.reserve(configs_with_priority.size());
    for (const auto& [priority, config] : configs_with_priority) {
        sorted_configs.push_back(config);
    }

    return sorted_configs;
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
