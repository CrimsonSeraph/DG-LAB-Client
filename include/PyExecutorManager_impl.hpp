#pragma once

#include "PyExecutorManager.h"
#include <tuple>
#include <utility>

template<typename ReturnType, typename... Args>
ReturnType PyExecutorManager::call_sync(const std::string& module_name,
    const std::string& class_name,
    const std::string& method_name,
    Args&&... args) {
    auto& var = get_executor(module_name, class_name);
    auto args_tuple = std::forward_as_tuple(std::forward<Args>(args)...);

    return std::visit([&](auto& executor) -> ReturnType {
        return std::apply([&](auto&&... tuple_args) {
            return executor.template call_sync<ReturnType>(
                method_name, std::forward<decltype(tuple_args)>(tuple_args)...);
            }, args_tuple);
        }, var);
}

template<typename ReturnType, typename... Args>
std::future<ReturnType> PyExecutorManager::call_async(const std::string& module_name,
    const std::string& class_name,
    const std::string& method_name,
    Args&&... args) {
    auto& var = get_executor(module_name, class_name);
    auto args_tuple = std::forward_as_tuple(std::forward<Args>(args)...);

    return std::visit([&](auto& executor) -> std::future<ReturnType> {
        return std::apply([&](auto&&... tuple_args) {
            return executor.template call_async<ReturnType>(
                method_name, std::forward<decltype(tuple_args)>(tuple_args)...);
            }, args_tuple);
        }, var);
}
