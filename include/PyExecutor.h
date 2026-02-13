#pragma once

#include <iostream>
#include <pybind11/embed.h>
#include <pybind11/stl.h>
#include <string>
#include <memory>
#include <future>
#include <functional>
#include <unordered_map>

namespace py = pybind11;

/**
 * @brief Python执行器类，用于同步和异步执行Python模块中的方法
 *
 * 这个类封装了pybind11的Python调用功能，提供了简单易用的API
 * 支持同步调用、异步调用、线程池执行等功能
 */
class PyExecutor {
public:
    /**
     * @brief 构造Python执行器
     * @param module_name 要执行的Python模块名称
     * @param auto_import 是否自动导入模块（默认true）
     */
    explicit PyExecutor(const std::string& module_name, bool auto_import = true);

    /**
     * @brief 析构函数
     */
    ~PyExecutor();

    // 删除拷贝构造函数和赋值操作符
    PyExecutor(const PyExecutor&) = delete;
    PyExecutor& operator=(const PyExecutor&) = delete;

    // 允许移动语义
    PyExecutor(PyExecutor&&) noexcept;
    PyExecutor& operator=(PyExecutor&&) noexcept;

    /**
     * @brief 初始化Python执行器
     * @param add_current_path 是否添加当前路径到Python的sys.path
     * @return 初始化是否成功
     */
    bool initialize(bool add_current_path = true);

    /**
     * @brief 导入Python模块
     * @param module_name 模块名称（如果不为空则使用新名称，否则使用构造函数中的名称）
     * @return 导入是否成功
     */
    bool import_module(const std::string& module_name = "");

    /**
     * @brief 检查模块是否已导入
     */
    bool is_module_loaded() const;

    /**
     * @brief 检查方法是否存在
     * @param method_name 方法名称
     * @return 方法是否存在
     */
    bool has_method(const std::string& method_name) const;

    /**
     * @brief 同步调用Python方法
     * @tparam ReturnType 返回类型
     * @tparam Args 参数类型
     * @param method_name 方法名称
     * @param args 方法参数
     * @return 方法返回值
     * @throws std::runtime_error 如果调用失败
     */
    template<typename ReturnType, typename... Args>
    ReturnType call_sync(const std::string& method_name, Args&&... args);

    /**
     * @brief 同步调用Python方法（无返回值版本）
     * @tparam Args 参数类型
     * @param method_name 方法名称
     * @param args 方法参数
     */
    template<typename... Args>
    void call_void(const std::string& method_name, Args&&... args);

    /**
     * @brief 异步调用Python方法（使用std::future）
     * @tparam ReturnType 返回类型
     * @tparam Args 参数类型
     * @param method_name 方法名称
     * @param args 方法参数
     * @return std::future<ReturnType> 异步结果
     */
    template<typename ReturnType, typename... Args>
    std::future<ReturnType> call_async(const std::string& method_name, Args&&... args);

    /**
     * @brief 批量异步调用（使用回调函数）
     * @tparam ReturnType 返回类型
     * @tparam Args 参数类型
     * @param method_name 方法名称
     * @param callback 回调函数，参数为(返回值, 是否成功, 错误信息)
     * @param args 方法参数
     */
    template<typename ReturnType, typename... Args>
    void call_with_callback(const std::string& method_name,
        std::function<void(ReturnType, bool, const std::string&)> callback,
        Args&&... args);

    /**
     * @brief 获取模块中的函数列表
     * @return 函数名称列表
     */
    std::vector<std::string> get_method_list() const;

    /**
     * @brief 执行Python代码字符串
     * @param code Python代码
     * @return 执行结果
     */
    py::object eval(const std::string& code);

    /**
     * @brief 获取底层pybind11模块对象
     * @warning 直接操作需要小心处理GIL
     */
    py::module get_module() const;

    /**
     * @brief 获取模块名称
     */
    std::string get_module_name() const;

    /**
     * @brief 重新导入模块
     */
    bool reload_module();

private:
    std::string module_name_;
    std::unique_ptr<py::scoped_interpreter> interpreter_;
    py::module module_;
    bool module_loaded_;

    // GIL管理辅助函数
    class GILLocker {
    public:
        GILLocker();
        ~GILLocker();
    private:
        bool has_gil_;
    };

    // 获取方法对象（内部使用）
    py::object get_method(const std::string& method_name) const;

    // 异步执行包装器
    template<typename Func, typename... Args>
    static auto execute_async(Func&& func, Args&&... args)
        -> std::future<decltype(func(args...))>;
};

#include "../include/PyExecutor_impl.hpp"
