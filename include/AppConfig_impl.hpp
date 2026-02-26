#pragma once

#include "DebugLog.h"
#include "ConfigStructs.h"
#include <iostream>
#include <mutex>
#include <memory>
#include <optional>
#include <functional>
#include <concepts>
#include <map>

// 前置声明
class ConfigManager;

// ============================================
// ConfigValue - 简单类型配置包装器
// ============================================

template<typename T>
class ConfigValue {
private:
    std::shared_ptr<ConfigManager> config_;
    std::string key_path_;
    T default_value_;
    mutable std::optional<T> cached_value_;
    std::function<void(const T&)> change_callback_;
    mutable std::mutex change_mutex_;

public:
    // 默认构造函数
    ConfigValue() = default;
    ~ConfigValue() {
        change_callback_ = nullptr;
    }

    // 带参数的构造函数
    ConfigValue(std::shared_ptr<ConfigManager> config,
        const std::string& key_path,
        T default_value = T{})
        : config_(std::move(config))
        , key_path_(key_path)
        , default_value_(default_value) {
    }

    // 拷贝构造函数
    ConfigValue(const ConfigValue& other)
        : config_(other.config_)
        , key_path_(other.key_path_)
        , default_value_(other.default_value_)
        , cached_value_(other.cached_value_) {
        // change_mutex_ 不能拷贝，使用默认构造
    }

    // 拷贝赋值运算符
    ConfigValue& operator=(const ConfigValue& other) {
        if (this != &other) {
            std::lock_guard<std::mutex> lock1(change_mutex_);
            std::lock_guard<std::mutex> lock2(other.change_mutex_);

            config_ = other.config_;
            key_path_ = other.key_path_;
            default_value_ = other.default_value_;
            cached_value_ = other.cached_value_;
            change_callback_ = other.change_callback_;
        }
        return *this;
    }

    // 移动构造函数
    ConfigValue(ConfigValue&& other) noexcept
        : config_(std::move(other.config_))
        , key_path_(std::move(other.key_path_))
        , default_value_(std::move(other.default_value_))
        , cached_value_(std::move(other.cached_value_))
        , change_callback_(std::move(other.change_callback_)) {
        // change_mutex_ 不能移动，使用默认构造
    }

    // 移动赋值运算符
    ConfigValue& operator=(ConfigValue&& other) noexcept {
        if (this != &other) {
            std::lock_guard<std::mutex> lock(change_mutex_);

            config_ = std::move(other.config_);
            key_path_ = std::move(other.key_path_);
            default_value_ = std::move(other.default_value_);
            cached_value_ = std::move(other.cached_value_);
            change_callback_ = std::move(other.change_callback_);
        }
        return *this;
    }

    // 获取值（带缓存）
    const T& get() const {
        if (!cached_value_.has_value()) {
            if (config_) {
                cached_value_ = config_->get<T>(key_path_, default_value_);
            }
            else {
                cached_value_ = default_value_;
            }
        }
        return cached_value_.value();
    }

    // 设置值
    void set(const T& value) {
        if (config_) {
            if (config_->set(key_path_, value)) {
                cached_value_ = value;
                try {
                    config_->save();  // 自动保存
                }
                catch (const std::exception& e) {
                    LOG_MODULE("AppConfig", "set", LOG_ERROR, "保存配置失败: " << e.what());
                }
                catch (...) {
                    LOG_MODULE("AppConfig", "set", LOG_ERROR, "保存配置失败: 未知异常");
                }
                // 确保回调安全
                if (change_callback_) {
                    try {
                        change_callback_(value);
                    }
                    catch (const std::exception& e) {
                        LOG_MODULE("AppConfig", "set", LOG_ERROR, "配置变更回调失败: " << e.what());
                    }
                    catch (...) {
                        LOG_MODULE("AppConfig", "set", LOG_ERROR, "配置变更回调失败: 未知异常");
                    }
                }
            }
        }
    }

    // 绑定变更回调
    void on_change(std::function<void(const T&)> callback) {
        std::lock_guard<std::mutex> lock(change_mutex_);
        change_callback_ = std::move(callback);
    }

    // 清空缓存
    void invalidate_cache() const {
        cached_value_.reset();
    }

    // 隐式转换
    operator T() const { return get(); }

    // 赋值运算符
    ConfigValue& operator=(const T& value) {
        set(value);
        return *this;
    }

    // 检查是否有有效的 ConfigManager
    bool is_initialized() const {
        return config_ != nullptr;
    }

    // 获取底层 ConfigManager
    std::shared_ptr<ConfigManager> get_config_manager() const {
        return config_;
    }
};

// ============================================
// ConfigObject - 复杂类型配置包装器
// ============================================

template<typename T>
concept ConfigSerializable = requires(T t, nlohmann::json & j) {
    { T::to_json(j, t) } -> std::same_as<void>;
    { T::from_json(j, t) } -> std::same_as<void>;
    { t.validate() } -> std::same_as<bool>;
};

template<ConfigSerializable T>
class ConfigObject {
private:
    std::shared_ptr<ConfigManager> config_;
    std::string key_path_;
    T default_value_;
    mutable std::optional<T> cached_value_;
    std::function<void(const T&)> change_callback_;
    mutable std::mutex change_mutex_;

public:
    // 默认构造函数
    ConfigObject() = default;
    ~ConfigObject() {
        change_callback_ = nullptr;
    }

    // 带参数的构造函数
    ConfigObject(std::shared_ptr<ConfigManager> config,
        const std::string& key_path,
        const T& default_value = T{})
        : config_(std::move(config))
        , key_path_(key_path)
        , default_value_(default_value) {
    }

    // 拷贝构造函数
    ConfigObject(const ConfigObject& other)
        : config_(other.config_)
        , key_path_(other.key_path_)
        , default_value_(other.default_value_)
        , cached_value_(other.cached_value_) {
        // change_mutex_ 不能拷贝，使用默认构造
    }

    // 拷贝赋值运算符
    ConfigObject& operator=(const ConfigObject& other) {
        if (this != &other) {
            std::lock_guard<std::mutex> lock1(change_mutex_);
            std::lock_guard<std::mutex> lock2(other.change_mutex_);

            config_ = other.config_;
            key_path_ = other.key_path_;
            default_value_ = other.default_value_;
            cached_value_ = other.cached_value_;
            change_callback_ = other.change_callback_;
        }
        return *this;
    }

    // 移动构造函数
    ConfigObject(ConfigObject&& other) noexcept
        : config_(std::move(other.config_))
        , key_path_(std::move(other.key_path_))
        , default_value_(std::move(other.default_value_))
        , cached_value_(std::move(other.cached_value_))
        , change_callback_(std::move(other.change_callback_)) {
        // change_mutex_ 不能移动，使用默认构造
    }

    // 移动赋值运算符
    ConfigObject& operator=(ConfigObject&& other) noexcept {
        if (this != &other) {
            std::lock_guard<std::mutex> lock(change_mutex_);

            config_ = std::move(other.config_);
            key_path_ = std::move(other.key_path_);
            default_value_ = std::move(other.default_value_);
            cached_value_ = std::move(other.cached_value_);
            change_callback_ = std::move(other.change_callback_);
        }
        return *this;
    }

    // 获取配置对象
    const T& get() const {
        if (!cached_value_.has_value()) {
            if (config_) {
                auto json_obj = config_->get<nlohmann::json>(key_path_);
                if (json_obj.has_value()) {
                    T obj = {};
                    T::from_json(json_obj.value(), obj);
                    cached_value_ = obj;
                }
                else {
                    cached_value_ = default_value_;
                    // 保存默认值
                    nlohmann::json j;
                    T::to_json(j, default_value_);
                    config_->set(key_path_, j);
                }
            }
            else {
                cached_value_ = default_value_;
            }
        }
        return cached_value_.value();
    }

    // 设置配置对象
    void set(const T& value) {
        if (config_) {
            nlohmann::json j;
            T::to_json(j, value);
            if (config_->set(key_path_, j)) {
                cached_value_ = value;
                try {
                    config_->save();
                }
                catch (const std::exception& e) {
                    LOG_MODULE("AppConfig", "set", LOG_ERROR, "保存配置对象失败: " << e.what());
                }
                catch (...) {
                    LOG_MODULE("AppConfig", "set", LOG_ERROR, "保存配置对象失败: 未知异常");
                }
                if (change_callback_) {
                    try {
                        change_callback_(value);
                    }
                    catch (const std::exception& e) {
                        LOG_MODULE("AppConfig", "set", LOG_ERROR, "配置对象变更回调失败: " << e.what());
                    }
                    catch (...) {
                        LOG_MODULE("AppConfig", "set", LOG_ERROR, "配置对象变更回调失败: 未知异常");
                    }
                }
            }
        }
    }

    // 绑定变更回调
    void on_change(std::function<void(const T&)> callback) {
        std::lock_guard<std::mutex> lock(change_mutex_);
        change_callback_ = std::move(callback);
    }

    // 清空缓存
    void invalidate_cache() const {
        cached_value_.reset();
    }

    // 访问运算符
    const T& operator*() const { return get(); }
    const T* operator->() const { return &get(); }

    // 赋值运算符
    ConfigObject& operator=(const T& value) {
        set(value);
        return *this;
    }

    // 检查是否有有效的 ConfigManager
    bool is_initialized() const {
        return config_ != nullptr;
    }

    // 获取底层 ConfigManager
    std::shared_ptr<ConfigManager> get_config_manager() const {
        return config_;
    }
};

// ============================================
// 自动注册字段
// ============================================

template<typename T>
struct FieldMap {
    static_assert(!std::is_same_v<T, T>, "FieldMap 未针对此类型进行专门化");
};

// 开始定义字段映射
#define BEGIN_FIELD_MAP(Class) \
    template<> struct FieldMap<Class> { \
        using T = Class; \
        static void set(T& obj, const std::string& name, const nlohmann::json& val) {

// 注册一个字段
#define FIELD(Class, Type, Name) \
            if (name == #Name) { \
                obj.Name = val.get<Type>(); \
                return; \
            }

// 结束映射
#define END_FIELD_MAP() \
            throw std::runtime_error("Unknown field: " + name); \
        } \
    };

// ============================================
// 配置构建器（辅助创建配置对象）
// ============================================

template<typename T>
class ConfigBuilder {
private:
    T config_;

public:
    ConfigBuilder() = default;

    // 链式设置方法
    template<typename FieldType>
    ConfigBuilder& set_field(FieldType T::* field_ptr, FieldType value) {
        config_.*field_ptr = value;
        return *this;
    }

    template<typename FieldType>
    ConfigBuilder& set_field(const std::string& field_name, FieldType value) {
        FieldMap<T>::set(config_, field_name, value);
        return *this;
    }

    // 构建配置
    T build() const {
        return config_;
    }

    // 构建并验证
    std::optional<T> build_and_validate() const {
        if (config_.validate()) {
            return config_;
        }
        return std::nullopt;
    }
};

// ============================================
// 配置验证器
// ============================================

template<typename T>
class ConfigValidator {
public:
    using ValidatorFunc = std::function<bool(const T&)>;

    void add_validator(const std::string& rule_name, ValidatorFunc validator) {
        validators_[rule_name] = std::move(validator);
    }

    bool validate(const T& config, std::vector<std::string>& errors) const {
        bool valid = true;
        errors.clear();

        for (const auto& [name, validator] : validators_) {
            if (!validator(config)) {
                errors.push_back(name);
                valid = false;
            }
        }

        return valid;
    }

private:
    std::map<std::string, ValidatorFunc> validators_;
};

// ============================================
// 配置监听器
// ============================================

template<typename T>
class ConfigListener {
public:
    virtual ~ConfigListener() = default;

    virtual void on_config_changed(const std::string& key_path,
        const T& old_value,
        const T& new_value) = 0;

    virtual void on_config_loaded(const T& value) = 0;

    virtual void on_config_saved() = 0;
};

// ============================================
// 配置更新器（批量更新）
// ============================================

template<typename T>
class ConfigUpdater {
public:
    void add_update(const std::string& key_path, const T& value) {
        updates_[key_path] = value;
    }

    template<typename UpdaterFunc>
    void add_update_func(const std::string& key_path, UpdaterFunc func) {
        func_updates_[key_path] = func;
    }

    void clear() {
        updates_.clear();
        func_updates_.clear();
    }

    size_t size() const {
        return updates_.size() + func_updates_.size();
    }

    // 获取更新映射（供ConfigManager使用）
    const auto& get_updates() const { return updates_; }
    const auto& get_func_updates() const { return func_updates_; }

private:
    std::map<std::string, T> updates_;
    std::map<std::string, std::function<T(const T&)>> func_updates_;
};
