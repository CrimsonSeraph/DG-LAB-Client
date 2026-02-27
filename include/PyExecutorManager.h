#pragma once

#include "PyExecutor.h"
#include "PyThreadPoolExecutor.h"

#include <mutex>
#include <shared_mutex>
#include <variant>
#include <unordered_map>
#include <string>
#include <vector>
#include <future>
#include <stdexcept>

class PyExecutorManager {
public:
    /// 执行器信息结构
    struct ExecutorInfo {
        std::string module_name;
        std::string class_name;
        bool is_thread_pool;   // true: 线程池版, false: 普通版
    };

    /**
     * @brief 注册一个执行器
     * @param module_name   Python 模块名
     * @param class_name    类名（用于创建实例）
     * @param use_thread_pool 是否使用线程池版
     * @param num_threads   线程池线程数（仅当 use_thread_pool=true 时有效，0 表示自动）
     * @return 是否注册成功（若已存在则返回 false）
     */
    bool register_executor(const std::string& module_name,
        const std::string& class_name,
        bool use_thread_pool = false,
        size_t num_threads = 0);

    /**
     * @brief 注销一个执行器
     */
    bool unregister_executor(const std::string& module_name,
        const std::string& class_name);

    /**
     * @brief 检查执行器是否存在
     */
    bool has_executor(const std::string& module_name,
        const std::string& class_name) const;

    /**
     * @brief 获取所有已注册的执行器信息
     */
    std::vector<ExecutorInfo> list_executors() const;

    /**
     * @brief 获取某个执行器的方法列表（仅限普通版，线程池版内部委托给普通版）
     */
    std::vector<std::string> get_method_list(const std::string& module_name,
        const std::string& class_name) const;

    /**
     * @brief 同步调用指定执行器的方法
     * @tparam ReturnType 返回值类型
     * @tparam Args       参数类型
     * @param module_name  模块名
     * @param class_name   类名
     * @param method_name  方法名
     * @param args         参数
     * @return 方法返回值
     * @throws std::runtime_error 如果执行器不存在或调用失败
     */
    template<typename ReturnType, typename... Args>
    ReturnType call_sync(const std::string& module_name,
        const std::string& class_name,
        const std::string& method_name,
        Args&&... args);

    /**
     * @brief 异步调用指定执行器的方法（返回 std::future）
     * @tparam ReturnType 返回值类型
     * @tparam Args       参数类型
     * @param module_name  模块名
     * @param class_name   类名
     * @param method_name  方法名
     * @param args         参数
     * @return std::future<ReturnType> 异步结果
     * @throws std::runtime_error 如果执行器不存在
     */
    template<typename ReturnType, typename... Args>
    std::future<ReturnType> call_async(const std::string& module_name,
        const std::string& class_name,
        const std::string& method_name,
        Args&&... args);

private:
    using ExecutorVariant = std::variant<PyExecutor, PyThreadPoolExecutor>;

    mutable std::shared_mutex mutex_;
    // 第一层 key: 模块名, 第二层 key: 类名
    std::unordered_map<std::string,
        std::unordered_map<std::string, ExecutorVariant>> executors_;

    // 辅助：获取 variant 的引用，若不存在则抛出异常
    ExecutorVariant& get_executor(const std::string& module_name,
        const std::string& class_name);
    const ExecutorVariant& get_executor(const std::string& module_name,
        const std::string& class_name) const;
};

#include "PyExecutorManager_impl.hpp"
