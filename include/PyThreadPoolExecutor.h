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

    // 禁止拷贝
    PyThreadPoolExecutor(const PyThreadPoolExecutor&) = delete;
    PyThreadPoolExecutor& operator=(const PyThreadPoolExecutor&) = delete;

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

private:
    void worker_thread();

    struct Task {
        std::function<void()> func;
        std::promise<void> promise;
    };

    PyExecutor executor_;
    std::vector<std::thread> workers_;
    std::queue<Task> task_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::atomic<bool> stop_{ false };
    std::atomic<size_t> active_tasks_{ 0 };
};

#include "../include/PyThreadPoolExecutor_impl.hpp"
