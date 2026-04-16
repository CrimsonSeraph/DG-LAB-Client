/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#ifdef _WIN32
#include <iostream>
#include <windows.h>
#else
#include <iostream>
#endif

// ============================================
// Console - 控制台管理类（单例，Windows 平台支持）
// ============================================
class Console {
public:
    // -------------------- 单例 --------------------
    /// @brief 获取单例实例
    static Console& get_instance();

    // -------------------- 公共接口 --------------------
    /// @brief 创建控制台（Windows 上实际创建，其他平台返回 false）
    /// @return 成功返回 true
    bool create();

    /// @brief 销毁控制台（Windows 上释放，其他平台无操作）
    void destroy();

    /// @brief 检查控制台是否已创建
    inline bool is_created() const { return is_created_; }

    // 禁止拷贝
    Console(const Console&) = delete;
    Console& operator=(const Console&) = delete;

private:
    // -------------------- 构造/析构（单例私有）--------------------
    Console();
    ~Console();

    // -------------------- 成员变量 --------------------
    bool is_created_ = false;   ///< 控制台是否已创建

#ifdef _WIN32
    // -------------------- 私有辅助函数（Windows 专用）--------------------
    /// @brief Windows 平台实际创建调试控制台
    /// @return 成功返回 true
    bool create_debug_console();
#endif
};
