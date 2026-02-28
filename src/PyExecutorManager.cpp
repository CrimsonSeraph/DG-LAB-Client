#include "PyExecutorManager.h"
#include "DebugLog.h"

#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <type_traits>
#include <chrono>

PyExecutorManager& PyExecutorManager::instance() {
    static PyExecutorManager inst;
    return inst;
}

bool PyExecutorManager::register_executor(const std::string& module_name,
    const std::string& class_name,
    bool use_thread_pool,
    size_t num_threads) {
    LOG_MODULE("PyExecutorManager", "register_executor", LOG_INFO,
        "正在注册执行器: " << module_name << "::" << class_name
        << " [线程池=" << (use_thread_pool ? "是" : "否")
        << ", 线程数=" << num_threads << "]");

    std::unique_lock lock(mutex_);
    auto& mod_map = executors_[module_name];
    if (mod_map.find(class_name) != mod_map.end()) {
        LOG_MODULE("PyExecutorManager", "register_executor", LOG_WARN,
            "执行器已存在: " << module_name << "::" << class_name);
        return false; // 已存在
    }

    try {
        if (use_thread_pool) {
            // 创建线程池版执行器并以 shared_ptr 存储
            auto ptr = std::make_shared<ExecutorVariant>(PyThreadPoolExecutor(module_name, num_threads));
            mod_map.emplace(class_name, ptr);
            // 创建类实例（访问 variant 内部对象）
            std::visit([&](auto& exec) {
                using T = std::decay_t<decltype(exec)>;
                if constexpr (std::is_same_v<T, PyThreadPoolExecutor>) {
                    exec.get_executor().create_instance(class_name);
                    LOG_MODULE("PyExecutorManager", "register_executor", LOG_DEBUG,
                        "已创建线程池执行器实例: " << module_name << "::" << class_name);
                }
                }, *mod_map.at(class_name));
        }
        else {
            // 创建普通版执行器，并创建实例，然后以 shared_ptr 存储
            PyExecutor exec(module_name);
            exec.create_instance(class_name);
            auto ptr = std::make_shared<ExecutorVariant>(std::move(exec));
            mod_map.emplace(class_name, ptr);
            LOG_MODULE("PyExecutorManager", "register_executor", LOG_DEBUG,
                "已创建单线程执行器实例: " << module_name << "::" << class_name);
        }
        LOG_MODULE("PyExecutorManager", "register_executor", LOG_INFO,
            "成功注册执行器: " << module_name << "::" << class_name);
        return true;
    }
    catch (const std::exception& e) {
        LOG_MODULE("PyExecutorManager", "register_executor", LOG_ERROR,
            "注册执行器失败 (" << module_name << "::" << class_name << "): " << e.what());
        mod_map.erase(class_name);
        if (mod_map.empty()) executors_.erase(module_name);
        return false;
    }
}

bool PyExecutorManager::unregister_executor(const std::string& module_name,
    const std::string& class_name) {
    LOG_MODULE("PyExecutorManager", "unregister_executor", LOG_INFO,
        "正在注销执行器: " << module_name << "::" << class_name);
    // Defensive approach:
    // 1. Copy shared_ptr to the executor under lock
    // 2. Release lock and, if it's a thread-pool executor, wait for its tasks to finish
    // 3. Reacquire lock and erase the entry only if it hasn't been replaced

    std::unique_lock lock(mutex_);
    auto mod_it = executors_.find(module_name);
    if (mod_it == executors_.end()) {
        LOG_MODULE("PyExecutorManager", "unregister_executor", LOG_WARN,
            "未找到模块: " << module_name);
        return false;
    }
    auto cls_it = mod_it->second.find(class_name);
    if (cls_it == mod_it->second.end()) {
        LOG_MODULE("PyExecutorManager", "unregister_executor", LOG_WARN,
            "在模块 " << module_name << " 中未找到类: " << class_name);
        return false;
    }

    // 持有一份 shared_ptr，保证在等待期间对象不会被销毁
    auto var_ptr = cls_it->second;
    // 不在持锁时等待，避免阻塞其他管理操作
    lock.unlock();

    bool timed_out = false;
    try {
        std::visit([&](auto &exec) {
            using T = std::decay_t<decltype(exec)>;
            if constexpr (std::is_same_v<T, PyThreadPoolExecutor>) {
                LOG_MODULE("PyExecutorManager", "unregister_executor", LOG_DEBUG,
                    "等待线程池执行器完成任务: " << module_name << "::" << class_name);
                // 等待最多 5 秒，超时则记录并放弃注销
                if (!exec.wait_all_for(std::chrono::milliseconds(5000))) {
                    LOG_MODULE("PyExecutorManager", "unregister_executor", LOG_WARN,
                        "等待执行器超时，放弃注销: " << module_name << "::" << class_name);
                    timed_out = true;
                }
            }
            }, *var_ptr);
    }
    catch (const std::exception &e) {
        LOG_MODULE("PyExecutorManager", "unregister_executor", LOG_WARN,
            "等待执行器任务完成时出现异常: " << e.what());
    }

    if (timed_out) {
        return false;
    }

    // 再次获取锁并确保我们删除的是同一个执行器实例（避免竞态替换）
    std::unique_lock lock2(mutex_);
    auto mod_it2 = executors_.find(module_name);
    if (mod_it2 == executors_.end()) {
        LOG_MODULE("PyExecutorManager", "unregister_executor", LOG_DEBUG,
            "模块在等待期间已被移除: " << module_name);
        return false;
    }
    auto cls_it2 = mod_it2->second.find(class_name);
    if (cls_it2 == mod_it2->second.end()) {
        LOG_MODULE("PyExecutorManager", "unregister_executor", LOG_DEBUG,
            "类在等待期间已被移除: " << module_name << "::" << class_name);
        return false;
    }

    // 仅当映射中仍然保存着我们之前观察到的实例时才擦除
    if (cls_it2->second == var_ptr) {
        mod_it2->second.erase(cls_it2);
        if (mod_it2->second.empty()) {
            executors_.erase(mod_it2);
        }
        LOG_MODULE("PyExecutorManager", "unregister_executor", LOG_INFO,
            "成功注销执行器: " << module_name << "::" << class_name);
        return true;
    }

    LOG_MODULE("PyExecutorManager", "unregister_executor", LOG_DEBUG,
        "在等待期间执行器已被替换，未执行删除: " << module_name << "::" << class_name);
    return false;
}

bool PyExecutorManager::has_executor(const std::string& module_name,
    const std::string& class_name) const {
    // 调试级别日志，避免频繁输出
    LOG_MODULE("PyExecutorManager", "has_executor", LOG_DEBUG,
        "检查是否存在: " << module_name << "::" << class_name);

    std::shared_lock lock(mutex_);
    auto mod_it = executors_.find(module_name);
    if (mod_it == executors_.end()) {
        LOG_MODULE("PyExecutorManager", "has_executor", LOG_DEBUG,
            "未找到模块: " << module_name);
        return false;
    }
    bool exists = (mod_it->second.find(class_name) != mod_it->second.end());
    LOG_MODULE("PyExecutorManager", "has_executor", LOG_DEBUG,
        "执行器 " << (exists ? "存在" : "不存在") << ": "
        << module_name << "::" << class_name);
    return exists;
}

std::vector<PyExecutorManager::ExecutorInfo> PyExecutorManager::list_executors() const {
    LOG_MODULE("PyExecutorManager", "list_executors", LOG_DEBUG, "列出所有执行器");

    std::vector<ExecutorInfo> infos;
    std::shared_lock lock(mutex_);
    for (const auto& [mod_name, cls_map] : executors_) {
        for (const auto& [cls_name, var_ptr] : cls_map) {
            bool is_thread_pool = std::visit([](const auto& exec) {
                using T = std::decay_t<decltype(exec)>;
                return std::is_same_v<T, PyThreadPoolExecutor>;
                }, *var_ptr);
            infos.push_back({ mod_name, cls_name, is_thread_pool });
            LOG_MODULE("PyExecutorManager", "list_executors", LOG_DEBUG,
                "找到执行器: " << mod_name << "::" << cls_name
                << " [线程池=" << (is_thread_pool ? "是" : "否") << "]");
        }
    }
    LOG_MODULE("PyExecutorManager", "list_executors", LOG_DEBUG,
        "总计列出执行器数量: " << infos.size());
    return infos;
}

std::vector<std::string> PyExecutorManager::get_method_list(
    const std::string& module_name,
    const std::string& class_name) const {
    LOG_MODULE("PyExecutorManager", "get_method_list", LOG_DEBUG,
        "获取方法列表: " << module_name << "::" << class_name);

    auto var_ptr = get_executor(module_name, class_name);
    auto methods = std::visit([](const auto& exec) {
        return exec.get_method_list();
        }, *var_ptr);

    LOG_MODULE("PyExecutorManager", "get_method_list", LOG_DEBUG,
        "执行器 " << module_name << "::" << class_name
        << " 的方法列表包含 " << methods.size() << " 个方法");
    return methods;
}

// 辅助：获取可写引用
PyExecutorManager::ExecutorPtr PyExecutorManager::get_executor(const std::string& module_name,
    const std::string& class_name) {
    LOG_MODULE("PyExecutorManager", "get_executor", LOG_DEBUG,
        "获取可写执行器引用: " << module_name << "::" << class_name);

    std::shared_lock lock(mutex_);
    auto mod_it = executors_.find(module_name);
    if (mod_it == executors_.end()) {
        LOG_MODULE("PyExecutorManager", "get_executor", LOG_ERROR,
            "未找到模块: " << module_name);
        throw std::runtime_error("Module not found: " + module_name);
    }
    auto cls_it = mod_it->second.find(class_name);
    if (cls_it == mod_it->second.end()) {
        LOG_MODULE("PyExecutorManager", "get_executor", LOG_ERROR,
            "在模块 " << module_name << " 中未找到类: " << class_name);
        throw std::runtime_error("Class not found: " + class_name + " in module " + module_name);
    }
    return cls_it->second;
}

// 辅助：获取只读引用
PyExecutorManager::ExecutorPtr PyExecutorManager::get_executor(const std::string& module_name,
    const std::string& class_name) const {
    LOG_MODULE("PyExecutorManager", "get_executor", LOG_DEBUG,
        "获取只读执行器引用: " << module_name << "::" << class_name);

    std::shared_lock lock(mutex_);
    auto mod_it = executors_.find(module_name);
    if (mod_it == executors_.end()) {
        LOG_MODULE("PyExecutorManager", "get_executor", LOG_ERROR,
            "未找到模块: " << module_name);
        throw std::runtime_error("Module not found: " + module_name);
    }
    auto cls_it = mod_it->second.find(class_name);
    if (cls_it == mod_it->second.end()) {
        LOG_MODULE("PyExecutorManager", "get_executor", LOG_ERROR,
            "在模块 " << module_name << " 中未找到类: " << class_name);
        throw std::runtime_error("Class not found: " + class_name + " in module " + module_name);
    }
    return cls_it->second;
}
