/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "Wave.h"

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

} // namespace

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

nlohmann::json Wave::to_json() const {
    nlohmann::json json;
    json["name"] = name;

    nlohmann::json section_array = nlohmann::json::array();
    for (const auto& section : sections) {
        section_array.push_back({ { "duration_ms", section.duration_ms },
            { "freq_start", section.freq_start },
            { "freq_end", section.freq_end },
            { "strength_start", section.strength_start },
            { "strength_end", section.strength_end } });
    }
    json["sections"] = section_array;

    nlohmann::json frame_array = nlohmann::json::array();
    for (const auto& frame : frames) {
        frame_array.push_back(frame.toStdString());
    }
    json["frames"] = frame_array;
    return json;
}

Wave Wave::from_json(const std::string& name, const nlohmann::json& json) {
    Wave wave;
    wave.name = json.value("name", name);

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

    if (json.contains("frames") && json["frames"].is_array()) {
        for (const auto& item : json["frames"]) {
            if (item.is_string()) {
                wave.frames.append(QString::fromStdString(item.get<std::string>()));
            }
        }
    }

    // 帧缺失时按段落重建，保证帧始终是权威数据
    if (wave.frames.isEmpty() && !wave.sections.empty()) {
        wave.frames = generate_frames(wave.sections);
    }
    if (wave.frames.isEmpty()) {
        wave = default_wave();
    }
    return wave;
}

Wave Wave::default_wave() {
    Wave wave;
    wave.name = "silence";
    wave.sections = {};
    wave.frames = { QStringLiteral("0A0A0A0A00000000") };
    return wave;
}
