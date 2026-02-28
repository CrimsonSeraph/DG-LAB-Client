#pragma once

#include "PyExecutorManager.h"
#include "DebugLog.h"

#include <tuple>
#include <utility>

template<typename ReturnType, typename... Args>
ReturnType PyExecutorManager::call_sync(const std::string& module_name,
    const std::string& class_name,
    const std::string& method_name,
    Args&&... args) {
    LOG_MODULE("PyExecutorManager", "call_sync", LOG_DEBUG,
        "同步调用: 模块=" << module_name << " 类=" << class_name << " 方法=" << method_name);
    try {
        auto var_ptr = get_executor(module_name, class_name);
        auto args_tuple = std::forward_as_tuple(std::forward<Args>(args)...);
        LOG_MODULE("PyExecutorManager", "call_sync", LOG_DEBUG,
            "同步调用成功: " << module_name << "::" << class_name << "::" << method_name);
        return std::visit([&](auto& executor) -> ReturnType {
            return std::apply([&](auto&&... tuple_args) {
                return executor.template call_sync<ReturnType>(
                    method_name, std::forward<decltype(tuple_args)>(tuple_args)...);
                }, args_tuple);
            }, *var_ptr);
    }
    catch (const std::exception& e) {
        LOG_MODULE("PyExecutorManager", "call_sync", LOG_ERROR,
            "同步调用失败: " << module_name << "::" << class_name << "::" << method_name << " - " << e.what());
        throw;
    }
}

template<typename ReturnType, typename... Args>
std::future<ReturnType> PyExecutorManager::call_async(const std::string& module_name,
    const std::string& class_name,
    const std::string& method_name,
    Args&&... args) {
    LOG_MODULE("PyExecutorManager", "call_async", LOG_DEBUG,
        "异步调用: 模块=" << module_name << " 类=" << class_name << " 方法=" << method_name);
    try {
        auto var_ptr = get_executor(module_name, class_name);
        auto args_tuple = std::forward_as_tuple(std::forward<Args>(args)...);
        LOG_MODULE("PyExecutorManager", "call_async", LOG_DEBUG,
            "异步调用已提交: " << module_name << "::" << class_name << "::" << method_name);
        return std::visit([&](auto& executor) -> std::future<ReturnType> {
            return std::apply([&](auto&&... tuple_args) {
                return executor.template call_async<ReturnType>(
                    method_name, std::forward<decltype(tuple_args)>(tuple_args)...);
                }, args_tuple);
            }, *var_ptr);
    }
    catch (const std::exception& e) {
        LOG_MODULE("PyExecutorManager", "call_async", LOG_ERROR,
            "异步调用失败: " << module_name << "::" << class_name << "::" << method_name << " - " << e.what());
        throw;
    }
}
