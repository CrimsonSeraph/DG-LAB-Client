#include "PyExecutor.h"
#include "PyThreadPoolExecutor.h"
#include <iostream>
#include <vector>
#include <future>
#include <fstream>

// Python示例模块
const char* PYTHON_MODULE_CODE = R"(
# example_module.py

def add(a, b):
    """加法运算"""
    return a + b

def multiply(a, b):
    """乘法运算"""
    return a * b

def process_list(data):
    """处理列表数据"""
    return [x * 2 for x in data]

def process_dict(data):
    """处理字典数据"""
    result = {}
    for key, value in data.items():
        result[key] = value * 3
    return result

def heavy_computation(n):
    """模拟耗时计算"""
    import time
    result = 0
    for i in range(n):
        result += i * i
        time.sleep(0.001)  # 模拟耗时
    return result

class Calculator:
    """计算器类"""
    def __init__(self, name):
        self.name = name
        self.history = []
    
    def calculate(self, operation, a, b):
        if operation == "add":
            result = a + b
        elif operation == "multiply":
            result = a * b
        else:
            raise ValueError(f"Unknown operation: {operation}")
        
        self.history.append(f"{operation}({a}, {b}) = {result}")
        return result
    
    def get_history(self):
        return self.history
)";

void write_python_module() {
    // 写入示例Python模块
    std::ofstream file("example_module.py");
    file << PYTHON_MODULE_CODE;
    file.close();
}

void basic_usage() {
    std::cout << "=== Basic Usage ===" << std::endl;

    // 创建执行器
    PyExecutor executor("example_module");
    executor.initialize();

    // 导入模块
    if (!executor.import_module()) {
        std::cerr << "Failed to import module" << std::endl;
        return;
    }

    // 同步调用
    try {
        int result = executor.call_sync<int>("add", 10, 20);
        std::cout << "add(10, 20) = " << result << std::endl;

        std::vector<int> input_list = { 1, 2, 3, 4, 5 };
        auto output_list = executor.call_sync<std::vector<int>>("process_list", input_list);

        std::cout << "process_list result: ";
        for (int val : output_list) {
            std::cout << val << " ";
        }
        std::cout << std::endl;

    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

void async_usage() {
    std::cout << "\n=== Async Usage ===" << std::endl;

    PyExecutor executor("example_module");
    executor.initialize();

    if (!executor.import_module()) {
        std::cerr << "Failed to import module" << std::endl;
        return;
    }

    // 异步调用
    auto future1 = executor.call_async<int>("heavy_computation", 1000);
    auto future2 = executor.call_async<int>("heavy_computation", 2000);

    std::cout << "Doing other work while Python computes..." << std::endl;

    // 获取异步结果
    try {
        int result1 = future1.get();
        int result2 = future2.get();
        std::cout << "Result 1: " << result1 << std::endl;
        std::cout << "Result 2: " << result2 << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Async error: " << e.what() << std::endl;
    }

    // 使用回调
    executor.call_with_callback<int>("add",
        [](int result, bool success, const std::string& error) {
            if (success) {
                std::cout << "Callback received: " << result << std::endl;
            }
            else {
                std::cerr << "Callback error: " << error << std::endl;
            }
        }, 30, 40);
}

void thread_pool_usage() {
    std::cout << "\n=== Thread Pool Usage ===" << std::endl;

    PyThreadPoolExecutor pool_executor("example_module", 4);

    // 提交多个任务
    std::vector<std::future<int>> futures;
    for (int i = 0; i < 10; ++i) {
        futures.push_back(pool_executor.submit<int>("heavy_computation", 500));
    }

    // 等待所有任务完成
    pool_executor.wait_all();

    // 获取结果
    int total = 0;
    for (auto& future : futures) {
        try {
            int result = future.get();
            total += result;
            std::cout << "Task result: " << result << std::endl;
        }
        catch (const std::exception& e) {
            std::cerr << "Task failed: " << e.what() << std::endl;
        }
    }

    std::cout << "Total: " << total << std::endl;
}

void advanced_features() {
    std::cout << "\n=== Advanced Features ===" << std::endl;

    PyExecutor executor("example_module");
    executor.initialize();

    if (!executor.import_module()) {
        return;
    }

    // 获取方法列表
    auto methods = executor.get_method_list();
    std::cout << "Available methods: ";
    for (const auto& method : methods) {
        std::cout << method << " ";
    }
    std::cout << std::endl;

    // 检查方法是否存在
    if (executor.has_method("multiply")) {
        std::cout << "multiply method exists" << std::endl;
    }

    // 重新导入模块（用于热重载）
    executor.reload_module();
}

int main() {
    // 创建示例Python模块
    write_python_module();

    // 演示各种用法
    basic_usage();
    async_usage();
    thread_pool_usage();
    advanced_features();

    return 0;
}
