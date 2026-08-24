#pragma once

#include <QString>

class ProcessChecker {
public:
    /// @brief 检查指定名称的进程是否在运行
    /// @param processName 进程名（Windows下可带 .exe 扩展名，Unix下通常不带）
    /// @param caseSensitive 是否区分大小写（默认：Windows不区分，Unix区分，但本函数统一提供参数，默认false即忽略大小写）
    /// @return true 如果存在匹配的进程
    static bool isRunning(const QString& processName, bool caseSensitive = false);
};
