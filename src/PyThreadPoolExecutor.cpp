#include "PyThreadPoolExecutor.h"
#include "DebugLog.h"
#include <iostream>

// Impl 构造函数
PyThreadPoolExecutor::Impl::Impl(const std::string& module_name)
    : executor(module_name) {
    if (!executor.initialize()) {
        throw std::runtime_error("Failed to initialize Python executor");
    }
}

// Impl 析构函数
PyThreadPoolExecutor::Impl::~Impl() {
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        stop = true;
    }
    queue_cv.notify_all();
    for (auto& t : workers) {
        if (t.joinable()) t.join();
    }
}

// 工作线程函数
void PyThreadPoolExecutor::Impl::worker_thread(PyThreadPoolExecutor* self) {
    while (true) {
        Task task;
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            queue_cv.wait(lock, [this] { return stop || !task_queue.empty(); });
            if (stop && task_queue.empty()) return;
            task = std::move(task_queue.front());
            task_queue.pop();
            ++active_tasks;
        }

        try {
            task.func();
        }
        catch (...) {
            // 记录未知异常（已通过 promise 传递，但这里添加日志便于追踪）
            LOG_MODULE("PyThreadPoolExecutor", "worker_thread", LOG_ERROR,
                "工作线程中未处理的异常");
        }

        --active_tasks;
    }
}

// PyThreadPoolExecutor 构造函数
PyThreadPoolExecutor::PyThreadPoolExecutor(const std::string& module_name, size_t num_threads)
    : pimpl_(std::make_unique<Impl>(module_name)) {
    if (num_threads == 0) {
        num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) num_threads = 4;
    }

    LOG_MODULE("PyThreadPoolExecutor", "PyThreadPoolExecutor", LOG_INFO,
        "线程池以模块的 " << num_threads << " 线程开始：" << module_name);

    pimpl_->workers.reserve(num_threads);
    for (size_t i = 0; i < num_threads; ++i) {
        pimpl_->workers.emplace_back(&Impl::worker_thread, pimpl_.get(), this);
    }
}

PyThreadPoolExecutor::~PyThreadPoolExecutor() = default;  // unique_ptr 自动释放

void PyThreadPoolExecutor::wait_all() {
    while (get_pending_count() > 0 || get_active_count() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

size_t PyThreadPoolExecutor::get_active_count() const {
    return pimpl_->active_tasks.load();
}

size_t PyThreadPoolExecutor::get_pending_count() const {
    std::lock_guard<std::mutex> lock(pimpl_->queue_mutex);
    return pimpl_->task_queue.size();
}

PyExecutor& PyThreadPoolExecutor::get_executor() {
    return pimpl_->executor;
}

std::vector<std::string> PyThreadPoolExecutor::get_method_list() const {
    return pimpl_->executor.get_method_list();
}
