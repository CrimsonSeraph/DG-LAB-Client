#include "AppConfig.h"
#include "MultiConfigManager.h"
#include "ConfigManager.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

// ============================================
// 单例实例
// ============================================

AppConfig& AppConfig::instance() {
    static AppConfig instance;
    DebugLog::Instance().set_log_level("AppConfig", LOG_DEBUG);
    return instance;
}

// ============================================
// 构造函数和析构函数
// ============================================

AppConfig::AppConfig()
    : multi_config_(nullptr)
    , main_config_(nullptr)
    , user_config_(nullptr)
    , system_config_(nullptr)
    // 配置项使用默认构造，不需要传递参数
    , main_config_obj_()
    //, xxx_config_()     调用 ConfigObject<XXXConfig> 的默认构造函数
{
    LOG_MODULE("AppConfig", "AppConfig", LOG_DEBUG, "AppConfig 构造函数被调用");
    // 延迟初始化
}

AppConfig::~AppConfig() {
    LOG_MODULE("AppConfig", "~AppConfig", LOG_DEBUG, "AppConfig 析构函数被调用");
}

// ============================================
// 初始化方法
// ============================================

bool AppConfig::initialize(const std::string& config_dir) {
    LOG_MODULE("AppConfig", "initialize", LOG_INFO, "开始初始化配置系统，配置目录: " << config_dir);
    std::lock_guard<std::mutex> lock(mutex_);

    if (initialized_) {
        LOG_MODULE("AppConfig", "initialize", LOG_WARN, "配置系统已经初始化，跳过");
        return true;
    }

    // 检查是否正在关闭
    static std::atomic<bool> is_shutting_down{ false };
    if (is_shutting_down) {
        LOG_MODULE("AppConfig", "initialize", LOG_WARN, "系统正在关闭，跳过初始化");
        return false;
    }

    try {
        std::string actual_config_dir = config_dir; // 创建可修改的副本

        // 确保目录存在
        std::error_code ec;
        if (!fs::exists(actual_config_dir, ec)) {
            LOG_MODULE("AppConfig", "initialize", LOG_INFO, "配置目录不存在，尝试创建: " << actual_config_dir);
            if (!fs::create_directories(actual_config_dir, ec)) {
                LOG_MODULE("AppConfig", "initialize", LOG_ERROR, "无法创建配置目录: " << actual_config_dir
                    << " 错误: " << ec.message());
                // 使用临时目录作为后备
                auto temp_dir = fs::temp_directory_path() / "DG-LAB-Client";
                fs::create_directories(temp_dir, ec);
                actual_config_dir = temp_dir.string();
                LOG_MODULE("AppConfig", "initialize", LOG_INFO, "使用临时目录: " << actual_config_dir);
            }
        }

        // 创建多配置管理器
        multi_config_ = &MultiConfigManager::instance();
        LOG_MODULE("AppConfig", "initialize", LOG_DEBUG, "获取 MultiConfigManager 单例成功");

        // 注册配置文件时设置不同的优先级
        std::vector<std::tuple<std::string, std::string, int>> configs = {
            {"main", actual_config_dir + "/main.json", 0},
            {"system", actual_config_dir + "/system.json", 1},
            {"user", actual_config_dir + "/user.json", 2}
        };

        for (auto& [name, path, priority] : configs) {
            try {
                // 确保文件存在
                if (!fs::exists(path)) {
                    LOG_MODULE("AppConfig", "initialize", LOG_INFO, "配置文件不存在，创建默认文件: " << path);
                    std::ofstream file(path);
                    if (file.is_open()) {
                        nlohmann::json default_config;
                        default_config["__priority"] = priority;
                        file << default_config.dump(4);
                        file.close();
                        LOG_MODULE("AppConfig", "initialize", LOG_INFO, "创建配置文件成功: " << path);
                    }
                    else {
                        LOG_MODULE("AppConfig", "initialize", LOG_WARN, "无法创建配置文件: " << path);
                    }
                }

                // 注册配置
                multi_config_->register_config(name, path);
                LOG_MODULE("AppConfig", "initialize", LOG_INFO, "注册配置: " << name << " -> " << path << " (优先级: " << priority << ")");

                // 立即获取并设置优先级
                auto config_manager = multi_config_->get_config(name);
                if (config_manager) {
                    config_manager->set("__priority", priority);
                    LOG_MODULE("AppConfig", "initialize", LOG_DEBUG, "设置配置优先级: " << name << " = " << priority);
                }
            }
            catch (const std::exception& e) {
                LOG_MODULE("AppConfig", "initialize", LOG_ERROR, "注册配置 " << name << " 失败: " << e.what());
            }
        }

        // 尝试加载所有配置
        try {
            LOG_MODULE("AppConfig", "initialize", LOG_DEBUG, "开始加载所有配置");
            if (!multi_config_->load_all()) {
                LOG_MODULE("AppConfig", "initialize", LOG_WARN, "配置加载失败，将使用默认配置");
                // 创建默认配置
                create_default_configs();
            }
            else {
                LOG_MODULE("AppConfig", "initialize", LOG_INFO, "所有配置加载成功");
            }
        }
        catch (const std::exception& e) {
            LOG_MODULE("AppConfig", "initialize", LOG_ERROR, "加载配置时异常: " << e.what());
            // 创建默认配置
            create_default_configs();
        }

        // 获取配置管理器引用
        try {
            main_config_ = multi_config_->get_config("main");
            user_config_ = multi_config_->get_config("user");
            system_config_ = multi_config_->get_config("system");
            LOG_MODULE("AppConfig", "initialize", LOG_DEBUG, "获取配置管理器引用成功");
        }
        catch (const std::exception& e) {
            LOG_MODULE("AppConfig", "initialize", LOG_ERROR, "获取配置管理器失败: " << e.what());
            return false;
        }

        // 重新初始化配置项
        initialize_configs_unsafe();

        // 设置监听器
        setup_listeners();

        initialized_ = true;
        LOG_MODULE("AppConfig", "initialize", LOG_INFO, "配置系统初始化完成");
        return true;
    }
    catch (const std::exception& e) {
        LOG_MODULE("AppConfig", "initialize", LOG_ERROR, "配置系统初始化失败: " << e.what());

        // 即使初始化失败，也设置一些默认值
        main_config_obj_ = ConfigObject<MainConfig>(main_config_, "main",
            MainConfig{
                .app_name_ = get_value_unsafe<std::string>("app.name", "DG-LAB-Client"),
                .app_version_ = get_value_unsafe<std::string>("app.version", "1.0.0"),
                .debug_mode_ = get_value_unsafe<bool>("app.debug", false),
                .log_level_ = get_value_unsafe<int>("app.log.level", 2),
                .is_only_type_info_ = get_value_unsafe<bool>("app.log.only_type_info", false),
                .python_path_ = get_value_unsafe<std::string>("python.path", "python")
            });

        initialized_ = true; // 仍然标记为已初始化，但使用内存配置
        LOG_MODULE("AppConfig", "initialize", LOG_WARN, "使用内存默认配置，系统将继续运行");
        return false;
    }
}

void AppConfig::shutdown() {
    LOG_MODULE("AppConfig", "shutdown", LOG_INFO, "开始关闭配置系统");
    std::unique_lock<std::mutex> lock(mutex_, std::defer_lock);

    // 尝试锁定，如果失败则跳过
    if (lock.try_lock()) {
        if (!initialized_) {
            LOG_MODULE("AppConfig", "shutdown", LOG_DEBUG, "配置系统未初始化，无需关闭");
            return;
        }

        // 先停止所有活动
        if (multi_config_) {
            multi_config_->enable_hot_reload(false);
            LOG_MODULE("AppConfig", "shutdown", LOG_DEBUG, "已禁用热重载");
        }

        // 保存配置（如果可能）
        try {
            if (multi_config_) {
                multi_config_->save_all();
                LOG_MODULE("AppConfig", "shutdown", LOG_INFO, "所有配置已保存");
            }
        }
        catch (const std::exception& e) {
            LOG_MODULE("AppConfig", "shutdown", LOG_ERROR, "保存配置失败: " << e.what());
        }
        catch (...) {
            LOG_MODULE("AppConfig", "shutdown", LOG_ERROR, "保存配置失败: 未知异常");
        }

        // 清空监听器
        config_listeners_.clear();
        LOG_MODULE("AppConfig", "shutdown", LOG_DEBUG, "配置监听器已清空");

        // 重置配置项（先清空回调，避免析构时调用）
        main_config_obj_ = ConfigObject<MainConfig>();

        // 重置指针
        multi_config_ = nullptr;
        main_config_.reset();
        user_config_.reset();
        system_config_.reset();

        initialized_ = false;
        LOG_MODULE("AppConfig", "shutdown", LOG_INFO, "配置系统已关闭");
    }
    else {
        LOG_MODULE("AppConfig", "shutdown", LOG_WARN, "无法获取锁进行清理，跳过配置关闭");
    }
}

bool AppConfig::is_initialized() const {
    bool init = main_config_ != nullptr && user_config_ != nullptr && system_config_ != nullptr;
    LOG_MODULE("AppConfig", "is_initialized", LOG_DEBUG, "检查初始化状态: " << (init ? "已初始化" : "未初始化"));
    return init;
}

bool AppConfig::check_priority_conflict(std::string& error_msg) const {
    LOG_MODULE("AppConfig", "check_priority_conflict", LOG_DEBUG, "检查优先级冲突");
    if (multi_config_) {
        return multi_config_->has_priority_conflict(error_msg);
    }
    LOG_MODULE("AppConfig", "check_priority_conflict", LOG_WARN, "MultiConfigManager 未初始化，无法检查优先级冲突");
    return false;
}

void AppConfig::initialize_configs() {
    LOG_MODULE("AppConfig", "initialize_configs", LOG_DEBUG, "调用 initialize_configs");
    std::unique_lock<std::mutex> lock(mutex_, std::defer_lock);
    initialize_configs_unsafe();
}

void AppConfig::initialize_configs_unsafe() {
    LOG_MODULE("AppConfig", "initialize_configs_unsafe", LOG_DEBUG, "开始无锁初始化配置项");
    // 确保已经初始化
    if (!is_initialized()) {
        LOG_MODULE("AppConfig", "initialize_configs_unsafe", LOG_ERROR, "MultiConfigManager 未初始化，无法初始化配置项");
        throw std::runtime_error("MultiConfigManager 未初始化");
    }

    // 构造配置项
    main_config_obj_ = ConfigObject<MainConfig>(main_config_, "main",
        MainConfig{
            .app_name_ = get_value_unsafe<std::string>("app.name", "DG-LAB-Client"),
            .app_version_ = get_value_unsafe<std::string>("app.version", "1.0.0"),
            .debug_mode_ = get_value_unsafe<bool>("app.debug", false),
            .log_level_ = get_value_unsafe<int>("app.log.level", 2),
            .is_only_type_info_ = get_value_unsafe<bool>("app.log.only_type_info", false),
            .python_path_ = get_value_unsafe<std::string>("python.path", "python")
        });

    //xxx_config_ = ConfigObject<XXXConfig>(xxx_config_, "xxx", 
    //    XXXConfig{
    //        .nmae = get_value_unsafe<T>("name", default),
    //        // ...
    //    }
    //);
    LOG_MODULE("AppConfig", "initialize_configs_unsafe", LOG_DEBUG, "配置项初始化完成");
}

void AppConfig::create_default_configs() {
    LOG_MODULE("AppConfig", "create_default_configs", LOG_INFO, "创建默认配置");
    if (main_config_) {
        main_config_->set("app.name", "DG-LAB-Client");
        main_config_->set("app.version", "1.0.0");
        main_config_->set("app.debug", false);
        main_config_->set("app.log_level", 2);
        main_config_->set("__priority", 0);
        main_config_->set("python.path", "python");
        main_config_->save();
        LOG_MODULE("AppConfig", "create_default_configs", LOG_INFO, "默认配置已写入 main.json");
    }
    else {
        LOG_MODULE("AppConfig", "create_default_configs", LOG_WARN, "main_config_ 为空，无法创建默认配置");
    }
}

// ============================================
// 配置项修改器实现
// ============================================

void AppConfig::set_value(const std::string& key_path, const std::string& value) {
    LOG_MODULE("AppConfig", "set_value", LOG_INFO, "设置配置值: " << key_path << " = " << value);
    set_value_with_priority<std::string>(key_path, value, -1);
}

void AppConfig::set_value_with_priority(const std::string& key_path, const std::string& value, int priority) {
    LOG_MODULE("AppConfig", "set_value_with_priority", LOG_INFO, "设置配置值（带优先级）: " << key_path << " = " << value << " 优先级=" << priority);
    set_value_with_priority<std::string>(key_path, value, priority);
}

void AppConfig::set_value_with_name(const std::string& key_path, const std::string& value, const std::string& key_name) {
    LOG_MODULE("AppConfig", "set_value_with_name", LOG_INFO, "设置配置值（带配置名）: " << key_path << " = " << value << " 配置名=" << key_name);
    set_value_with_name<std::string>(key_path, value, key_name);
}

// ============================================
// 批量操作实现
// ============================================

bool AppConfig::save_all() {
    LOG_MODULE("AppConfig", "save_all", LOG_INFO, "开始保存所有配置");
    std::lock_guard<std::mutex> lock(mutex_);

    if (!multi_config_) {
        LOG_MODULE("AppConfig", "save_all", LOG_ERROR, "配置系统未初始化，无法保存");
        return false;
    }

    try {
        bool success = multi_config_->save_all();
        if (success) {
            LOG_MODULE("AppConfig", "save_all", LOG_INFO, "所有配置保存成功");
            notify_config_changed("all");
        }
        else {
            LOG_MODULE("AppConfig", "save_all", LOG_WARN, "部分配置保存失败");
        }
        return success;
    }
    catch (const std::exception& e) {
        LOG_MODULE("AppConfig", "save_all", LOG_ERROR, "保存配置时异常: " << e.what());
        return false;
    }
}

void AppConfig::reload_all() {
    LOG_MODULE("AppConfig", "reload_all", LOG_INFO, "开始重新加载所有配置");
    std::lock_guard<std::mutex> lock(mutex_);

    if (!multi_config_) {
        LOG_MODULE("AppConfig", "reload_all", LOG_ERROR, "配置系统未初始化，无法重新加载");
        return;
    }

    try {
        multi_config_->load_all();
        invalidate_caches();
        notify_config_changed("all");
        LOG_MODULE("AppConfig", "reload_all", LOG_INFO, "配置重新加载成功");
    }
    catch (const std::exception& e) {
        LOG_MODULE("AppConfig", "reload_all", LOG_ERROR, "重新加载配置失败: " << e.what());
    }
}

// ============================================
// 配置监听实现
// ============================================

void AppConfig::setup_listeners() {
    LOG_MODULE("AppConfig", "setup_listeners", LOG_DEBUG, "开始设置配置监听器");
    if (!main_config_) {
        LOG_MODULE("AppConfig", "setup_listeners", LOG_WARN, "main_config_ 为空，跳过设置监听器");
        return;
    }

    // 主配置变更监听
    main_config_->add_listener([this](const nlohmann::json&) {
        LOG_MODULE("AppConfig", "setup_listeners", LOG_DEBUG, "主配置变更，失效缓存并通知");
        invalidate_caches();
        notify_config_changed("main");
        });
    LOG_MODULE("AppConfig", "setup_listeners", LOG_DEBUG, "主配置监听器已添加");

    // 用户配置变更监听
    if (user_config_) {
        user_config_->add_listener([this](const nlohmann::json&) {
            LOG_MODULE("AppConfig", "setup_listeners", LOG_DEBUG, "用户配置变更，失效缓存并通知");
            invalidate_caches();
            notify_config_changed("user");
            });
        LOG_MODULE("AppConfig", "setup_listeners", LOG_DEBUG, "用户配置监听器已添加");
    }

    // 系统配置变更监听
    if (system_config_) {
        system_config_->add_listener([this](const nlohmann::json&) {
            LOG_MODULE("AppConfig", "setup_listeners", LOG_DEBUG, "系统配置变更，失效缓存并通知");
            invalidate_caches();
            notify_config_changed("system");
            });
        LOG_MODULE("AppConfig", "setup_listeners", LOG_DEBUG, "系统配置监听器已添加");
    }
}

void AppConfig::add_config_listener(const std::string& config_name,
    std::function<void()> listener) {
    LOG_MODULE("AppConfig", "add_config_listener", LOG_INFO, "添加配置监听器: " << config_name);
    std::lock_guard<std::mutex> lock(mutex_);
    config_listeners_[config_name].push_back(std::move(listener));
    LOG_MODULE("AppConfig", "add_config_listener", LOG_DEBUG, "监听器添加成功，当前 " << config_name << " 监听器数量: " << config_listeners_[config_name].size());
}

void AppConfig::remove_config_listener(const std::string& config_name,
    std::function<void()> listener) {
    LOG_MODULE("AppConfig", "remove_config_listener", LOG_INFO, "移除配置监听器: " << config_name);
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = config_listeners_.find(config_name);
    if (it != config_listeners_.end()) {
        auto& listeners = it->second;
        size_t before = listeners.size();
        listeners.erase(
            std::remove_if(listeners.begin(), listeners.end(),
                [&listener](const auto& func) {
                    // 比较函数对象（注意：这方法不完美）
                    return func.target_type() == listener.target_type();
                }),
            listeners.end()
        );
        size_t removed = before - listeners.size();
        LOG_MODULE("AppConfig", "remove_config_listener", LOG_DEBUG, "移除了 " << removed << " 个监听器，剩余 " << listeners.size());
    }
    else {
        LOG_MODULE("AppConfig", "remove_config_listener", LOG_WARN, "未找到配置名: " << config_name);
    }
}

void AppConfig::notify_config_changed(const std::string& config_name) {
    LOG_MODULE("AppConfig", "notify_config_changed", LOG_DEBUG, "通知配置变更: " << config_name);
    // 通知特定配置的监听器
    auto it = config_listeners_.find(config_name);
    if (it != config_listeners_.end()) {
        LOG_MODULE("AppConfig", "notify_config_changed", LOG_DEBUG, "触发 " << config_name << " 的 " << it->second.size() << " 个监听器");
        for (const auto& listener : it->second) {
            try {
                listener();
            }
            catch (const std::exception& e) {
                LOG_MODULE("AppConfig", "notify_config_changed", LOG_ERROR, "配置监听器错误: " << e.what());
            }
        }
    }

    // 通知全局监听器
    auto global_it = config_listeners_.find("all");
    if (global_it != config_listeners_.end()) {
        LOG_MODULE("AppConfig", "notify_config_changed", LOG_DEBUG, "触发全局 " << global_it->second.size() << " 个监听器");
        for (const auto& listener : global_it->second) {
            try {
                listener();
            }
            catch (const std::exception& e) {
                LOG_MODULE("AppConfig", "notify_config_changed", LOG_ERROR, "全局配置监听器错误: " << e.what());
            }
        }
    }
}

void AppConfig::invalidate_caches() {
    LOG_MODULE("AppConfig", "invalidate_caches", LOG_DEBUG, "使所有配置缓存失效");
    if (!is_initialized()) {
        LOG_MODULE("AppConfig", "invalidate_caches", LOG_WARN, "配置系统未初始化，跳过缓存失效");
        return;
    }

    main_config_obj_.invalidate_cache();

    //xxx_config_.invalidate_cache();
    LOG_MODULE("AppConfig", "invalidate_caches", LOG_DEBUG, "缓存失效完成");
}

// ============================================
// 配置验证实现
// ============================================

bool AppConfig::validate_configs() const {
    LOG_MODULE("AppConfig", "validate_configs", LOG_DEBUG, "验证所有配置");
    std::vector<std::string> errors;
    bool valid = validate_all(errors);
    if (!valid) {
        for (const auto& err : errors) {
            LOG_MODULE("AppConfig", "validate_configs", LOG_WARN, "配置验证错误: " << err);
        }
    }
    return valid;
}

bool AppConfig::is_called_from_main_thread() const {
    static std::thread::id main_thread_id = std::this_thread::get_id();
    bool main = std::this_thread::get_id() == main_thread_id;
    LOG_MODULE("AppConfig", "is_called_from_main_thread", LOG_DEBUG, "检查是否在主线程: " << (main ? "是" : "否"));
    return main;
}

bool AppConfig::validate_all(std::vector<std::string>& errors) const {
    LOG_MODULE("AppConfig", "validate_all", LOG_DEBUG, "执行全面验证");
    std::lock_guard<std::mutex> lock(mutex_);
    errors.clear();
    bool valid = true;

    // 验证简单配置项
    if (!main_config_obj_.get().validate()) {
        errors.push_back("主配置无效");
        valid = false;
    }

    // 验证复杂配置项
    //if (!xxx_config_.get().validate()) {
    //    errors.push_back("XXX配置无效");
    //    valid = false;
    //}

    LOG_MODULE("AppConfig", "validate_all", LOG_DEBUG, "全面验证结果: " << (valid ? "通过" : "失败") << "，错误数: " << errors.size());
    return valid;
}

bool AppConfig::validate_config(const std::string& config_name,
    std::vector<std::string>& errors) const {
    LOG_MODULE("AppConfig", "validate_config", LOG_DEBUG, "验证指定配置: " << config_name);
    std::lock_guard<std::mutex> lock(mutex_);
    errors.clear();

    //if (config_name == "xxx") {
    //    if (!xxx_config_.get().validate()) {
    //        errors.push_back("XXX配置无效");
    //        return false;
    //    }
    //}
    //else {
    //    errors.push_back("未知配置项: " + config_name);
    //    return false;
    //}

    LOG_MODULE("AppConfig", "validate_config", LOG_DEBUG, "指定配置验证完成: " << config_name);
    return true;
}

// ============================================
// 高级操作实现
// ============================================

std::shared_ptr<ConfigManager> AppConfig::get_config_manager(const std::string& name) {
    LOG_MODULE("AppConfig", "get_config_manager", LOG_DEBUG, "获取配置管理器: " << name);
    std::lock_guard<std::mutex> lock(mutex_);

    if (!multi_config_) {
        LOG_MODULE("AppConfig", "get_config_manager", LOG_ERROR, "MultiConfigManager 未初始化");
        return nullptr;
    }

    try {
        auto mgr = multi_config_->get_config(name);
        LOG_MODULE("AppConfig", "get_config_manager", LOG_DEBUG, "获取配置管理器成功: " << name);
        return mgr;
    }
    catch (const std::exception& e) {
        LOG_MODULE("AppConfig", "get_config_manager", LOG_ERROR, "获取配置管理器失败: " << name << " - " << e.what());
        return nullptr;
    }
}

std::vector<std::string> AppConfig::get_all_config_names() const {
    LOG_MODULE("AppConfig", "get_all_config_names", LOG_DEBUG, "获取所有配置名称");
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> names;
    if (multi_config_) {
        names = multi_config_->get_config_names();
    }

    LOG_MODULE("AppConfig", "get_all_config_names", LOG_DEBUG, "找到 " << names.size() << " 个配置");
    return names;
}

bool AppConfig::has_config(const std::string& name) const {
    LOG_MODULE("AppConfig", "has_config", LOG_DEBUG, "检查配置是否存在: " << name);
    auto names = get_all_config_names();
    bool exists = std::find(names.begin(), names.end(), name) != names.end();
    LOG_MODULE("AppConfig", "has_config", LOG_DEBUG, "配置 " << name << " 存在: " << (exists ? "是" : "否"));
    return exists;
}

bool AppConfig::export_config(const std::string& name, const std::string& file_path) const {
    LOG_MODULE("AppConfig", "export_config", LOG_INFO, "导出配置: " << name << " 到文件: " << file_path);
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        nlohmann::json config_json;

        //if (name == "xxx") {
        //    XXXConfig::to_json(config_json, xxx_config_.get());
        //}
        //else {
        //    LOG_MODULE("AppConfig", LOG_ERROR, "未知配置项: " << name);
        //    return false;
        //}
        // 目前没有实际配置可导出，直接返回失败
        LOG_MODULE("AppConfig", "export_config", LOG_ERROR, "导出功能未实现: " << name);
        return false;

        std::ofstream file(file_path);
        if (!file.is_open()) {
            LOG_MODULE("AppConfig", "export_config", LOG_ERROR, "无法打开文件: " << file_path);
            return false;
        }

        file << config_json.dump(4);
        LOG_MODULE("AppConfig", "export_config", LOG_INFO, "配置导出成功: " << name << " -> " << file_path);
        return true;

    }
    catch (const std::exception& e) {
        LOG_MODULE("AppConfig", "export_config", LOG_ERROR, "导出配置失败: " << e.what());
        return false;
    }
}

bool AppConfig::import_config(const std::string& name, const std::string& file_path) {
    LOG_MODULE("AppConfig", "import_config", LOG_INFO, "导入配置: " << name << " 从文件: " << file_path);
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        std::ifstream file(file_path);
        if (!file.is_open()) {
            LOG_MODULE("AppConfig", "import_config", LOG_ERROR, "无法打开文件: " << file_path);
            return false;
        }

        nlohmann::json config_json = nlohmann::json::parse(file);
        LOG_MODULE("AppConfig", "import_config", LOG_DEBUG, "文件解析成功");

        //if (name == "xxx") {
        //    XXXConfig config;
        //    XXXConfig::from_json(config_json, config);
        //    if (config.validate()) {
        //        xxx_config_ = config;
        //    }
        //    else {
        //        throw std::runtime_error("导入的XXX配置无效");
        //    }
        //}
        //else {
        //    LOG_MODULE("AppConfig", LOG_ERROR, "未知配置项: " << name);
        //    return false;
        //}
        // 目前没有实际配置可导入，直接返回失败
        LOG_MODULE("AppConfig", "import_config", LOG_ERROR, "导入功能未实现: " << name);
        return false;

        save_all();
        LOG_MODULE("AppConfig", "import_config", LOG_INFO, "配置导入成功: " << file_path << " -> " << name);
        return true;

    }
    catch (const std::exception& e) {
        LOG_MODULE("AppConfig", "import_config", LOG_ERROR, "导入配置失败: " << e.what());
        return false;
    }
}
