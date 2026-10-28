/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "WaveImportExport.h"

#include <nlohmann/json.hpp>

#include <exception>
#include <string>
#include <vector>

namespace {

    /// @brief .pulse 文本的固定前缀
    const char* const PULSE_PREFIX = "Dungeonlab+pulse:";

    /// @brief V2 帧的字段别名（按优先级取第一个存在的数组）
    const char* const V2_KEYS[] = { "expectedV2", "framesV2", "frames_v2" };
    /// @brief V3 帧的字段别名（按优先级；frames 为旧存储格式）
    const char* const V3_KEYS[] = { "expectedV3", "framesV3", "frames_v3", "frames" };

    /// @brief 别名列表长度
    constexpr int KEY_COUNT_V2 = static_cast<int>(sizeof(V2_KEYS) / sizeof(V2_KEYS[0]));
    /// @brief 别名列表长度
    constexpr int KEY_COUNT_V3 = static_cast<int>(sizeof(V3_KEYS) / sizeof(V3_KEYS[0]));

    /// @brief 读取条目的字符串字段；缺失或类型不符返回空串
    std::string string_field(const nlohmann::json& item, const char* key) {
        const auto it = item.find(key);
        if (it == item.end() || !it->is_string()) {
            return std::string();
        }
        return it->get<std::string>();
    }

    /// @brief 按别名列表取第一个存在的数组字段；都没有返回 nullptr
    const nlohmann::json* find_array(const nlohmann::json& item, const char* const* keys, int key_count) {
        for (int index = 0; index < key_count; ++index) {
            const auto it = item.find(keys[index]);
            if (it != item.end() && it->is_array()) {
                return &(*it);
            }
        }
        return nullptr;
    }

    /// @brief 逐帧规范化帧数组；任一帧不合规即整条失败并记录首个非法帧
    bool read_frames(const nlohmann::json& array, int byte_count, QStringList& frames, QString& bad_frame) {
        frames.clear();
        bad_frame.clear();
        for (const auto& element : array) {
            if (!element.is_string()) {
                bad_frame = QStringLiteral("非字符串帧");
                return false;
            }
            const QString text = QString::fromStdString(element.get<std::string>());
            const QString frame = Wave::normalize_hex_frame(text, byte_count);
            if (frame.isEmpty()) {
                bad_frame = text;
                return false;
            }
            frames.append(frame);
        }
        return true;
    }

    /// @brief 帧序列转 JSON 字符串数组
    nlohmann::json frames_to_json(const QStringList& frames) {
        nlohmann::json array = nlohmann::json::array();
        for (const QString& frame : frames) {
            array.push_back(frame.toStdString());
        }
        return array;
    }

    /// @brief 解析单条目为波形；失败时写跳过原因并返回 false
    bool parse_entry(const nlohmann::json& item, int index, Wave& wave, QString& reason) {
        if (!item.is_object()) {
            reason = QStringLiteral("第 %1 条不是波形对象").arg(index);
            return false;
        }
        wave = Wave();
        wave.source = WaveSource::User;
        wave.name = string_field(item, "name");
        wave.raw = string_field(item, "raw");
        if (wave.name.empty()) {
            reason = QStringLiteral("第 %1 条缺少 name").arg(index);
            return false;
        }
        const QString label = QString::fromStdString(wave.name);
        QString bad_frame;
        if (const nlohmann::json* v2 = find_array(item, V2_KEYS, KEY_COUNT_V2)) {
            if (!read_frames(*v2, 0, wave.frames_v2, bad_frame)) {
                reason = QStringLiteral("《%1》V2 帧非法（%2），已跳过").arg(label, bad_frame);
                return false;
            }
        }
        if (const nlohmann::json* v3 = find_array(item, V3_KEYS, KEY_COUNT_V3)) {
            if (!read_frames(*v3, 8, wave.frames_v3, bad_frame)) {
                reason = QStringLiteral("《%1》V3 帧非法（%2），已跳过").arg(label, bad_frame);
                return false;
            }
        }
        if (wave.frames_v2.isEmpty() && wave.frames_v3.isEmpty() && wave.raw.empty()) {
            reason = QStringLiteral("《%1》既没有 raw 也没有帧，已跳过").arg(label);
            return false;
        }
        return true;
    }

} // namespace

QString wave_io::build_json(const std::vector<Wave>& waves, QStringList& notes) {
    notes.clear();
    nlohmann::json array = nlohmann::json::array();
    for (const Wave& wave : waves) {
        nlohmann::json item;
        item["name"] = wave.name;
        item["raw"] = wave.raw;
        item["expectedV2"] = frames_to_json(wave.frames_v2);
        item["expectedV3"] = frames_to_json(wave.frames_v3);
        array.push_back(item);

        const QString label = QString::fromStdString(wave.name);
        if (wave.frames_v2.isEmpty() && wave.frames_v3.isEmpty()) {
            notes.append(QStringLiteral("《%1》没有 V2 / V3 帧").arg(label));
        }
        else if (wave.frames_v2.isEmpty()) {
            notes.append(QStringLiteral("《%1》缺少 V2 帧").arg(label));
        }
        else if (wave.frames_v3.isEmpty()) {
            notes.append(QStringLiteral("《%1》缺少 V3 帧").arg(label));
        }
    }
    return QString::fromStdString(array.dump(4));
}

bool wave_io::parse_json(const QString& text, std::vector<Wave>& waves, QStringList& skipped, QString& error) {
    waves.clear();
    skipped.clear();
    error.clear();

    nlohmann::json root;
    try {
        root = nlohmann::json::parse(text.toStdString());
    }
    catch (const std::exception&) {
        error = QStringLiteral("JSON 解析失败：文件不是合法的 JSON 文本");
        return false;
    }

    std::vector<nlohmann::json> items;
    if (root.is_array()) {
        for (const auto& item : root) {
            items.push_back(item);
        }
    }
    else if (root.is_object()) {
        items.push_back(root);
    }
    else {
        error = QStringLiteral("JSON 顶层必须是波形对象或波形数组");
        return false;
    }
    if (items.empty()) {
        error = QStringLiteral("JSON 文件里没有波形条目");
        return false;
    }

    int index = 0;
    for (const auto& item : items) {
        ++index;
        Wave wave;
        QString reason;
        if (!parse_entry(item, index, wave, reason)) {
            skipped.append(reason);
            continue;
        }
        waves.push_back(wave);
    }
    return true;
}

bool wave_io::parse_pulse(const QString& text, const QString& name, Wave& wave, QString& error) {
    error.clear();
    const QString content = text.trimmed();
    if (content.isEmpty()) {
        error = QStringLiteral("文件内容为空，不是有效的 .pulse 波形");
        return false;
    }
    if (!content.startsWith(QString::fromLatin1(PULSE_PREFIX))) {
        error = QStringLiteral("文件不是 .pulse 波形：内容缺少 %1 前缀").arg(QString::fromLatin1(PULSE_PREFIX));
        return false;
    }
    const QString trimmed_name = name.trimmed();
    if (trimmed_name.isEmpty()) {
        error = QStringLiteral("无法从文件名得到波形名称，请重命名文件后再导入");
        return false;
    }
    wave = Wave();
    wave.name = trimmed_name.toStdString();
    wave.source = WaveSource::User;
    wave.raw = content.toStdString();
    return true;
}

bool wave_io::build_pulse(const Wave& wave, QString& text, QString& error) {
    error.clear();
    text.clear();
    if (wave.raw.empty()) {
        error = QStringLiteral("《%1》没有原始串（raw 为空），不能导出为 .pulse")
                    .arg(QString::fromStdString(wave.name));
        return false;
    }
    text = QString::fromStdString(wave.raw);
    return true;
}
