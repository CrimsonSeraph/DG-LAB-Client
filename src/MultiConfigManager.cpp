#include "MultiConfigManager.h"

#include "DebugLog.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <map>
#include <set>
#include <sstream>

#ifdef _WIN32
#include <sys/stat.h>
#else
#include <sys/stat.h>
#endif

namespace fs = std::filesystem;

// ============================================
// 公共接口实现（public）
// ============================================

void MultiConfigManager::register_config(const std::string& name,
    const std::string& file_path,
    bool auto_reload) {
    LOG_MODULE("MultiConfigManager", "register_config", LOG_DEBUG,
        "注册配置: " << name << " -> " << file_path << " 自动重载=" << (auto_reload ? "是" : "否"));
    std::lock_guard<std::mutex> lock(registry_mutex_);

    if (config_registry_.find(name) != config_registry_.end()) {
        LOG_MODULE("MultiConfigManager", "register_config", LOG_INFO, "配置已存在，跳过: " << name);
        return;
    }

    ConfigInfo info;
    info.file_path = file_path;
    info.auto_reload = auto_reload;
    info.last_mod_time = get_file_mod_time(file_path);
    info.priority = 0;
    info.manager = std::make_shared<ConfigManager>(file_path);

    config_registry_[name] = std::move(info);
    LOG_MODULE("MultiConfigManager", "register_config", LOG_INFO, "配置注册成功: " << name);
    cache_dirty_ = true;
}

std::shared_ptr<ConfigManager> MultiConfigManager::get_config(const std::string& name) {
    LOG_MODULE("MultiConfigManager", "get_config", LOG_DEBUG, "获取配置: " << name);
    std::lock_guard<std::mutex> lock(registry_mutex_);

    auto it = config_registry_.find(name);
    if (it == config_registry_.end()) {
        LOG_MODULE("MultiConfigManager", "get_config", LOG_ERROR, "配置未注册: " << name);
        throw std::runtime_error("配置未注册: " + name);
    }

    if (!it->second.manager) {
        it->second.manager = std::make_shared<ConfigManager>(it->second.file_path);
        it->second.manager->load();
    }
    else if (it->second.manager->raw().empty()) {
        it->second.manager->load();
    }

    return it->second.manager;
}

bool MultiConfigManager::has_priority_conflict_unsafe(std::string& error_msg) const {
    LOG_MODULE("MultiConfigManager", "has_priority_conflict_unsafe", LOG_DEBUG, "检查优先级冲突");
    std::set<int> priorities;
    std::map<int, std::string> priority_to_name;

    for (const auto& [name, info] : config_registry_) {
        if (info.manager) {
            int priority = info.priority;
            if (priorities.find(priority) != priorities.end()) {
                std::stringstream ss;
                ss << "优先级冲突: 配置 '" << name << "' 和 '" << priority_to_name[priority]
                    << "' 有相同的优先级 " << priority;
                error_msg = ss.str();
                LOG_MODULE("MultiConfigManager", "has_priority_conflict_unsafe", LOG_WARN, error_msg);
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
    std::lock_guard<std::mutex> lock(registry_mutex_);
    return has_priority_conflict_unsafe(error_msg);
}

bool MultiConfigManager::load_all() {
    LOG_MODULE("MultiConfigManager", "load_all", LOG_INFO, "开始加载所有配置");
    std::lock_guard<std::mutex> lock(registry_mutex_);
    bool all_success = true;

    for (auto& [name, info] : config_registry_) {
        LOG_MODULE("MultiConfigManager", "load_all", LOG_DEBUG, "加载配置: " << name);
        if (!info.manager->load()) {
            LOG_MODULE("MultiConfigManager", "load_all", LOG_ERROR, "配置加载失败: " << name);
            all_success = false;
            continue;
        }

        auto priority_opt = info.manager->get<int>("__priority");
        info.priority = priority_opt.value_or(0);
        LOG_MODULE("MultiConfigManager", "load_all", LOG_DEBUG, "配置加载成功，优先级=" << info.priority << " : " << name);
        info.last_mod_time = get_file_mod_time(info.file_path);
    }

    std::string error_msg;
    if (has_priority_conflict_unsafe(error_msg)) {
        LOG_MODULE("MultiConfigManager", "load_all", LOG_WARN, "优先级冲突: " << error_msg);
        all_success = false;
    }

    cache_dirty_ = true;
    LOG_MODULE("MultiConfigManager", "load_all", LOG_INFO, "所有配置加载完成，全部成功=" << (all_success ? "是" : "否"));
    return all_success;
}

bool MultiConfigManager::save_all() {
    LOG_MODULE("MultiConfigManager", "save_all", LOG_INFO, "开始保存所有配置");
    std::lock_guard<std::mutex> lock(registry_mutex_);
    bool all_success = true;

    for (auto& [name, info] : config_registry_) {
        LOG_MODULE("MultiConfigManager", "save_all", LOG_DEBUG, "保存配置: " << name);
        if (!info.manager->save()) {
            LOG_MODULE("MultiConfigManager", "save_all", LOG_ERROR, "配置保存失败: " << name);
            all_success = false;
        }
    }
    LOG_MODULE("MultiConfigManager", "save_all", LOG_INFO, "所有配置保存完成，全部成功=" << (all_success ? "是" : "否"));
    return all_success;
}

bool MultiConfigManager::reload(const std::string& name) {
    LOG_MODULE("MultiConfigManager", "reload", LOG_DEBUG, "重载配置: " << name);
    std::lock_guard<std::mutex> lock(registry_mutex_);

    auto it = config_registry_.find(name);
    if (it == config_registry_.end()) {
        LOG_MODULE("MultiConfigManager", "reload", LOG_ERROR, "配置未注册: " << name);
        return false;
    }

    bool success = it->second.manager->load();
    if (success) {
        it->second.last_mod_time = get_file_mod_time(it->second.file_path);
        cache_dirty_ = true;
        LOG_MODULE("MultiConfigManager", "reload", LOG_INFO, "重载配置成功: " << name);
    }
    else {
        LOG_MODULE("MultiConfigManager", "reload", LOG_ERROR, "重载配置失败: " << name);
    }
    return success;
}

void MultiConfigManager::enable_hot_reload(bool enabled) {
    LOG_MODULE("MultiConfigManager", "enable_hot_reload", LOG_INFO, "设置热重载: " << (enabled ? "开启" : "关闭"));
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
    LOG_MODULE("MultiConfigManager", "get_config_names", LOG_DEBUG, "获取到 " << names.size() << " 个配置名");
    return names;
}

std::vector<std::shared_ptr<ConfigManager>> MultiConfigManager::get_sorted_configs_unsafe() const {
    if (!cache_dirty_ && !sorted_configs_cache_.empty()) {
        return sorted_configs_cache_;
    }

    std::vector<std::pair<int, std::shared_ptr<ConfigManager>>> configs_with_priority;
    for (const auto& [name, info] : config_registry_) {
        if (info.manager) {
            configs_with_priority.emplace_back(info.priority, info.manager);
        }
    }

    std::sort(configs_with_priority.begin(), configs_with_priority.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });

    sorted_configs_cache_.clear();
    for (const auto& [priority, config] : configs_with_priority) {
        sorted_configs_cache_.push_back(config);
    }
    cache_dirty_ = false;
    return sorted_configs_cache_;
}

std::vector<std::shared_ptr<ConfigManager>> MultiConfigManager::get_sorted_configs() const {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    return get_sorted_configs_unsafe();
}

MultiConfigManager::~MultiConfigManager() {
    save_all();
}

// ============================================
// 私有辅助函数实现（private）
// ============================================

void MultiConfigManager::start_file_watcher() {
    LOG_MODULE("MultiConfigManager", "start_file_watcher", LOG_INFO, "启动文件监控线程");
    if (running_) {
        LOG_MODULE("MultiConfigManager", "start_file_watcher", LOG_WARN, "监控线程已在运行");
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
        LOG_MODULE("MultiConfigManager", "stop_file_watcher", LOG_WARN, "监控线程未运行");
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
    static int loop_count = 0;
    const int LOG_INTERVAL = 30;    // 每30次循环（约60秒）记录一次

    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        loop_count++;
        if (loop_count % LOG_INTERVAL == 0) {
            LOG_MODULE("MultiConfigManager", "file_watcher_loop", LOG_DEBUG,
                "监控循环检查（已运行 " << (loop_count * 2) << " 秒）");
        }

        std::lock_guard<std::mutex> lock(registry_mutex_);
        for (auto& [name, info] : config_registry_) {
            if (!info.auto_reload) continue;

            auto current_time = get_file_mod_time(info.file_path);
            if (current_time > info.last_mod_time) {
                LOG_MODULE("MultiConfigManager", "file_watcher_loop", LOG_INFO,
                    "检测到文件变化，重载配置: " << name);
                try {
                    if (info.manager->load()) {
                        auto priority_opt = info.manager->get<int>("__priority");
                        info.priority = priority_opt.value_or(0);
                        info.last_mod_time = current_time;
                        cache_dirty_ = true;
                        LOG_MODULE("MultiConfigManager", "file_watcher_loop", LOG_INFO,
                            "重载成功: " << name);
                    }
                    else {
                        LOG_MODULE("MultiConfigManager", "file_watcher_loop", LOG_ERROR,
                            "重载失败: " << name);
                    }
                }
                catch (const nlohmann::json::parse_error& e) {
                    LOG_MODULE("MultiConfigManager", "file_watcher_loop", LOG_ERROR,
                        "JSON解析错误: " << name << " - " << e.what());
                }
                catch (const std::exception& e) {
                    LOG_MODULE("MultiConfigManager", "file_watcher_loop", LOG_ERROR,
                        "重载异常: " << name << " - " << e.what());
                }
            }
        }
    }
    LOG_MODULE("MultiConfigManager", "file_watcher_loop", LOG_INFO, "文件监控循环结束");
}

std::time_t MultiConfigManager::get_file_mod_time(const std::string& path) const {
    try {
#ifdef _WIN32
        struct _stat64 file_stat;
        if (_stat64(path.c_str(), &file_stat) == 0) {
            return file_stat.st_mtime;
        }
#else
        struct stat file_stat;
        if (stat(path.c_str(), &file_stat) == 0) {
            return file_stat.st_mtime;
        }
#endif
        LOG_MODULE("MultiConfigManager", "get_file_mod_time", LOG_WARN, "无法获取文件修改时间: " << path);
        return 0;
    }
    catch (...) {
        LOG_MODULE("MultiConfigManager", "get_file_mod_time", LOG_ERROR, "获取修改时间异常: " << path);
        return 0;
    }
}
