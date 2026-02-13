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
    , app_name_()      // 调用 ConfigValue<std::string> 的默认构造函数
    , app_version_()   // 调用 ConfigValue<std::string> 的默认构造函数
    , debug_mode_()    // 调用 ConfigValue<bool> 的默认构造函数
    , log_level_()     // 调用 ConfigValue<int> 的默认构造函数
    //, xxx_config_()     调用 ConfigObject<XXXConfig> 的默认构造函数
{
    // 延迟初始化
}

AppConfig::~AppConfig() {
}

// ============================================
// 初始化方法
// ============================================

bool AppConfig::initialize(const std::string& config_dir) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (initialized_) {
        std::cerr << "配置系统已经初始化" << std::endl;
        return true;
    }

    // 检查是否正在关闭
    static std::atomic<bool> is_shutting_down{ false };
    if (is_shutting_down) {
        std::cerr << "系统正在关闭，跳过初始化" << std::endl;
        return false;
    }

    try {
        std::string actual_config_dir = config_dir; // 创建可修改的副本

        // 确保目录存在
        std::error_code ec;
        if (!fs::exists(actual_config_dir, ec)) {
            if (!fs::create_directories(actual_config_dir, ec)) {
                std::cerr << "无法创建配置目录: " << actual_config_dir
                    << " 错误: " << ec.message() << std::endl;
                // 使用临时目录作为后备
                auto temp_dir = fs::temp_directory_path() / "DG-LAB-Client";
                fs::create_directories(temp_dir, ec);
                actual_config_dir = temp_dir.string();
                std::cout << "使用临时目录: " << actual_config_dir << std::endl;
            }
        }

        // 创建多配置管理器
        multi_config_ = &MultiConfigManager::instance();

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
                    std::ofstream file(path);
                    if (file.is_open()) {
                        nlohmann::json default_config;
                        default_config["__priority"] = priority;
                        file << default_config.dump(4);
                        file.close();
                        std::cout << "创建配置文件: " << path << std::endl;
                    }
                }

                // 注册配置
                multi_config_->register_config(name, path);
                std::cout << "注册配置: " << name << " -> " << path << " (优先级: " << priority << ")" << std::endl;

                // 立即获取并设置优先级
                auto config_manager = multi_config_->get_config(name);
                if (config_manager) {
                    config_manager->set("__priority", priority);
                }
            }
            catch (const std::exception& e) {
                std::cerr << "注册配置 " << name << " 失败: " << e.what() << std::endl;
            }
        }

        // 尝试加载所有配置
        try {
            if (!multi_config_->load_all()) {
                std::cerr << "配置加载失败，使用默认配置" << std::endl;
                // 创建默认配置
                create_default_configs();
            }
        }
        catch (const std::exception& e) {
            std::cerr << "加载配置时异常: " << e.what() << std::endl;
            // 创建默认配置
            create_default_configs();
        }

        // 获取配置管理器引用
        try {
            main_config_ = multi_config_->get_config("main");
            user_config_ = multi_config_->get_config("user");
            system_config_ = multi_config_->get_config("system");
        }
        catch (const std::exception& e) {
            std::cerr << "获取配置管理器失败: " << e.what() << std::endl;
            return false;
        }

        // 重新初始化配置项
        initialize_configs_unsafe();

        // 设置监听器
        setup_listeners();

        initialized_ = true;
        std::cout << "配置系统初始化完成" << std::endl;
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "配置系统初始化失败: " << e.what() << std::endl;

        // 即使初始化失败，也设置一些默认值
        app_name_ = ConfigValue<std::string>(nullptr, "", "DG-LAB-Client");
        app_version_ = ConfigValue<std::string>(nullptr, "", "1.0.0");
        debug_mode_ = ConfigValue<bool>(nullptr, "", false);
        log_level_ = ConfigValue<int>(nullptr, "", 2);

        initialized_ = true; // 仍然标记为已初始化，但使用内存配置
        return false;
    }
}

void AppConfig::shutdown() {
    std::unique_lock<std::mutex> lock(mutex_, std::defer_lock);

    // 尝试锁定，如果失败则跳过
    if (lock.try_lock()) {
        if (!initialized_) {
            return;
        }

        // 先停止所有活动
        if (multi_config_) {
            multi_config_->enable_hot_reload(false);
        }

        // 保存配置（如果可能）
        try {
            if (multi_config_) {
                multi_config_->save_all();
            }
        }
        catch (const std::exception& e) {
            std::cerr << "保存配置失败: " << e.what() << std::endl;
        }
        catch (...) {
            std::cerr << "保存配置失败: 未知异常" << std::endl;
        }

        // 清空监听器
        config_listeners_.clear();

        // 重置配置项（先清空回调，避免析构时调用）
        app_name_.on_change(nullptr);
        app_version_.on_change(nullptr);
        debug_mode_.on_change(nullptr);
        log_level_.on_change(nullptr);

        // 重置指针
        multi_config_ = nullptr;
        main_config_.reset();
        user_config_.reset();
        system_config_.reset();

        initialized_ = false;
        std::cout << "配置系统已关闭" << std::endl;
    }
    else {
        std::cerr << "警告: 无法获取锁进行清理，跳过配置关闭" << std::endl;
    }
}

bool AppConfig::is_initialized() const {
    return main_config_ != nullptr &&
        user_config_ != nullptr &&
        system_config_ != nullptr;
}

bool AppConfig::check_priority_conflict(std::string& error_msg) const {
    if (multi_config_) {
        return multi_config_->has_priority_conflict(error_msg);
    }
    return false;
}

void AppConfig::initialize_configs() {
    std::unique_lock<std::mutex> lock(mutex_, std::defer_lock);
    initialize_configs_unsafe();
}

void AppConfig::initialize_configs_unsafe() {
    // 确保已经初始化
    if (!is_initialized()) {
        throw std::runtime_error("MultiConfigManager 未初始化");
    }

    // 构造配置项
    app_name_ = ConfigValue<std::string>(main_config_, "", get_value_unsafe<std::string>("app.name", "DG-LAB-Client"));
    app_version_ = ConfigValue<std::string>(main_config_, "", get_value_unsafe<std::string>("app.version", "1.0.0"));
    debug_mode_ = ConfigValue<bool>(main_config_, "", get_value_unsafe<bool>("app.debug", false));
    log_level_ = ConfigValue<int>(main_config_, "", get_value_unsafe<int>("app.log_level", 2));

     //xxx_config_ = ConfigObject<XXXConfig>(xxx_config_, "xxx", 
     //    XXXConfig{
     //        .nmae = get_value_unsafe<T>("name", default),
     //        // ...
     //    }
     //);
}

void AppConfig::create_default_configs() {
    if (main_config_) {
        main_config_->set("app.name", "DG-LAB-Client");
        main_config_->set("app.version", "1.0.0");
        main_config_->set("app.debug", false);
        main_config_->set("app.log_level", 2);
        main_config_->set("__priority", 0);
        main_config_->save();
    }
}

// ============================================
// 配置项访问器实现
// ============================================

const std::string& AppConfig::get_app_name() const {
    std::lock_guard<std::mutex> lock(mutex_);

    static const std::string empty = "";
    if (!is_initialized() || !is_called_from_main_thread()) {
        return empty;
    }
    return get_app_name_unsafe();
}

const std::string& AppConfig::get_app_name_unsafe() const {
    if (!is_initialized()) {
        throw std::runtime_error("配置未初始化");
    }

    // 使用带优先级的查找
    auto value = multi_config_->get_unsafe<std::string>("app.name");
    if (value.has_value()) {
        static thread_local std::string cached_value;
        cached_value = value.value();
        return cached_value;
    }

    static const std::string default_value = "DG-LAB-Client";
    return default_value;
}

const std::string& AppConfig::get_app_version() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!is_initialized()) {
        throw std::runtime_error("配置未初始化");
    }
    return app_version_.get();
}

bool AppConfig::is_debug_mode() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!multi_config_) {
        return false;
    }

    auto value = multi_config_->get<bool>("app.debug");
    return value.has_value() ? value.value() : false;
}

int AppConfig::get_log_level() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!multi_config_) {
        return 2;
    }

    auto value = multi_config_->get<int>("app.log_level");
    return value.has_value() ? value.value() : 2;
}

// ============================================
// 配置项修改器实现
// ============================================


void AppConfig::set_app_name(const std::string& name) {
    set_value_with_priority<std::string>("app.name", name, -1);
}

void AppConfig::set_debug_mode(bool enabled) {
    set_value_with_priority<bool>("app.debug", enabled, -1);
}

void AppConfig::set_log_level(int level) {
    set_value_with_priority<int>("app.log_level", level, -1);
}

void AppConfig::set_value(const std::string& key_path, const std::string& value) {
    set_value_with_priority<std::string>(key_path, value, -1);
}

void AppConfig::set_app_name_with_priority(const std::string& name, int priority) {
    set_value_with_priority<std::string>("app.name", name, priority);
}

void AppConfig::set_debug_mode_with_priority(const bool enable, int priority) {
    set_value_with_priority<bool>("app.debug", enable, priority);
}

void AppConfig::set_log_level_with_priority(int level, int priority) {
    set_value_with_priority<int>("app.log_level", level, priority);
}

void AppConfig::set_value_with_priority(const std::string& key_path, const std::string& value, int priority) {
    set_value_with_priority<std::string>(key_path, value, priority);
}

void AppConfig::set_app_name_with_name(const std::string& name, const std::string& key_name) {
    set_value_with_name<std::string>("app.name", name, key_name);
}

void AppConfig::set_debug_mode_with_name(const bool enable, const std::string& key_name) {
    set_value_with_name<bool>("app.debug", enable, key_name);
}

void AppConfig::set_log_level_with_name(int level, const std::string& key_name) {
    set_value_with_name<int>("app.log_level", level, key_name);
}

void AppConfig::set_value_with_name(const std::string& key_path, const std::string& value, const std::string& key_name) {
    set_value_with_name<std::string>(key_path, value, key_name);
}

// ============================================
// 批量操作实现
// ============================================

bool AppConfig::save_all() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!multi_config_) {
        std::cerr << "配置系统未初始化" << std::endl;
        return false;
    }

    try {
        bool success = multi_config_->save_all();
        if (success) {
            notify_config_changed("all");
        }
        return success;
    }
    catch (const std::exception& e) {
        std::cerr << "保存配置失败: " << e.what() << std::endl;
        return false;
    }
}

void AppConfig::reload_all() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!multi_config_) {
        std::cerr << "配置系统未初始化" << std::endl;
        return;
    }

    try {
        multi_config_->load_all();
        invalidate_caches();
        notify_config_changed("all");
        std::cout << "配置重新加载成功" << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "重新加载配置失败: " << e.what() << std::endl;
    }
}

// ============================================
// 配置监听实现
// ============================================

void AppConfig::setup_listeners() {
    if (!main_config_) return;

    // 主配置变更监听
    main_config_->add_listener([this](const nlohmann::json&) {
        invalidate_caches();
        notify_config_changed("main");
        });

    // 用户配置变更监听
    if (user_config_) {
        user_config_->add_listener([this](const nlohmann::json&) {
            invalidate_caches();
            notify_config_changed("user");
            });
    }

    // 系统配置变更监听
    if (system_config_) {
        system_config_->add_listener([this](const nlohmann::json&) {
            invalidate_caches();
            notify_config_changed("system");
            });
    }
}

void AppConfig::add_config_listener(const std::string& config_name,
    std::function<void()> listener) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_listeners_[config_name].push_back(std::move(listener));
}

void AppConfig::remove_config_listener(const std::string& config_name,
    std::function<void()> listener) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = config_listeners_.find(config_name);
    if (it != config_listeners_.end()) {
        auto& listeners = it->second;
        listeners.erase(
            std::remove_if(listeners.begin(), listeners.end(),
                [&listener](const auto& func) {
                    // 比较函数对象（注意：这方法不完美）
                    return func.target_type() == listener.target_type();
                }),
            listeners.end()
        );
    }
}

void AppConfig::notify_config_changed(const std::string& config_name) {
    // 通知特定配置的监听器
    auto it = config_listeners_.find(config_name);
    if (it != config_listeners_.end()) {
        for (const auto& listener : it->second) {
            try {
                listener();
            }
            catch (const std::exception& e) {
                std::cerr << "配置监听器错误: " << e.what() << std::endl;
            }
        }
    }

    // 通知全局监听器
    auto global_it = config_listeners_.find("all");
    if (global_it != config_listeners_.end()) {
        for (const auto& listener : global_it->second) {
            try {
                listener();
            }
            catch (const std::exception& e) {
                std::cerr << "全局配置监听器错误: " << e.what() << std::endl;
            }
        }
    }
}

void AppConfig::invalidate_caches() {
    if (!is_initialized()) return;
    app_name_.invalidate_cache();
    app_version_.invalidate_cache();
    debug_mode_.invalidate_cache();
    log_level_.invalidate_cache();

    //xxx_config_.invalidate_cache();
}

// ============================================
// 配置验证实现
// ============================================

bool AppConfig::validate_configs() const {
    std::vector<std::string> errors;
    return validate_all(errors);
}

bool AppConfig::is_called_from_main_thread() const {
    static std::thread::id main_thread_id = std::this_thread::get_id();
    return std::this_thread::get_id() == main_thread_id;
}

bool AppConfig::validate_all(std::vector<std::string>& errors) const {
    std::lock_guard<std::mutex> lock(mutex_);
    errors.clear();
    bool valid = true;

    // 验证简单配置项
    if (app_name_.get().empty()) {
        errors.push_back("应用名称不能为空");
        valid = false;
    }

    if (log_level_.get() < 0 || log_level_.get() > 5) {
        errors.push_back("日志级别必须在0-5之间");
        valid = false;
    }

    // 验证复杂配置项
    //if (!xxx_config_.get().validate()) {
    //    errors.push_back("XXX配置无效");
    //    valid = false;
    //}

    return valid;
}

bool AppConfig::validate_config(const std::string& config_name,
    std::vector<std::string>& errors) const {
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

    return true;
}

// ============================================
// 高级操作实现
// ============================================

std::shared_ptr<ConfigManager> AppConfig::get_config_manager(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!multi_config_) {
        return nullptr;
    }

    try {
        return multi_config_->get_config(name);
    }
    catch (const std::exception& e) {
        std::cerr << "获取配置管理器失败: " << e.what() << std::endl;
        return nullptr;
    }
}

std::vector<std::string> AppConfig::get_all_config_names() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> names;
    if (multi_config_) {
        names = multi_config_->get_config_names();
    }

    return names;
}

bool AppConfig::has_config(const std::string& name) const {
    auto names = get_all_config_names();
    return std::find(names.begin(), names.end(), name) != names.end();
}

bool AppConfig::export_config(const std::string& name, const std::string& file_path) const {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        nlohmann::json config_json;

        //if (name == "xxx") {
        //    XXXConfig::to_json(config_json, xxx_config_.get());
        //}
        //else {
        //    std::cerr << "未知配置项: " << name << std::endl;
        //    return false;
        //}

        std::ofstream file(file_path);
        if (!file.is_open()) {
            std::cerr << "无法打开文件: " << file_path << std::endl;
            return false;
        }

        file << config_json.dump(4);
        std::cout << "配置导出成功: " << name << " -> " << file_path << std::endl;
        return true;

    }
    catch (const std::exception& e) {
        std::cerr << "导出配置失败: " << e.what() << std::endl;
        return false;
    }
}

bool AppConfig::import_config(const std::string& name, const std::string& file_path) {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        std::ifstream file(file_path);
        if (!file.is_open()) {
            std::cerr << "无法打开文件: " << file_path << std::endl;
            return false;
        }

        nlohmann::json config_json = nlohmann::json::parse(file);

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
        //    std::cerr << "未知配置项: " << name << std::endl;
        //    return false;
        //}

        save_all();
        std::cout << "配置导入成功: " << file_path << " -> " << name << std::endl;
        return true;

    }
    catch (const std::exception& e) {
        std::cerr << "导入配置失败: " << e.what() << std::endl;
        return false;
    }
}
