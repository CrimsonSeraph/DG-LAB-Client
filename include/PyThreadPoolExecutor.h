#pragma once

#include "PyExecutor.h"

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <atomic>
#include <memory>

/**
 * @brief 线程池增强的Python执行器
 */
class PyThreadPoolExecutor {
public:
    /**
     * @brief 构造线程池执行器
     * @param module_name Python模块名称
     * @param num_threads 线程数量（0表示自动选择）
     */
    explicit PyThreadPoolExecutor(const std::string& module_name,
        size_t num_threads = 0);

    ~PyThreadPoolExecutor();

    // 移动构造和移动赋值
    PyThreadPoolExecutor(PyThreadPoolExecutor&&) noexcept = default;
    PyThreadPoolExecutor& operator=(PyThreadPoolExecutor&&) noexcept = default;

    // 禁止拷贝
    PyThreadPoolExecutor(const PyThreadPoolExecutor&) = delete;
    PyThreadPoolExecutor& operator=(const PyThreadPoolExecutor&) = delete;

    template<typename ReturnType, typename ...Args>
    ReturnType call_sync(const std::string& method_name, Args && ...args);
    template<typename ReturnType, typename ...Args>
    std::future<ReturnType> call_async(const std::string& method_name, Args && ...args);

    /**
     * @brief 异步调用Python方法（使用线程池）
     */
    template<typename ReturnType, typename... Args>
    std::future<ReturnType> submit(const std::string& method_name, Args&&... args);

    /**
     * @brief 等待所有任务完成
     */
    void wait_all();

    /**
     * @brief 获取活跃线程数量
     */
    size_t get_active_count() const;

    /**
     * @brief 获取等待任务数量
     */
    size_t get_pending_count() const;

    /**
     * @brief 获取Python执行器
     */
    PyExecutor& get_executor();

    std::vector<std::string> get_method_list() const;

private:
    struct Task {
        std::function<void()> func;
    };

    struct Impl {
        PyExecutor executor;
        std::vector<std::thread> workers;
        std::queue<Task> task_queue;
        mutable std::mutex queue_mutex;
        std::condition_variable queue_cv;
        std::atomic<bool> stop{ false };
        std::atomic<size_t> active_tasks{ 0 };

        Impl(const std::string& module_name);
        ~Impl();
        void worker_thread(PyThreadPoolExecutor* self);
    };

    std::unique_ptr<Impl> pimpl_;
};

#include "PyThreadPoolExecutor_impl.hpp"
