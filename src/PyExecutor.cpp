#include "PyExecutor.h"
#include <iostream>
#include <stdexcept>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <memory>

// 全局初始化互斥锁，确保Python解释器只初始化一次
static std::once_flag python_init_flag;

PyExecutor::PyExecutor(const std::string& module_name, bool auto_import)
    : module_name_(module_name), module_loaded_(false) {

    // 确保Python解释器只初始化一次
    std::call_once(python_init_flag, []() {
        if (!Py_IsInitialized()) {
            Py_Initialize();
            // 初始化线程支持
            PyEval_InitThreads();
        }
        });

    interpreter_ = std::make_unique<py::scoped_interpreter>();

    if (auto_import && !module_name.empty()) {
        import_module(module_name);
    }
}

PyExecutor::~PyExecutor() {
    // 注意：scoped_interpreter会在析构时自动清理
}

PyExecutor::PyExecutor(PyExecutor&& other) noexcept
    : module_name_(std::move(other.module_name_)),
    interpreter_(std::move(other.interpreter_)),
    module_(std::move(other.module_)),
    module_loaded_(other.module_loaded_) {
    other.module_loaded_ = false;
}

PyExecutor& PyExecutor::operator=(PyExecutor&& other) noexcept {
    if (this != &other) {
        module_name_ = std::move(other.module_name_);
        interpreter_ = std::move(other.interpreter_);
        module_ = std::move(other.module_);
        module_loaded_ = other.module_loaded_;
        other.module_loaded_ = false;
    }
    return *this;
}

bool PyExecutor::initialize(bool add_current_path) {
    try {
        py::gil_scoped_acquire acquire;

        if (add_current_path) {
            py::module sys = py::module::import("sys");
            sys.attr("path").attr("append")(".");
        }

        return true;
    }
    catch (const py::error_already_set& e) {
        std::cerr << "Failed to initialize Python executor: " << e.what() << std::endl;
        return false;
    }
}

bool PyExecutor::import_module(const std::string& module_name) {
    try {
        py::gil_scoped_acquire acquire;

        std::string name_to_import = module_name.empty() ? module_name_ : module_name;
        if (name_to_import.empty()) {
            throw std::runtime_error("Module name is empty");
        }

        module_ = py::module::import(name_to_import.c_str());
        module_name_ = name_to_import;
        module_loaded_ = true;

        return true;
    }
    catch (const py::error_already_set& e) {
        std::cerr << "Failed to import module '" << module_name << "': " << e.what() << std::endl;
        module_loaded_ = false;
        return false;
    }
}

bool PyExecutor::is_module_loaded() const {
    return module_loaded_;
}

bool PyExecutor::has_method(const std::string& method_name) const {
    if (!module_loaded_) return false;

    try {
        py::gil_scoped_acquire acquire;
        return py::hasattr(module_, method_name.c_str());
    }
    catch (...) {
        return false;
    }
}

py::object PyExecutor::get_method(const std::string& method_name) const {
    if (!module_loaded_) {
        throw std::runtime_error("Module not loaded");
    }

    try {
        py::gil_scoped_acquire acquire;
        if (!py::hasattr(module_, method_name.c_str())) {
            throw std::runtime_error("Method '" + method_name + "' not found in module");
        }
        return module_.attr(method_name.c_str());
    }
    catch (const py::error_already_set& e) {
        throw std::runtime_error(std::string("Python error when getting method: ") + e.what());
    }
}

std::vector<std::string> PyExecutor::get_method_list() const {
    std::vector<std::string> methods;

    if (!module_loaded_) return methods;

    try {
        py::gil_scoped_acquire acquire;

        py::list dir_list = py::module::import("builtins").attr("dir")(module_);

        for (auto item : dir_list) {
            std::string name = py::str(item);
            // 过滤掉私有方法和特殊方法
            if (name.empty() || name[0] == '_') continue;

            // 检查是否是可调用对象
            py::object obj = module_.attr(name.c_str());
            if (py::isinstance<py::function>(obj) ||
                py::hasattr(obj, "__call__")) {
                methods.push_back(name);
            }
        }

        return methods;
    }
    catch (...) {
        return methods;
    }
}

py::object PyExecutor::eval(const std::string& code) {
    try {
        py::gil_scoped_acquire acquire;
        return py::eval(code);
    }
    catch (const py::error_already_set& e) {
        throw std::runtime_error(std::string("Python eval error: ") + e.what());
    }
}

py::module PyExecutor::get_module() const {
    return module_;
}

std::string PyExecutor::get_module_name() const {
    return module_name_;
}

bool PyExecutor::reload_module() {
    if (!module_loaded_) return false;

    try {
        py::gil_scoped_acquire acquire;
        py::module importlib = py::module::import("importlib");
        module_ = importlib.attr("reload")(module_);
        return true;
    }
    catch (const py::error_already_set& e) {
        std::cerr << "Failed to reload module: " << e.what() << std::endl;
        return false;
    }
}

// GILLocker实现
PyExecutor::GILLocker::GILLocker() : has_gil_(false) {
    if (!PyGILState_Check()) {
        PyEval_InitThreads();
        PyGILState_Ensure();
        has_gil_ = true;
    }
}

PyExecutor::GILLocker::~GILLocker() {
    if (has_gil_) {
        PyGILState_Release(PyGILState_UNLOCKED);
    }
}
