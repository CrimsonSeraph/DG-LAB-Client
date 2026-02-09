#pragma once

#include "ConfigManager.h"
#include <type_traits>
#include <string_view>

template<typename T>
concept ConfigSerializable = requires(T t, nlohmann::json & j) {
    { T::to_json(j, t) } -> std::same_as<void>;
    { T::from_json(j, t) } -> std::same_as<void>;
};

// 基础类型配置包装
template<typename T>
class ConfigValue {
private:
    std::shared_ptr<ConfigManager> config_;
    std::string key_path_;
    T default_value_;
    mutable std::optional<T> cached_value_;

public:
    ConfigValue(std::shared_ptr<ConfigManager> config,
        const std::string& key_path,
        T default_value = T{})
        : config_(std::move(config))
        , key_path_(key_path)
        , default_value_(default_value) {
    }

    // 获取值（带缓存）
    const T& get() const {
        if (!cached_value_.has_value()) {
            cached_value_ = config_->get<T>(key_path_, default_value_);
        }
        return cached_value_.value();
    }

    // 设置值（清除缓存）
    void set(const T& value) {
        if (config_->set(key_path_, value)) {
            cached_value_ = value;
            config_->save();  // 自动保存
        }
    }

    // 隐式转换为T
    operator T() const { return get(); }

    // 赋值运算符
    ConfigValue& operator=(const T& value) {
        set(value);
        return *this;
    }

    // 清空缓存（当配置外部修改时调用）
    void invalidate_cache() const {
        cached_value_.reset();
    }
};

// 复杂类型配置包装
template<ConfigSerializable T>
class ConfigObject {
private:
    std::shared_ptr<ConfigManager> config_;
    std::string key_path_;
    T default_value_;
    mutable std::optional<T> cached_value_;

public:
    ConfigObject(std::shared_ptr<ConfigManager> config,
        const std::string& key_path,
        const T& default_value = T{})
        : config_(std::move(config))
        , key_path_(key_path)
        , default_value_(default_value) {
    }

    const T& get() const {
        if (!cached_value_.has_value()) {
            auto json_obj = config_->get<nlohmann::json>(key_path_);
            if (json_obj.has_value()) {
                T obj;
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
        return cached_value_.value();
    }

    void set(const T& value) {
        nlohmann::json j;
        T::to_json(j, value);
        if (config_->set(key_path_, j)) {
            cached_value_ = value;
            config_->save();
        }
    }

    const T& operator*() const { return get(); }
    const T* operator->() const { return &get(); }

    void invalidate_cache() const {
        cached_value_.reset();
    }
};
