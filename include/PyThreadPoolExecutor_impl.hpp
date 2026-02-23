#pragma once

#include "PyThreadPoolExecutor.h"
#include "DebugLog.h"

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
        LOG_MODULE("PyThreadPoolExecutor", "submit", LOG_DEBUG, "线程池任务开始执行: " << method_name);
        try {
            ReturnType result = pimpl_->executor.call_sync<ReturnType>(method_name, std::forward<Args>(args)...);
            promise->set_value(std::move(result));
            LOG_MODULE("PyThreadPoolExecutor", "submit", LOG_DEBUG, "线程池任务执行成功: " << method_name);
        }
        catch (const std::exception& e) {
            LOG_MODULE("PyThreadPoolExecutor", "submit", LOG_ERROR, "线程池任务执行异常: " << method_name << " - " << e.what());
            promise->set_exception(std::current_exception());
        }
        catch (...) {
            LOG_MODULE("PyThreadPoolExecutor", "submit", LOG_ERROR, "线程池任务执行未知异常: " << method_name);
            promise->set_exception(std::current_exception());
        }
        };

    {
        std::lock_guard<std::mutex> lock(pimpl_->queue_mutex);
        if (pimpl_->stop) {
            LOG_MODULE("PyThreadPoolExecutor", "submit", LOG_ERROR,
                "提交任务失败：线程池已停止，方法=" << method_name);
            throw std::runtime_error("Thread pool is stopped");
        }
        pimpl_->task_queue.emplace(Task{ std::move(task) });
    }
    pimpl_->queue_cv.notify_one();
    return future;
}
