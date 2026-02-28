#include "PyExecutor.h"
#include "DebugLog.h"

#include <iostream>
#include <stdexcept>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <memory>

static void ensure_interpreter() {
    static py::scoped_interpreter guard{};
}

py::object PyExecutor::get_bound_method(const std::string& method_name) const {
    // 返回方法对象的副本（py::object 实现为引用计数的持有者）
    return get_method(method_name);
}

PyExecutor::PyExecutor(const std::string& module_name, bool auto_import)
    : module_name_(module_name), module_loaded_(false) {

    if (auto_import && !module_name.empty()) {
        bool ok = import_module(module_name);
    }
}

PyExecutor::~PyExecutor() {
    // 注意：scoped_interpreter会在析构时自动清理
}

PyExecutor::PyExecutor(PyExecutor&& other) noexcept
    : module_name_(std::move(other.module_name_)),
    module_(std::move(other.module_)),
    module_loaded_(other.module_loaded_),
    instance_(std::move(other.instance_)) {
    other.module_loaded_ = false;
}

PyExecutor& PyExecutor::operator=(PyExecutor&& other) noexcept {
    if (this != &other) {
        module_name_ = std::move(other.module_name_);
        module_ = std::move(other.module_);
        module_loaded_ = other.module_loaded_;
        instance_ = std::move(other.instance_);
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
        LOG_MODULE("PyExecutor", "initialize", LOG_ERROR, "初始化失败：" << e.what());
        return false;
    }
    catch (const std::exception& e) {
        return false;
    }
    catch (...) {
        return false;
    }
}

bool PyExecutor::import_module(const std::string& module_name) {
    try {
        std::string name_to_import = module_name.empty() ? module_name_ : module_name;
        py::gil_scoped_acquire acquire;
        if (name_to_import.empty()) {
            throw std::runtime_error("Module name is empty");
        }

        module_ = py::module::import(name_to_import.c_str());
        module_name_ = name_to_import;
        module_loaded_ = true;

        return true;
    }
    catch (const py::error_already_set& e) {
        LOG_MODULE("PyExecutor", "import_module", LOG_ERROR,
            "导入模块失败 '" << module_name << "': " << e.what());
        module_loaded_ = false;
        return false;
    }
}

void PyExecutor::add_path(const std::string& path) {
    py::gil_scoped_acquire acquire;
    py::module sys = py::module::import("sys");
    sys.attr("path").attr("append")(path);
}

bool PyExecutor::create_instance(const std::string& class_name) {
    py::gil_scoped_acquire acquire;
    py::object cls = module_.attr(class_name.c_str());
    instance_ = cls();
    return true;
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

void PyExecutor::exec(const std::string& code) {
    try {
        py::gil_scoped_acquire acquire;
        py::exec(code);
    }
    catch (const py::error_already_set& e) {
        throw std::runtime_error(std::string("Python exec error: ") + e.what());
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
        LOG_MODULE("PyExecutor", "reload_module", LOG_ERROR,
            "模块重新加载失败: " << e.what());
        return false;
    }
}
