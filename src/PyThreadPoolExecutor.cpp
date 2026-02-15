#include "PyThreadPoolExecutor.h"
#include <iostream>

PyThreadPoolExecutor::PyThreadPoolExecutor(const std::string& module_name,
    size_t num_threads)
    : executor_(module_name) {

    if (!executor_.initialize()) {
        throw std::runtime_error("Failed to initialize Python executor");
    }

    if (num_threads == 0) {
        num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) num_threads = 4;  // 默认4个线程
    }

    workers_.reserve(num_threads);
    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back(&PyThreadPoolExecutor::worker_thread, this);
    }
}

PyThreadPoolExecutor::~PyThreadPoolExecutor() {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        stop_ = true;
    }
    queue_cv_.notify_all();

    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void PyThreadPoolExecutor::worker_thread() {
    while (true) {
        Task task;

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this]() {
                return stop_ || !task_queue_.empty();
                });

            if (stop_ && task_queue_.empty()) {
                return;
            }

            task = std::move(task_queue_.front());
            task_queue_.pop();
            ++active_tasks_;
        }

        try {
            task.func();
        }
        catch (...) {
            // 异常已在 task.func 内部通过 promise 传递
        }

        --active_tasks_;
    }
}

void PyThreadPoolExecutor::wait_all() {
    while (get_pending_count() > 0 || get_active_count() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

size_t PyThreadPoolExecutor::get_active_count() const {
    return active_tasks_.load();
}

size_t PyThreadPoolExecutor::get_pending_count() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return task_queue_.size();
}

PyExecutor& PyThreadPoolExecutor::get_executor() {
    return executor_;
}
