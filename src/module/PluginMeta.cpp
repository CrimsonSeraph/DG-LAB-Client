/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "PluginMeta.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QString>

#include <string>
#include <vector>

namespace {

    // 读取字符串字段（缺失或类型不符返回空串）
    std::string read_string_field(const QJsonObject& object, const char* key) {
        const QJsonValue value = object.value(QString::fromUtf8(key));
        if (!value.isString()) {
            return std::string();
        }
        return value.toString().toStdString();
    }

    // 读取字符串数组字段（缺失、类型不符或空数组返回空列表）
    std::vector<std::string> read_string_list_field(const QJsonObject& object, const char* key) {
        std::vector<std::string> list;
        const QJsonValue value = object.value(QString::fromUtf8(key));
        if (!value.isArray()) {
            return list;
        }
        const QJsonArray array = value.toArray();
        for (int index = 0; index < array.size(); ++index) {
            const QJsonValue item = array.at(index);
            if (!item.isString()) {
                continue;
            }
            const QString text = item.toString();
            if (!text.isEmpty()) {
                list.push_back(text.toStdString());
            }
        }
        return list;
    }

} // namespace

PluginMeta read_plugin_meta(const std::string& plugin_dir, const std::string& stem) {
    PluginMeta meta;
    if (plugin_dir.empty() || stem.empty()) {
        return meta;
    }
    const QDir plugin_root(QString::fromStdString(plugin_dir));
    QFile meta_file(plugin_root.filePath(QString::fromStdString(stem) + QStringLiteral("/plugin.json")));
    if (!meta_file.exists() || !meta_file.open(QIODevice::ReadOnly)) {
        return meta;
    }
    // 容错：坏 JSON 或顶层非对象一律视为无侧车，由调用方回退到既有展示名称
    QJsonParseError parse_error;
    const QJsonDocument document = QJsonDocument::fromJson(meta_file.readAll(), &parse_error);
    meta_file.close();
    if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
        return meta;
    }
    const QJsonObject object = document.object();
    meta.display_name = read_string_field(object, "name");
    meta.author = read_string_field(object, "author");
    meta.description = read_string_field(object, "description");
    meta.banner = read_string_field(object, "banner");
    meta.version = read_string_field(object, "version");
    meta.tags = read_string_list_field(object, "tags");
    meta.events = parse_event_definitions(object);
    meta.found = true;
    return meta;
}
