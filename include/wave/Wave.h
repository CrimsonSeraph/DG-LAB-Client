/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QStringList>

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

// ============================================
// 波形数据模型（V3 格式）
// ============================================
// 一条 V3 数据为 8 字节 HEX，代表 100ms：
//   [频率1][频率2][频率3][频率4][强度1][强度2][强度3][强度4]
// 前 4 字节为 4 个 25ms 子单元的频率（10~240），后 4 字节为对应强度（0~100）。
// 详见 out/波形说明.md。

/// @brief 波形段落：在时长内对频率与强度做线性渐变（起止关键帧）
struct WaveSection {
    int duration_ms = 1000;   ///< 段落时长（100ms 的整数倍）
    int freq_start = 50;      ///< 起始频率（10~240）
    int freq_end = 50;        ///< 结束频率（10~240）
    int strength_start = 50;  ///< 起始强度（0~100）
    int strength_end = 50;    ///< 结束强度（0~100）
};

/// @brief 波形：名称 + 段落（可选，用于再编辑）+ V3 帧序列（权威数据）
struct Wave {
    std::string name;
    std::vector<WaveSection> sections;
    QStringList frames; ///< V3 8 字节 HEX 帧

    /// @brief 总时长（毫秒），每帧 100ms
    int duration_ms() const { return static_cast<int>(frames.size()) * 100; }

    /// @brief 由段落序列生成 V3 帧
    static QStringList generate_frames(const std::vector<WaveSection>& sections);

    /// @brief 段落与帧互转的 JSON
    nlohmann::json to_json() const;
    static Wave from_json(const std::string& name, const nlohmann::json& json);

    /// @brief 内置默认波形（静音 1 秒）
    static Wave default_wave();
};
