/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include <algorithm>
#include <cstddef>
#include <set>
#include <string>

// DebugLogUtil - 日志辅助工具命名空间
namespace DebugLogUtil {

    /// @brief 将 QJsonValue 转换为可读字符串
    /// @param val JSON 值
    /// @return 字符串表示
    inline std::string json_value_to_string(const QJsonValue& val) {
        if (val.isString()) return val.toString().toStdString();
        if (val.isDouble()) return std::to_string(val.toDouble());
        if (val.isBool()) return val.toBool() ? "true" : "false";
        if (val.isNull()) return "null";
        if (val.isArray())
            return QJsonDocument(val.toArray()).toJson(QJsonDocument::Compact).toStdString();
        if (val.isObject())
            return QJsonDocument(val.toObject()).toJson(QJsonDocument::Compact).toStdString();
        return "unknown";
    }

    /// @brief 移除字符串中的换行符，并将连续多个空格压缩为一个
    /// @param str 输入字符串
    /// @return 处理后的字符串
    inline std::string remove_newline(const std::string& str) {
        std::string result;
        result.reserve(str.size());
        bool lastCharWasSpace = false;

        for (char ch : str) {
            if (ch == '\n' || ch == '\r') {
                continue;
            }
            if (ch == ' ') {
                if (!lastCharWasSpace) {
                    result.push_back(ch);
                    lastCharWasSpace = true;
                }
            }
            else {
                result.push_back(ch);
                lastCharWasSpace = false;
            }
        }
        return result;
    }

    /// @brief 判断某个问题是否为首次出现（用于按次/按包触发的高频路径，避免同一问题刷屏）
    /// @param key 问题标识（如表达式文本、来源 + 类型）
    /// @return 首次出现返回 true
    /// @note 最多记录 kMaxKeys 个标识，达到上限后不再上报，避免标识无限增长
    inline bool should_log_once(const std::string& key) {
        static thread_local std::set<std::string> reported;
        constexpr std::size_t kMaxKeys = 64;
        if (reported.size() >= kMaxKeys) {
            return false;
        }
        return reported.insert(key).second;
    }

} // namespace DebugLogUtil
