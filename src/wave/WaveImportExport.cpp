/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "WaveImportExport.h"

// miniz 3.0.2（MIT，third_party/miniz）：仅使用内存 zip 读写 API
#include <miniz.h>
#include <nlohmann/json.hpp>

#include <cstring>
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
    /// @brief 标签字段的别名（按优先级；labels 为兼容别名）
    const char* const TAG_KEYS[] = { "tags", "labels" };

    /// @brief 别名列表长度
    constexpr int KEY_COUNT_V2 = static_cast<int>(sizeof(V2_KEYS) / sizeof(V2_KEYS[0]));
    /// @brief 别名列表长度
    constexpr int KEY_COUNT_V3 = static_cast<int>(sizeof(V3_KEYS) / sizeof(V3_KEYS[0]));
    /// @brief 别名列表长度
    constexpr int KEY_COUNT_TAG = static_cast<int>(sizeof(TAG_KEYS) / sizeof(TAG_KEYS[0]));

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

    /// @brief 标签序列转 JSON 字符串数组
    nlohmann::json tags_to_json(const QStringList& tags) {
        nlohmann::json array = nlohmann::json::array();
        for (const QString& tag : tags) {
            array.push_back(tag.toStdString());
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
        // 标签：缺失按空数组处理，非字符串元素丢弃，统一规范化去重
        if (const nlohmann::json* tags = find_array(item, TAG_KEYS, KEY_COUNT_TAG)) {
            QStringList collected;
            for (const auto& element : *tags) {
                if (element.is_string()) {
                    collected.append(QString::fromStdString(element.get<std::string>()));
                }
            }
            wave.tags = Wave::normalize_tags(collected);
        }
        if (wave.frames_v2.isEmpty() && wave.frames_v3.isEmpty() && wave.raw.empty()) {
            reason = QStringLiteral("《%1》既没有 raw 也没有帧，已跳过").arg(label);
            return false;
        }
        return true;
    }

    /// @brief 按 UTF-8 解码 zip 条目名；非法 UTF-8 回退本地 8 位编码（部分压缩工具写 GBK）
    QString decode_entry_name(const char* raw) {
        const QByteArray bytes(raw);
        const QString name = QString::fromUtf8(bytes);
        if (name.contains(QChar::ReplacementCharacter)) {
            return QString::fromLocal8Bit(bytes);
        }
        return name;
    }

    /// @brief 条目名是否含绝对路径或 .. 路径段（路径穿越）
    bool unsafe_entry_name(const QString& name) {
        if (name.startsWith(QLatin1Char('/')) || name.startsWith(QLatin1Char('\\'))) {
            return true;
        }
        if (name.size() >= 2 && name.at(1) == QLatin1Char(':') && name.at(0).isLetter()) {
            return true;
        }
        const QString normalized = QString(name).replace(QLatin1Char('\\'), QLatin1Char('/'));
        for (const QString& part : normalized.split(QLatin1Char('/'))) {
            if (part == QStringLiteral("..")) {
                return true;
            }
        }
        return false;
    }

} // namespace

QString wave_io::build_json(const std::vector<Wave>& waves, QStringList& notes) {
    notes.clear();
    nlohmann::json array = nlohmann::json::array();
    for (const Wave& wave : waves) {
        nlohmann::json item;
        item["name"] = wave.name;
        item["raw"] = wave.raw;
        item["tags"] = tags_to_json(wave.tags);
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

bool wave_io::parse_zip(const QByteArray& archive, std::vector<ZipEntry>& entries, QStringList& skipped,
    QString& error) {
    entries.clear();
    skipped.clear();
    error.clear();
    if (archive.isEmpty()) {
        error = QStringLiteral("zip 文件为空，没有可读取的内容");
        return false;
    }

    mz_zip_archive zip_archive;
    std::memset(&zip_archive, 0, sizeof(zip_archive));
    if (!mz_zip_reader_init_mem(&zip_archive, archive.constData(), static_cast<size_t>(archive.size()), 0)) {
        error = QStringLiteral("不是有效的 zip 归档：无法读取中央目录");
        return false;
    }

    const mz_uint file_count = mz_zip_reader_get_num_files(&zip_archive);
    if (file_count == 0) {
        mz_zip_reader_end(&zip_archive);
        error = QStringLiteral("zip 归档里没有任何文件");
        return false;
    }
    if (file_count > static_cast<mz_uint>(ZIP_MAX_ENTRIES)) {
        mz_zip_reader_end(&zip_archive);
        error = QStringLiteral("zip 归档条目数 %1 超过上限 %2，已拒绝整个归档")
                    .arg(file_count)
                    .arg(ZIP_MAX_ENTRIES);
        return false;
    }

    qint64 total_bytes = 0;
    for (mz_uint index = 0; index < file_count; ++index) {
        mz_zip_archive_file_stat stat;
        std::memset(&stat, 0, sizeof(stat));
        if (!mz_zip_reader_file_stat(&zip_archive, index, &stat)) {
            skipped.append(QStringLiteral("zip 第 %1 个条目无法读取头部信息，已跳过").arg(index + 1));
            continue;
        }
        if (stat.m_is_directory) {
            continue;
        }
        const QString name = decode_entry_name(stat.m_filename);
        if (name.isEmpty()) {
            skipped.append(QStringLiteral("zip 第 %1 个条目没有名称，已跳过").arg(index + 1));
            continue;
        }
        if (unsafe_entry_name(name)) {
            skipped.append(QStringLiteral("「%1」的条目名是绝对路径或含 ..，已拒绝").arg(name));
            continue;
        }
        if (static_cast<qint64>(stat.m_uncomp_size) > ZIP_MAX_ENTRY_BYTES) {
            mz_zip_reader_end(&zip_archive);
            entries.clear();
            error = QStringLiteral("「%1」解压后 %2 字节，超过单文件上限 %3 字节（4 MiB），已拒绝整个 zip")
                        .arg(name)
                        .arg(static_cast<qulonglong>(stat.m_uncomp_size))
                        .arg(ZIP_MAX_ENTRY_BYTES);
            return false;
        }
        total_bytes += static_cast<qint64>(stat.m_uncomp_size);
        if (total_bytes > ZIP_MAX_TOTAL_BYTES) {
            mz_zip_reader_end(&zip_archive);
            entries.clear();
            error = QStringLiteral("zip 解压后总量超过上限 %1 字节（32 MiB），已拒绝整个 zip").arg(ZIP_MAX_TOTAL_BYTES);
            return false;
        }
        if (!mz_zip_reader_is_file_supported(&zip_archive, index)
            || mz_zip_reader_is_file_encrypted(&zip_archive, index)) {
            skipped.append(QStringLiteral("「%1」的压缩方式不支持或已加密，已跳过").arg(name));
            continue;
        }

        size_t size = 0;
        void* raw = mz_zip_reader_extract_to_heap(&zip_archive, index, &size, 0);
        if (raw == nullptr) {
            skipped.append(QStringLiteral("「%1」解压失败（条目数据损坏），已跳过").arg(name));
            continue;
        }
        ZipEntry entry;
        entry.name = name;
        entry.data = QByteArray(static_cast<const char*>(raw), static_cast<qsizetype>(size));
        entries.push_back(entry);
        mz_free(raw);
    }
    mz_zip_reader_end(&zip_archive);
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

bool wave_io::build_zip(const std::vector<ZipEntry>& entries, QByteArray& out, QString& error) {
    out.clear();
    error.clear();
    if (entries.empty()) {
        error = QStringLiteral("没有可写入 zip 的波形");
        return false;
    }
    if (entries.size() > static_cast<size_t>(ZIP_MAX_ENTRIES)) {
        error = QStringLiteral("条目数 %1 超过上限 %2，无法写入 zip").arg(entries.size()).arg(ZIP_MAX_ENTRIES);
        return false;
    }

    mz_zip_archive zip_archive;
    std::memset(&zip_archive, 0, sizeof(zip_archive));
    if (!mz_zip_writer_init_heap(&zip_archive, 0, 64 * 1024)) {
        error = QStringLiteral("无法初始化 zip 写入器");
        return false;
    }
    for (const ZipEntry& entry : entries) {
        const QByteArray name = entry.name.toUtf8();
        const bool ok = mz_zip_writer_add_mem(&zip_archive, name.constData(), entry.data.constData(),
            static_cast<size_t>(entry.data.size()), static_cast<mz_uint>(MZ_DEFAULT_LEVEL));
        if (!ok) {
            mz_zip_writer_end(&zip_archive);
            error = QStringLiteral("写入 zip 条目失败：%1").arg(entry.name);
            return false;
        }
    }

    void* buffer = nullptr;
    size_t size = 0;
    const bool finalized = mz_zip_writer_finalize_heap_archive(&zip_archive, &buffer, &size);
    if (!finalized || buffer == nullptr) {
        mz_zip_writer_end(&zip_archive);
        error = QStringLiteral("生成 zip 归档失败");
        return false;
    }
    out = QByteArray(static_cast<const char*>(buffer), static_cast<qsizetype>(size));
    mz_free(buffer);
    mz_zip_writer_end(&zip_archive);
    return true;
}
