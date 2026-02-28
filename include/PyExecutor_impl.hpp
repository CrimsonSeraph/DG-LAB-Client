#pragma once

#include "PyExecutor.h"
#include "DebugLog.h"

template<typename ReturnType, typename... Args>
inline ReturnType PyExecutor::call_sync(const std::string& method_name, Args&&... args) {
    if (!module_loaded_) {
        LOG_MODULE("PyExecutor", "call_sync", LOG_ERROR, "模块未加载，无法调用方法: " << method_name);
        throw std::runtime_error("Module not loaded. Call import_module() first.");
    }

    try {
        py::gil_scoped_acquire acquire;

        // 获取方法
        py::object method;
        if (instance_) {
            method = instance_.attr(method_name.c_str());
        }
        else {
            method = module_.attr(method_name.c_str());
        }

        LOG_MODULE("PyExecutor", "call_sync", LOG_DEBUG, "开始同步调用方法: " << method_name);
        // 调用方法并转换返回值
        py::object result = method(std::forward<Args>(args)...);

        if constexpr (std::is_same_v<ReturnType, void>) {
            LOG_MODULE("PyExecutor", "call_sync", LOG_DEBUG, "同步调用方法成功（无返回值）: " << method_name);
            return;
        }
        else {
            LOG_MODULE("PyExecutor", "call_sync", LOG_DEBUG, "同步调用方法成功: " << method_name);
            return result.cast<ReturnType>();
        }

    }
    catch (const py::error_already_set& e) {
        LOG_MODULE("PyExecutor", "call_sync", LOG_ERROR, "Python调用错误 [" << method_name << "]: " << e.what());
        throw std::runtime_error(std::string("Python call error [") +
            method_name + "]: " + e.what());
    }
    catch (const std::exception& e) {
        LOG_MODULE("PyExecutor", "call_sync", LOG_ERROR, "C++调用错误 [" << method_name << "]: " << e.what());
        throw std::runtime_error(std::string("C++ error in call_sync: ") + e.what());
    }
}

template<typename... Args>
inline void PyExecutor::call_void(const std::string& method_name, Args&&... args) {
    call_sync<void, Args...>(method_name, std::forward<Args>(args)...);
}

template<typename Func, typename... Args>
inline auto PyExecutor::execute_async(Func&& func, Args&&... args)
-> std::future<decltype(func(args...))> {
    LOG_MODULE("PyExecutor", "execute_async", LOG_DEBUG, "启动异步任务（使用 std::async）");
    return std::async(std::launch::async, [func = std::forward<Func>(func),
        args...]() mutable {
            // 在新的线程中需要获取GIL
            py::gil_scoped_acquire acquire;
            return func(args...);
        });
}

template<typename ReturnType, typename... Args>
inline std::future<ReturnType> PyExecutor::call_async(const std::string& method_name,
    Args&&... args) {
    if (!module_loaded_) {
        LOG_MODULE("PyExecutor", "call_async", LOG_ERROR, "模块未加载，无法异步调用方法: " << method_name);
        throw std::runtime_error("Module not loaded. Call import_module() first.");
    }

    py::module module_copy = module_; // 保持对 module 的引用
    py::object instance_copy = instance_; // 保持对 instance 的引用（若存在）

    auto call_wrapper = [module_copy, instance_copy, method_name, args...]() -> ReturnType {
        // execute_async 内部会获取 GIL，这里不需要再次获取
        py::object method;
        if (instance_copy) {
            method = instance_copy.attr(method_name.c_str());
        }
        else {
            method = module_copy.attr(method_name.c_str());
        }

        py::object result = method(args...);

        if constexpr (std::is_same_v<ReturnType, void>) {
            return;
        }
        else {
            return result.cast<ReturnType>();
        }
    };

    return execute_async(call_wrapper);
}

template<typename ReturnType, typename... Args>
inline void PyExecutor::call_with_callback(const std::string& method_name,
    std::function<void(ReturnType, bool, const std::string&)> callback,
    Args&&... args) {
    if (!callback) {
        LOG_MODULE("PyExecutor", "call_with_callback", LOG_ERROR, "回调函数为空，无法调用: " << method_name);
        throw std::invalid_argument("Callback function cannot be null");
    }

    LOG_MODULE("PyExecutor", "call_with_callback", LOG_DEBUG, "开始带回调的异步调用: " << method_name);
    // 启动异步任务
    auto future = call_async<ReturnType>(method_name, std::forward<Args>(args)...);

    // 使用std::async处理回调
    std::async(std::launch::async, [future = std::move(future), callback, method_name]() mutable {
        LOG_MODULE("PyExecutor", "call_with_callback", LOG_DEBUG, "回调任务启动，等待异步结果: " << method_name);
        try {
            ReturnType result = future.get();
            LOG_MODULE("PyExecutor", "call_with_callback", LOG_DEBUG, "异步调用成功，执行回调: " << method_name);
            callback(std::move(result), true, "");
        }
        catch (const std::exception& e) {
            LOG_MODULE("PyExecutor", "call_with_callback", LOG_ERROR, "异步调用失败，执行回调: " << method_name << " - " << e.what());
            ReturnType dummy{};
            callback(std::move(dummy), false, e.what());
        }
        });
}
