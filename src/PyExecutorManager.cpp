#include "PyExecutorManager.h"
#include <iostream>

bool PyExecutorManager::register_executor(const std::string& module_name,
    const std::string& class_name,
    bool use_thread_pool,
    size_t num_threads) {
    std::unique_lock lock(mutex_);
    auto& mod_map = executors_[module_name];
    if (mod_map.find(class_name) != mod_map.end()) {
        return false; // 已存在
    }

    try {
        if (use_thread_pool) {
            // 创建线程池版执行器
            mod_map.emplace(class_name,
                PyThreadPoolExecutor(module_name, num_threads));
            // 创建类实例
            std::visit([&](auto& exec) {
                if constexpr (std::is_same_v<std::decay_t<decltype(exec)>,
                    PyThreadPoolExecutor>) {
                    exec.get_executor().create_instance(class_name);
                }
                }, mod_map.at(class_name));
        }
        else {
            // 创建普通版执行器，并创建实例
            PyExecutor exec(module_name);
            exec.create_instance(class_name);
            mod_map.emplace(class_name, std::move(exec));
        }
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to register executor (" << module_name << "::"
            << class_name << "): " << e.what() << std::endl;
        mod_map.erase(class_name);
        if (mod_map.empty()) executors_.erase(module_name);
        return false;
    }
}

bool PyExecutorManager::unregister_executor(const std::string& module_name,
    const std::string& class_name) {
    std::unique_lock lock(mutex_);
    auto mod_it = executors_.find(module_name);
    if (mod_it == executors_.end()) return false;
    auto cls_it = mod_it->second.find(class_name);
    if (cls_it == mod_it->second.end()) return false;

    mod_it->second.erase(cls_it);
    if (mod_it->second.empty()) {
        executors_.erase(mod_it);
    }
    return true;
}

bool PyExecutorManager::has_executor(const std::string& module_name,
    const std::string& class_name) const {
    std::shared_lock lock(mutex_);
    auto mod_it = executors_.find(module_name);
    if (mod_it == executors_.end()) return false;
    return mod_it->second.find(class_name) != mod_it->second.end();
}

std::vector<PyExecutorManager::ExecutorInfo> PyExecutorManager::list_executors() const {
    std::vector<ExecutorInfo> infos;
    std::shared_lock lock(mutex_);
    for (const auto& [mod_name, cls_map] : executors_) {
        for (const auto& [cls_name, var] : cls_map) {
            bool is_thread_pool = std::visit([](const auto& exec) {
                using T = std::decay_t<decltype(exec)>;
                return std::is_same_v<T, PyThreadPoolExecutor>;
                }, var);
            infos.push_back({ mod_name, cls_name, is_thread_pool });
        }
    }
    return infos;
}

std::vector<std::string> PyExecutorManager::get_method_list(
    const std::string& module_name,
    const std::string& class_name) const {
    const auto& var = get_executor(module_name, class_name);
    return std::visit([](const auto& exec) {
        return exec.get_method_list();
        }, var);
}

// 辅助：获取可写引用
PyExecutorManager::ExecutorVariant& PyExecutorManager::get_executor(const std::string& module_name,
    const std::string& class_name) {
    std::shared_lock lock(mutex_);
    auto mod_it = executors_.find(module_name);
    if (mod_it == executors_.end()) {
        throw std::runtime_error("Module not found: " + module_name);
    }
    auto cls_it = mod_it->second.find(class_name);
    if (cls_it == mod_it->second.end()) {
        throw std::runtime_error("Class not found: " + class_name + " in module " + module_name);
    }
    return cls_it->second;
}

// 辅助：获取只读引用
const PyExecutorManager::ExecutorVariant& PyExecutorManager::get_executor(const std::string& module_name,
    const std::string& class_name) const {
    std::shared_lock lock(mutex_);
    auto mod_it = executors_.find(module_name);
    if (mod_it == executors_.end()) {
        throw std::runtime_error("Module not found: " + module_name);
    }
    auto cls_it = mod_it->second.find(class_name);
    if (cls_it == mod_it->second.end()) {
        throw std::runtime_error("Class not found: " + class_name + " in module " + module_name);
    }
    return cls_it->second;
}
