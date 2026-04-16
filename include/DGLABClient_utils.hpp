/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QString>

#include <string>
#include <vector>

// ============================================
// DGLABClientUtil - 客户端工具函数命名空间
// ============================================
namespace DGLABClientUtil {

    /// @brief 检查字符串是否包含任一关键词（不区分大小写）
    /// @param str 待检查的字符串
    /// @param keywords 关键词列表
    /// @return 包含任一关键词返回 true
    inline static bool contains_any_keyword(const QString& str, const std::vector<std::string>& keywords) {
        QString lowerStr = str.toLower();
        for (const auto& kw : keywords) {
            if (lowerStr.contains(QString::fromStdString(kw))) {
                return true;
            }
        }
        return false;
    }

}   // namespace DGLABClientUtil
