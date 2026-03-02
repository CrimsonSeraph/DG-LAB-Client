#pragma once

#include <QJsonValue>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <string>
#include <algorithm>

namespace DebugLogUtil {
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
}
