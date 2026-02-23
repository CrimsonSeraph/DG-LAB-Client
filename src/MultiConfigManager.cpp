#include "MultiConfigManager.h"
#include <iostream>
#include <filesystem>
#include <chrono>
#include <thread>

namespace fs = std::filesystem;

void MultiConfigManager::register_config(const std::string& name,
    const std::string& file_path,
    bool auto_reload) {
    LOG_MODULE("MultiConfigManager", "register_config", LOG_DEBUG, "进入 register_config，名称=" << name
        << "，路径=" << file_path << "，自动重载=" << (auto_reload ? "是" : "否"));
    std::lock_guard<std::mutex> lock(registry_mutex_);

    if (config_registry_.find(name) != config_registry_.end()) {
        LOG_MODULE("MultiConfigManager", "register_config", LOG_INFO,
            "配置已存在，跳过注册: " << name);
        return;
    }

    ConfigInfo info;
    info.file_path = file_path;
    info.auto_reload = auto_reload;
    info.last_mod_time = get_file_mod_time(file_path);
    info.priority = 0;  // 默认优先级为0

    // 创建但不立即加载
    info.manager = std::make_shared<ConfigManager>(file_path);

    config_registry_[name] = std::move(info);
    LOG_MODULE("MultiConfigManager", "register_config", LOG_INFO, "配置注册成功: " << name);
}

std::shared_ptr<ConfigManager> MultiConfigManager::get_config(const std::string& name) {
    LOG_MODULE("MultiConfigManager", "get_config", LOG_DEBUG, "进入 get_config，名称=" << name);
    std::lock_guard<std::mutex> lock(registry_mutex_);

    auto it = config_registry_.find(name);
    if (it == config_registry_.end()) {
        LOG_MODULE("MultiConfigManager", "get_config", LOG_ERROR, "配置未注册，获取失败: " << name);
        throw std::runtime_error("配置未注册: " + name);
    }

    // 如果 manager 是空的，我们需要创建
    if (!it->second.manager) {
        LOG_MODULE("MultiConfigManager", "get_config", LOG_DEBUG, "配置管理器为空，创建新实例: " << name);
        it->second.manager = std::make_shared<ConfigManager>(it->second.file_path);
        it->second.manager->load();
        LOG_MODULE("MultiConfigManager", "get_config", LOG_DEBUG, "配置管理器创建并加载完成: " << name);
    }
    // 如果已经存在但未加载，确保加载
    else if (it->second.manager->raw().empty()) {
        LOG_MODULE("MultiConfigManager", "get_config", LOG_DEBUG, "配置管理器已存在但内容为空，重新加载: " << name);
        it->second.manager->load();
        LOG_MODULE("MultiConfigManager", "get_config", LOG_DEBUG, "重新加载完成: " << name);
    }

    LOG_MODULE("MultiConfigManager", "get_config", LOG_INFO, "获取配置成功: " << name);
    return it->second.manager;
}

bool MultiConfigManager::load_all() {
    LOG_MODULE("MultiConfigManager", "load_all", LOG_INFO, "开始加载所有配置");
    std::lock_guard<std::mutex> lock(registry_mutex_);
    bool all_success = true;

    for (auto& [name, info] : config_registry_) {
        LOG_MODULE("MultiConfigManager", "load_all", LOG_INFO, "正在加载配置: " << name);

        if (!info.manager->load()) {
            LOG_MODULE("MultiConfigManager", "load_all", LOG_ERROR, "配置加载失败: " << name);
            all_success = false;
            continue;
        }

        // 从JSON中读取优先级
        auto priority_opt = info.manager->get<int>("__priority");
        info.priority = priority_opt.has_value() ? priority_opt.value() : 0;

        LOG_MODULE("MultiConfigManager", "load_all", LOG_INFO, "配置加载成功，优先级=" << info.priority << " : " << name);

        info.last_mod_time = get_file_mod_time(info.file_path);
    }

    // 检查优先级冲突
    std::string error_msg;
    if (has_priority_conflict_unsafe(error_msg)) {
        LOG_MODULE("MultiConfigManager", "load_all", LOG_WARN, "优先级冲突检测到: " << error_msg);
        throw std::runtime_error(error_msg);
    }

    LOG_MODULE("MultiConfigManager", "load_all", LOG_INFO, "所有配置加载完成，全部成功=" << (all_success ? "是" : "否"));
    return all_success;
}

bool MultiConfigManager::save_all() {
    LOG_MODULE("MultiConfigManager", "save_all", LOG_INFO, "开始保存所有配置");
    std::lock_guard<std::mutex> lock(registry_mutex_);
    bool all_success = true;

    for (auto& [name, info] : config_registry_) {
        LOG_MODULE("MultiConfigManager", "save_all", LOG_INFO, "正在保存配置: " << name);
        if (!info.manager->save()) {
            LOG_MODULE("MultiConfigManager", "save_all", LOG_ERROR, "配置保存失败: " << name);
            all_success = false;
        }
        else {
            LOG_MODULE("MultiConfigManager", "save_all", LOG_INFO, "配置保存成功: " << name);
        }
    }

    LOG_MODULE("MultiConfigManager", "save_all", LOG_INFO, "所有配置保存完成，全部成功=" << (all_success ? "是" : "否"));
    return all_success;
}

bool MultiConfigManager::reload(const std::string& name) {
    LOG_MODULE("MultiConfigManager", "reload", LOG_DEBUG, "进入 reload，名称=" << name);
    std::lock_guard<std::mutex> lock(registry_mutex_);

    auto it = config_registry_.find(name);
    if (it == config_registry_.end()) {
        LOG_MODULE("MultiConfigManager", "reload", LOG_ERROR, "配置未注册，重载失败: " << name);
        return false;
    }

    LOG_MODULE("MultiConfigManager", "reload", LOG_INFO, "开始重载配置: " << name);
    bool success = it->second.manager->load();
    if (success) {
        it->second.last_mod_time = get_file_mod_time(it->second.file_path);
        LOG_MODULE("MultiConfigManager", "reload", LOG_INFO, "重载配置成功: " << name);
    }
    else {
        LOG_MODULE("MultiConfigManager", "reload", LOG_ERROR, "重载配置失败: " << name);
    }

    return success;
}

void MultiConfigManager::enable_hot_reload(bool enabled) {
    LOG_MODULE("MultiConfigManager", "enable_hot_reload", LOG_INFO, "设置热重载状态: " << (enabled ? "开启" : "关闭"));
    if (enabled && !hot_reload_enabled_) {
        hot_reload_enabled_ = true;
        start_file_watcher();
        LOG_MODULE("MultiConfigManager", "enable_hot_reload", LOG_INFO, "热重载已开启");
    }
    else if (!enabled && hot_reload_enabled_) {
        hot_reload_enabled_ = false;
        stop_file_watcher();
        LOG_MODULE("MultiConfigManager", "enable_hot_reload", LOG_INFO, "热重载已关闭");
    }
}

std::vector<std::string> MultiConfigManager::get_config_names() const {
    LOG_MODULE("MultiConfigManager", "get_config_names", LOG_DEBUG, "进入 get_config_names");
    std::lock_guard<std::mutex> lock(registry_mutex_);

    std::vector<std::string> names;
    names.reserve(config_registry_.size());

    for (const auto& [name, _] : config_registry_) {
        names.push_back(name);
    }

    LOG_MODULE("MultiConfigManager", "get_config_names", LOG_DEBUG, "获取到 " << names.size() << " 个配置名称");
    return names;
}

bool MultiConfigManager::has_priority_conflict_unsafe(std::string& error_msg) const {
    LOG_MODULE("MultiConfigManager", "has_priority_conflict_unsafe", LOG_DEBUG, "检查优先级冲突（无锁调用）");
    std::set<int> priorities;
    std::map<int, std::string> priority_to_name;

    for (const auto& [name, info] : config_registry_) {
        if (info.manager) {
            int priority = info.priority;

            // 检查重复
            if (priorities.find(priority) != priorities.end()) {
                std::stringstream ss;
                ss << "优先级冲突: 配置 '" << name
                    << "' 和 '" << priority_to_name[priority]
                    << "' 有相同的优先级 " << priority;
                error_msg = ss.str();
                LOG_MODULE("MultiConfigManager", "has_priority_conflict_unsafe", LOG_WARN, "检测到优先级冲突: " << error_msg);
                return true;
            }

            priorities.insert(priority);
            priority_to_name[priority] = name;
        }
    }

    LOG_MODULE("MultiConfigManager", "has_priority_conflict_unsafe", LOG_DEBUG, "未检测到优先级冲突");
    return false;
}

bool MultiConfigManager::has_priority_conflict(std::string& error_msg) const {
    LOG_MODULE("MultiConfigManager", "has_priority_conflict", LOG_DEBUG, "检查优先级冲突（加锁）");
    std::lock_guard<std::mutex> lock(registry_mutex_);
    return has_priority_conflict_unsafe(error_msg);
}

std::vector<std::shared_ptr<ConfigManager>> MultiConfigManager::get_sorted_configs_unsafe() const {
    LOG_MODULE("MultiConfigManager", "get_sorted_configs_unsafe", LOG_DEBUG, "获取按优先级排序的配置列表（无锁）");
    std::vector<std::pair<int, std::shared_ptr<ConfigManager>>> configs_with_priority;

    for (const auto& [name, info] : config_registry_) {
        if (info.manager) {
            configs_with_priority.emplace_back(info.priority, info.manager);
            LOG_MODULE("MultiConfigManager", "get_sorted_configs_unsafe", LOG_DEBUG, "配置: " << name << " 优先级=" << info.priority);
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

    LOG_MODULE("MultiConfigManager", "get_sorted_configs_unsafe", LOG_DEBUG, "排序完成，共 " << sorted_configs.size() << " 个配置");
    return sorted_configs;
}

std::vector<std::shared_ptr<ConfigManager>> MultiConfigManager::get_sorted_configs() const {
    LOG_MODULE("MultiConfigManager", "get_sorted_configs", LOG_DEBUG, "获取按优先级排序的配置列表（加锁）");
    std::lock_guard<std::mutex> lock(registry_mutex_);
    return get_sorted_configs_unsafe();
}

void MultiConfigManager::start_file_watcher() {
    LOG_MODULE("MultiConfigManager", "start_file_watcher", LOG_INFO, "启动文件监控线程");
    if (running_) {
        LOG_MODULE("MultiConfigManager", "start_file_watcher", LOG_WARN, "文件监控线程已在运行");
        return;
    }

    running_ = true;
    file_watcher_thread_ = std::thread([this]() {
        LOG_MODULE("MultiConfigManager", "start_file_watcher", LOG_INFO, "文件监控线程已启动");
        file_watcher_loop();
        });
}

void MultiConfigManager::stop_file_watcher() {
    LOG_MODULE("MultiConfigManager", "stop_file_watcher", LOG_INFO, "停止文件监控线程");
    if (!running_) {
        LOG_MODULE("MultiConfigManager", "stop_file_watcher", LOG_WARN, "文件监控线程未运行");
        return;
    }

    running_ = false;
    if (file_watcher_thread_.joinable()) {
        file_watcher_thread_.join();
        LOG_MODULE("MultiConfigManager", "stop_file_watcher", LOG_INFO, "文件监控线程已停止");
    }
}

void MultiConfigManager::file_watcher_loop() {
    LOG_MODULE("MultiConfigManager", "file_watcher_loop", LOG_INFO, "文件监控循环开始");
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        LOG_MODULE("MultiConfigManager", "file_watcher_loop", LOG_DEBUG, "文件监控循环唤醒，检查文件变化");

        std::lock_guard<std::mutex> lock(registry_mutex_);

        for (auto& [name, info] : config_registry_) {
            if (!info.auto_reload) continue;

            auto current_time = get_file_mod_time(info.file_path);
            if (current_time > info.last_mod_time) {
                LOG_MODULE("MultiConfigManager", "file_watcher_loop", LOG_INFO,
                    "检测到文件变化，重新加载配置: " << name);

                try {
                    if (info.manager->load()) {
                        info.last_mod_time = current_time;
                        LOG_MODULE("MultiConfigManager", "file_watcher_loop", LOG_INFO,
                            "重新加载成功: " << name);
                    }
                    else {
                        LOG_MODULE("MultiConfigManager", "file_watcher_loop", LOG_ERROR,
                            "重新加载失败: " << name);
                    }
                }
                catch (const std::exception& e) {
                    LOG_MODULE("MultiConfigManager", "file_watcher_loop", LOG_ERROR,
                        "重新加载异常: " << name << " - " << e.what());
                }
            }
        }
    }
    LOG_MODULE("MultiConfigManager", "file_watcher_loop", LOG_INFO, "文件监控循环结束");
}

std::time_t MultiConfigManager::get_file_mod_time(const std::string& path) const {
    LOG_MODULE("MultiConfigManager", "get_file_mod_time", LOG_DEBUG, "获取文件修改时间: " << path);
    try {
#ifdef _WIN32
        struct _stat64 file_stat;
        if (_stat64(path.c_str(), &file_stat) == 0) {
            LOG_MODULE("MultiConfigManager", "get_file_mod_time", LOG_DEBUG, "文件修改时间: " << file_stat.st_mtime);
            return file_stat.st_mtime;
        }
#else
        struct stat file_stat;
        if (stat(path.c_str(), &file_stat) == 0) {
            LOG_MODULE("MultiConfigManager", "get_file_mod_time", LOG_DEBUG, "文件修改时间: " << file_stat.st_mtime);
            return file_stat.st_mtime;
        }
#endif
        LOG_MODULE("MultiConfigManager", "get_file_mod_time", LOG_WARN, "无法获取文件修改时间: " << path);
        return 0;
    }
    catch (...) {
        LOG_MODULE("MultiConfigManager", "get_file_mod_time", LOG_ERROR, "获取文件修改时间时发生异常: " << path);
        return 0;
    }
}
