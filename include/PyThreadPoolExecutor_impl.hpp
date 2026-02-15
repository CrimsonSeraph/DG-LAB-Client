#pragma once

#include "PyThreadPoolExecutor.h"

template<typename ReturnType, typename... Args>
std::future<ReturnType> PyThreadPoolExecutor::submit(const std::string& method_name,
    Args&&... args) {
    auto promise = std::make_shared<std::promise<ReturnType>>();
    auto future = promise->get_future();

    auto task = [this, promise, method_name, args...]() mutable {
        try {
            ReturnType result = executor_.call_sync<ReturnType>(
                method_name, std::forward<Args>(args)...);
            promise->set_value(std::move(result));
        }
        catch (...) {
            promise->set_exception(std::current_exception());
        }
        };

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (stop_) {
            throw std::runtime_error("Thread pool is stopped");
        }
        task_queue_.emplace(Task{ std::move(task) });
    }

    queue_cv_.notify_one();
    return future;
}
