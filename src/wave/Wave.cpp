/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "Wave.h"

#include <QChar>

#include <algorithm>
#include <cmath>

namespace {

    int clamp_int(int value, int low, int high) {
        return std::clamp(value, low, high);
    }

    /// @brief 线性插值
    double lerp(double from, double to, double t) {
        return from + (to - from) * t;
    }

    /// @brief 字节转两位大写 HEX
    QString hex_byte(int value) {
        return QString::number(clamp_int(value, 0, 255), 16).rightJustified(2, '0').toUpper();
    }

    /// @brief 帧分隔符：空白与常见标点
    bool is_separator(QChar ch) {
        return ch.isSpace() || ch == ',' || ch == ';' || ch == ':' || ch == '-' || ch == '_';
    }

    /// @brief 单个字符是否十六进制数字
    bool is_hex_digit(QChar ch) {
        return (ch >= QLatin1Char('0') && ch <= QLatin1Char('9'))
            || (ch >= QLatin1Char('a') && ch <= QLatin1Char('f'))
            || (ch >= QLatin1Char('A') && ch <= QLatin1Char('F'));
    }

} // namespace

int Wave::frame_count() const {
    return frames_v3.isEmpty() ? static_cast<int>(frames_v2.size()) : static_cast<int>(frames_v3.size());
}

int Wave::duration_ms() const {
    return frame_count() * 100;
}

const QStringList& Wave::frames_of(const QString& protocol) const {
    return protocol.compare(QStringLiteral("v2"), Qt::CaseInsensitive) == 0 ? frames_v2 : frames_v3;
}

QStringList Wave::generate_frames(const std::vector<WaveSection>& sections) {
    QStringList frames;
    for (const auto& section : sections) {
        const int duration = std::max(100, (section.duration_ms / 100) * 100);
        const int frame_count = duration / 100;
        const int freq_start = clamp_int(section.freq_start, 10, 240);
        const int freq_end = clamp_int(section.freq_end, 10, 240);
        const int strength_start = clamp_int(section.strength_start, 0, 100);
        const int strength_end = clamp_int(section.strength_end, 0, 100);

        for (int frame = 0; frame < frame_count; ++frame) {
            // 每帧 100ms 内含 4 个 25ms 子单元，子单元按整段做线性插值
            const double base = static_cast<double>(frame) / frame_count;
            const double step = 1.0 / (frame_count * 4.0);
            QString encoded;
            for (int unit = 0; unit < 4; ++unit) {
                const double t = base + step * unit;
                encoded += hex_byte(static_cast<int>(std::lround(lerp(freq_start, freq_end, t))));
            }
            for (int unit = 0; unit < 4; ++unit) {
                const double t = base + step * unit;
                encoded += hex_byte(static_cast<int>(std::lround(lerp(strength_start, strength_end, t))));
            }
            frames.append(encoded);
        }
    }
    if (frames.isEmpty()) {
        frames.append(QStringLiteral("0A0A0A0A00000000"));
    }
    return frames;
}

QString Wave::normalize_hex_frame(const QString& text, int byte_count) {
    QString cleaned = text.simplified().remove(QLatin1Char(' '));
    if (cleaned.size() >= 2 && cleaned.at(0) == QLatin1Char('0')
        && (cleaned.at(1) == QLatin1Char('x') || cleaned.at(1) == QLatin1Char('X'))) {
        cleaned = cleaned.mid(2);
    }
    QString hex;
    hex.reserve(cleaned.size());
    for (const QChar ch : cleaned) {
        if (is_separator(ch)) {
            continue;
        }
        if (!is_hex_digit(ch)) {
            return QString();
        }
        hex.append(ch.toUpper());
    }
    if (hex.isEmpty() || hex.size() % 2 != 0) {
        return QString();
    }
    if (byte_count > 0 && hex.size() != byte_count * 2) {
        return QString();
    }
    return hex;
}

const char* Wave::source_name(WaveSource source) {
    return source == WaveSource::Official ? "official" : "user";
}

WaveSource Wave::source_from_name(const std::string& name) {
    return name == "official" ? WaveSource::Official : WaveSource::User;
}

nlohmann::json Wave::to_json() const {
    nlohmann::json json;
    json["name"] = name;
    json["source"] = source_name(source);
    json["raw"] = raw;

    nlohmann::json section_array = nlohmann::json::array();
    for (const auto& section : sections) {
        section_array.push_back({ { "duration_ms", section.duration_ms },
            { "freq_start", section.freq_start },
            { "freq_end", section.freq_end },
            { "strength_start", section.strength_start },
            { "strength_end", section.strength_end } });
    }
    json["sections"] = section_array;

    nlohmann::json frame_v2_array = nlohmann::json::array();
    for (const auto& frame : frames_v2) {
        frame_v2_array.push_back(frame.toStdString());
    }
    json["frames_v2"] = frame_v2_array;

    nlohmann::json frame_v3_array = nlohmann::json::array();
    for (const auto& frame : frames_v3) {
        frame_v3_array.push_back(frame.toStdString());
    }
    json["frames_v3"] = frame_v3_array;
    return json;
}

Wave Wave::from_json(const std::string& name, const nlohmann::json& json) {
    Wave wave;
    wave.name = json.value("name", name);
    wave.source = source_from_name(json.value("source", std::string("user")));
    wave.raw = json.value("raw", std::string());

    if (json.contains("sections") && json["sections"].is_array()) {
        for (const auto& item : json["sections"]) {
            WaveSection section;
            section.duration_ms = item.value("duration_ms", 1000);
            section.freq_start = item.value("freq_start", 50);
            section.freq_end = item.value("freq_end", 50);
            section.strength_start = item.value("strength_start", 50);
            section.strength_end = item.value("strength_end", 50);
            wave.sections.push_back(section);
        }
    }

    // 帧数组逐条规范化，非法帧丢弃（缺失的帧不跨协议补）
    auto read_frames = [&json](const char* key, int byte_count) {
        QStringList frames;
        if (!json.contains(key) || !json[key].is_array()) {
            return frames;
        }
        for (const auto& item : json[key]) {
            if (!item.is_string()) {
                continue;
            }
            const QString frame
                = normalize_hex_frame(QString::fromStdString(item.get<std::string>()), byte_count);
            if (!frame.isEmpty()) {
                frames.append(frame);
            }
        }
        return frames;
    };

    wave.frames_v2 = read_frames("frames_v2", 0);
    wave.frames_v3 = read_frames("frames_v3", 8);
    // 旧格式只有 frames（V3 语义）：落到 frames_v3
    if (wave.frames_v3.isEmpty()) {
        wave.frames_v3 = read_frames("frames", 8);
    }

    // 帧缺失时按段落重建 V3 帧
    if (wave.frames_v3.isEmpty() && !wave.sections.empty()) {
        wave.frames_v3 = generate_frames(wave.sections);
    }
    return wave;
}
