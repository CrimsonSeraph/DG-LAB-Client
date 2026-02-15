#pragma once

#include "PyThreadPoolExecutor.h"

template<typename ReturnType, typename... Args>
inline ReturnType PyThreadPoolExecutor::call_sync(const std::string& method_name, Args&&... args) {
    return pimpl_->executor.call_sync<ReturnType>(method_name, std::forward<Args>(args)...);
}

template<typename ReturnType, typename... Args>
inline std::future<ReturnType> PyThreadPoolExecutor::call_async(const std::string& method_name, Args&&... args) {
    return submit<ReturnType>(method_name, std::forward<Args>(args)...);
}

template<typename ReturnType, typename... Args>
inline std::future<ReturnType> PyThreadPoolExecutor::submit(const std::string& method_name, Args&&... args) {
    auto promise = std::make_shared<std::promise<ReturnType>>();
    auto future = promise->get_future();

    auto task = [this, promise, method_name, args...]() mutable {
        try {
            ReturnType result = pimpl_->executor.call_sync<ReturnType>(method_name, std::forward<Args>(args)...);
            promise->set_value(std::move(result));
        }
        catch (...) {
            promise->set_exception(std::current_exception());
        }
        };

    {
        std::lock_guard<std::mutex> lock(pimpl_->queue_mutex);
        if (pimpl_->stop) {
            throw std::runtime_error("Thread pool is stopped");
        }
        pimpl_->task_queue.emplace(Task{ std::move(task) });
    }
    pimpl_->queue_cv.notify_one();
    return future;
}
