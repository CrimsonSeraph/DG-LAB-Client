#pragma once

#ifdef _WIN32
#include <windows.h>
#include <iostream>
#else
#include <iostream>
#endif

class Console {
public:
    // 单例模式获取实例（使用局部静态变量）
    static Console& GetInstance();

    // 手动创建和销毁控制台
    // 会尝试创建控制台（Windows 上实际创建并返回 true，其他平台返回 false）
    bool Create();
    // 销毁控制台（Windows 上实际销毁，其他平台无操作）
    void Destroy();

    // 检查控制台是否已创建
    inline bool IsCreated() const { return m_isCreated; }

    // 禁止拷贝
    Console(const Console&) = delete;
    Console& operator=(const Console&) = delete;

private:
    Console();  // 私有构造函数
    ~Console(); // 私有析构函数

#ifdef _WIN32
    bool CreateDebugConsole();
#endif

    bool m_isCreated = false;
};
