#pragma once

#ifdef _WIN32
#include <iostream>
#include <windows.h>
#else
#include <iostream>
#endif

class Console {
public:
    // 单例模式获取实例（使用局部静态变量）
    static Console& get_instance();

    // 手动创建和销毁控制台
    // 会尝试创建控制台（Windows 上实际创建并返回 true，其他平台返回 false）
    bool create();
    // 销毁控制台（Windows 上实际销毁，其他平台无操作）
    void destroy();

    // 检查控制台是否已创建
    inline bool is_created() const { return is_created_; }

    // 禁止拷贝
    Console(const Console&) = delete;
    Console& operator=(const Console&) = delete;

private:
    Console();  // 私有构造函数
    ~Console(); // 私有析构函数

#ifdef _WIN32
    bool create_debug_console();
#endif

    bool is_created_ = false;
};
