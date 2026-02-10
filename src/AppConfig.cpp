#include "AppConfig.h"
#include "MultiConfigManager.h"
#include "ConfigManager.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

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
    shutdown();
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

    try {
        // 创建配置目录
        fs::create_directories(config_dir);

        // 创建多配置管理器
        multi_config_ = &MultiConfigManager::instance();

        // 注册配置文件
        multi_config_->register_config("main", config_dir + "/main.json");
        multi_config_->register_config("user", config_dir + "/user.json");
        multi_config_->register_config("system", config_dir + "/system.json");

        // 加载所有配置
        if (!multi_config_->load_all()) {
            std::cerr << "配置加载失败，使用默认配置" << std::endl;
        }

        // 获取配置管理器引用
        main_config_ = multi_config_->get_config("main");
        user_config_ = multi_config_->get_config("user");
        system_config_ = multi_config_->get_config("system");

        // 重新初始化配置项
        initialize_configs();

        // 设置监听器
        setup_listeners();

        // 验证配置
        if (!validate_configs()) {
            std::cerr << "配置验证失败，部分配置可能无效" << std::endl;
        }

        initialized_ = true;
        std::cout << "配置系统初始化成功" << std::endl;
        return true;

    }
    catch (const std::exception& e) {
        std::cerr << "配置系统初始化失败: " << e.what() << std::endl;
        return false;
    }
}

void AppConfig::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return;
    }

    // 保存所有配置
    save_all();

    // 清空监听器
    config_listeners_.clear();

    // 重置配置项
    multi_config_ = nullptr;
    main_config_.reset();
    user_config_.reset();
    system_config_.reset();

    initialized_ = false;
    std::cout << "配置系统已关闭" << std::endl;
}

void AppConfig::initialize_configs() {
    // 确保main_config_已经初始化
    if (!main_config_) {
        if (multi_config_) {
            main_config_ = multi_config_->get_config("main");
        }
        else {
            throw std::runtime_error("MultiConfigManager 未初始化");
        }
    }

    // 重新构造配置项
    app_name_ = ConfigValue<std::string>(main_config_, "app.name", "MyApplication");
    app_version_ = ConfigValue<std::string>(main_config_, "app.version", "1.0.0");
    debug_mode_ = ConfigValue<bool>(main_config_, "app.debug", false);
    log_level_ = ConfigValue<int>(main_config_, "app.log_level", 2);

    // xxx_config_ = ConfigObject<XXXConfig>(main_config_, "xxx", 
    //     XXXConfig{
    //         .nmae = "nmae",
    //         // ...
    //     }
    // );
}

// ============================================
// 配置项访问器实现
// ============================================

const std::string& AppConfig::get_app_name() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!is_initialized()) {
        throw std::runtime_error("配置未初始化");
    }
    return app_name_.get();
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
    if (!is_initialized()) {
        throw std::runtime_error("配置未初始化");
    }
    return debug_mode_.get();
}

int AppConfig::get_log_level() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!is_initialized()) {
        throw std::runtime_error("配置未初始化");
    }
    return log_level_.get();
}

//const XXXConfig& AppConfig::get_xxx_config() const {
//    std::lock_guard<std::mutex> lock(mutex_);
//    return xxx_config_.get();
//}

// ============================================
// 配置项修改器实现
// ============================================

void AppConfig::set_app_name(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    app_name_ = name;
    save_all();
}

void AppConfig::set_debug_mode(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    debug_mode_ = enabled;
    save_all();
}

void AppConfig::set_log_level(int level) {
    std::lock_guard<std::mutex> lock(mutex_);
    log_level_ = level;
    save_all();
}

//void AppConfig::update_xxx_config(const XXXConfig& config) {
//    std::lock_guard<std::mutex> lock(mutex_);
//    xxx_config_ = config;
//    save_all();
//}

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

    // 添加内置配置名称
    //names.push_back("xxx");

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
