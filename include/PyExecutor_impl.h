template<typename ReturnType, typename... Args>
ReturnType PythonExecutor::call_sync(const std::string& method_name, Args&&... args) {
    if (!module_loaded_) {
        throw std::runtime_error("Module not loaded. Call import_module() first.");
    }

    try {
        py::gil_scoped_acquire acquire;

        // 获取方法
        py::object method = get_method(method_name);

        // 调用方法并转换返回值
        py::object result = method(std::forward<Args>(args)...);

        if constexpr (std::is_same_v<ReturnType, void>) {
            return;
        }
        else {
            return result.cast<ReturnType>();
        }

    }
    catch (const py::error_already_set& e) {
        // 重新抛出为C++异常
        throw std::runtime_error(std::string("Python call error [") +
            method_name + "]: " + e.what());
    }
    catch (const std::exception& e) {
        throw std::runtime_error(std::string("C++ error in call_sync: ") + e.what());
    }
}

template<typename... Args>
void PythonExecutor::call_void(const std::string& method_name, Args&&... args) {
    call_sync<void, Args...>(method_name, std::forward<Args>(args)...);
}

template<typename Func, typename... Args>
auto PythonExecutor::execute_async(Func&& func, Args&&... args)
-> std::future<decltype(func(args...))> {

    return std::async(std::launch::async, [func = std::forward<Func>(func),
        args...]() mutable {
            // 在新的线程中需要获取GIL
            py::gil_scoped_acquire acquire;
            return func(args...);
        });
}

template<typename ReturnType, typename... Args>
std::future<ReturnType> PythonExecutor::call_async(const std::string& method_name,
    Args&&... args) {
    if (!module_loaded_) {
        throw std::runtime_error("Module not loaded. Call import_module() first.");
    }

    // 创建一个lambda包装同步调用
    auto call_wrapper = [this, method_name, args...]() -> ReturnType {
        return this->call_sync<ReturnType>(method_name, args...);
        };

    // 使用execute_async执行
    return execute_async(call_wrapper);
}

template<typename ReturnType, typename... Args>
void PythonExecutor::call_with_callback(const std::string& method_name,
    std::function<void(ReturnType, bool, const std::string&)> callback,
    Args&&... args) {
    if (!callback) {
        throw std::invalid_argument("Callback function cannot be null");
    }

    // 启动异步任务
    auto future = call_async<ReturnType>(method_name, std::forward<Args>(args)...);

    // 使用std::async处理回调
    std::async(std::launch::async, [future = std::move(future), callback]() mutable {
        try {
            ReturnType result = future.get();
            callback(std::move(result), true, "");
        }
        catch (const std::exception& e) {
            ReturnType dummy{};
            callback(std::move(dummy), false, e.what());
        }
        });
}
