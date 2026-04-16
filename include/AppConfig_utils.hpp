/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "ConfigManager.h"
#include "ConfigStructs.h"
#include "DebugLog.h"

#include <nlohmann/json.hpp>

#include <concepts>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

// ============================================
// ConfigValue - 简单类型配置包装器
// ============================================
template<typename T>
class ConfigValue {
public:
    ConfigValue() = default;
    ~ConfigValue() { change_callback_ = nullptr; }

    ConfigValue(std::shared_ptr<ConfigManager> config,
        const std::string& key_path,
        T default_value = T{})
        : config_(std::move(config))
        , key_path_(key_path)
        , default_value_(default_value) {}

    ConfigValue(const ConfigValue& other)
        : config_(other.config_)
        , key_path_(other.key_path_)
        , default_value_(other.default_value_)
        , cached_value_(other.cached_value_) {}

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

    ConfigValue(ConfigValue&& other) noexcept
        : config_(std::move(other.config_))
        , key_path_(std::move(other.key_path_))
        , default_value_(std::move(other.default_value_))
        , cached_value_(std::move(other.cached_value_))
        , change_callback_(std::move(other.change_callback_)) {}

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

    /// @brief 获取值（带缓存）
    const T& get() const {
        if (!cached_value_.has_value()) {
            if (config_) {
                cached_value_ = config_->template get<T>(key_path_, default_value_);
            }
            else {
                cached_value_ = default_value_;
            }
        }
        return cached_value_.value();
    }

    /// @brief 设置值
    void set(const T& value) {
        if (config_) {
            if (config_->set(key_path_, value)) {
                cached_value_ = value;
                try {
                    config_->save();
                }
                catch (const std::exception& e) {
                    LOG_MODULE("ConfigValue", "set", LOG_WARN, "保存配置失败: " << e.what());
                }
                catch (...) {
                    LOG_MODULE("ConfigValue", "set", LOG_WARN, "保存配置失败: 未知异常");
                }
                if (change_callback_) {
                    try {
                        change_callback_(value);
                    }
                    catch (const std::exception& e) {
                        LOG_MODULE("ConfigValue", "set", LOG_ERROR, "配置变更回调失败: " << e.what());
                    }
                    catch (...) {
                        LOG_MODULE("ConfigValue", "set", LOG_ERROR, "配置变更回调失败: 未知异常");
                    }
                }
            }
        }
    }

    /// @brief 绑定变更回调
    void on_change(std::function<void(const T&)> callback) {
        std::lock_guard<std::mutex> lock(change_mutex_);
        change_callback_ = std::move(callback);
    }

    /// @brief 清空缓存
    void invalidate_cache() const { cached_value_.reset(); }

    operator T() const { return get(); }
    ConfigValue& operator=(const T& value) { set(value); return *this; }

    bool is_initialized() const { return config_ != nullptr; }
    std::shared_ptr<ConfigManager> get_config_manager() const { return config_; }

private:
    std::shared_ptr<ConfigManager> config_;
    std::string key_path_;
    T default_value_;
    mutable std::optional<T> cached_value_;
    std::function<void(const T&)> change_callback_;
    mutable std::mutex change_mutex_;
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
public:
    ConfigObject() = default;
    ~ConfigObject() { change_callback_ = nullptr; }

    ConfigObject(std::shared_ptr<ConfigManager> config,
        const std::string& key_path,
        const T& default_value = T{})
        : config_(std::move(config))
        , key_path_(key_path)
        , default_value_(default_value) {}

    ConfigObject(const ConfigObject& other)
        : config_(other.config_)
        , key_path_(other.key_path_)
        , default_value_(other.default_value_)
        , cached_value_(other.cached_value_) {}

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

    ConfigObject(ConfigObject&& other) noexcept
        : config_(std::move(other.config_))
        , key_path_(std::move(other.key_path_))
        , default_value_(std::move(other.default_value_))
        , cached_value_(std::move(other.cached_value_))
        , change_callback_(std::move(other.change_callback_)) {}

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

    /// @brief 获取配置对象
    const T& get() const {
        if (!cached_value_.has_value()) {
            if (config_) {
                auto json_obj = config_->template get<nlohmann::json>(key_path_);
                if (json_obj.has_value()) {
                    T obj = {};
                    T::from_json(json_obj.value(), obj);
                    cached_value_ = obj;
                }
                else {
                    cached_value_ = default_value_;
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

    /// @brief 设置配置对象
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
                    LOG_MODULE("ConfigObject", "set", LOG_WARN, "保存配置对象失败: " << e.what());
                }
                catch (...) {
                    LOG_MODULE("ConfigObject", "set", LOG_WARN, "保存配置对象失败: 未知异常");
                }
                if (change_callback_) {
                    try {
                        change_callback_(value);
                    }
                    catch (const std::exception& e) {
                        LOG_MODULE("ConfigObject", "set", LOG_ERROR, "配置对象变更回调失败: " << e.what());
                    }
                    catch (...) {
                        LOG_MODULE("ConfigObject", "set", LOG_ERROR, "配置对象变更回调失败: 未知异常");
                    }
                }
            }
        }
    }

    void on_change(std::function<void(const T&)> callback) {
        std::lock_guard<std::mutex> lock(change_mutex_);
        change_callback_ = std::move(callback);
    }

    void invalidate_cache() const { cached_value_.reset(); }

    const T& operator*() const { return get(); }
    const T* operator->() const { return &get(); }
    ConfigObject& operator=(const T& value) { set(value); return *this; }

    bool is_initialized() const { return config_ != nullptr; }
    std::shared_ptr<ConfigManager> get_config_manager() const { return config_; }

private:
    std::shared_ptr<ConfigManager> config_;
    std::string key_path_;
    T default_value_;
    mutable std::optional<T> cached_value_;
    std::function<void(const T&)> change_callback_;
    mutable std::mutex change_mutex_;
};

// ============================================
// 自动注册字段（宏辅助）
// ============================================
template<typename T>
struct FieldMap {
    static_assert(!std::is_same_v<T, T>, "FieldMap 未针对此类型进行专门化");
};

#define BEGIN_FIELD_MAP(Class) \
    template<> struct FieldMap<Class> { \
        using T = Class; \
        static void set(T& obj, const std::string& name, const nlohmann::json& val) {

#define FIELD(Class, Type, Name) \
            if (name == #Name) { \
                obj.Name = val.get<Type>(); \
                return; \
            }

#define END_FIELD_MAP() \
            throw std::runtime_error("Unknown field: " + name); \
        } \
    };

// ============================================
// 配置构建器
// ============================================
template<typename T>
class ConfigBuilder {
public:
    ConfigBuilder() = default;

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

    T build() const { return config_; }
    std::optional<T> build_and_validate() const {
        if (config_.validate()) return config_;
        return std::nullopt;
    }

private:
    T config_;
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
// 配置监听器接口
// ============================================
template<typename T>
class ConfigListener {
public:
    virtual ~ConfigListener() = default;
    virtual void on_config_changed(const std::string& key_path, const T& old_value, const T& new_value) = 0;
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

    size_t size() const { return updates_.size() + func_updates_.size(); }

    const auto& get_updates() const { return updates_; }
    const auto& get_func_updates() const { return func_updates_; }

private:
    std::map<std::string, T> updates_;
    std::map<std::string, std::function<T(const T&)>> func_updates_;
};
