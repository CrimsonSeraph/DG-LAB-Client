#pragma once

#include <windows.h>
#include <iostream>

class Console {
public:
    // 单例模式获取实例（使用局部静态变量）
    static Console& GetInstance();

    // 手动创建和销毁控制台
    bool Create();
    void Destroy();

    // 检查控制台是否已创建
    inline bool IsCreated() const { return m_isCreated; }

    // 禁止拷贝
    Console(const Console&) = delete;
    Console& operator=(const Console&) = delete;

private:
    Console();  // 私有构造函数
    ~Console(); // 私有析构函数

    bool CreateDebugConsole();

    bool m_isCreated = false;
};
