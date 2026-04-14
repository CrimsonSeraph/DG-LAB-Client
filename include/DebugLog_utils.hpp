#pragma once

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include <algorithm>
#include <string>

// ============================================
// DebugLogUtil - 日志辅助工具命名空间
// ============================================
namespace DebugLogUtil {

    /// @brief 将 QJsonValue 转换为可读字符串
    /// @param val JSON 值
    /// @return 字符串表示
    inline std::string jsonValueToString(const QJsonValue& val) {
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

}   // namespace DebugLogUtil
