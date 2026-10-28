/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QString>
#include <QStringList>

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

// 波形数据模型：raw 为设计层原始串，frames_v2 / frames_v3 为同一条波形在两代协议下的帧序列。
// V2 帧为偶数长度 HEX，V3 帧固定 8 字节（16 个十六进制字符），每帧代表 100ms；
// 帧布局见 include/README.md 的「关键协议与约定」。

/// @brief 波形段落：在时长内对频率与强度做线性渐变（起止关键帧）
struct WaveSection {
    int duration_ms = 1000;   ///< 段落时长（100ms 的整数倍）
    int freq_start = 50;      ///< 起始频率（10~240）
    int freq_end = 50;        ///< 结束频率（10~240）
    int strength_start = 50;  ///< 起始强度（0~100）
    int strength_end = 50;    ///< 结束强度（0~100）
};

/// @brief 波形来源：官方内置资源 / 用户波形库
enum class WaveSource {
    Official, ///< 只读内置波形（assets/waves/official_waves.json）
    User      ///< 用户保存的波形
};

/// @brief 波形：名称 + 来源 + 原始串 + 段落（可选，用于再编辑）+ V2 / V3 帧序列（权威数据）
struct Wave {
    std::string name;
    WaveSource source = WaveSource::User;
    std::string raw;              ///< 设计层原始串（Dungeonlab+pulse: 开头），可为空
    std::vector<WaveSection> sections;
    QStringList frames_v2;        ///< V2 HEX 帧（偶数长度，帧长不固定）
    QStringList frames_v3;        ///< V3 8 字节 HEX 帧

    /// @brief 是否官方内置波形（不可删除、不可改名、不可覆盖）
    bool is_official() const { return source == WaveSource::Official; }

    /// @brief 是否同时具备 V2 与 V3 帧
    bool has_both_protocols() const { return !frames_v2.isEmpty() && !frames_v3.isEmpty(); }

    /// @brief 帧数（优先取 V3，仅缺 V3 时取 V2）
    int frame_count() const;

    /// @brief 总时长（毫秒），每帧 100ms
    int duration_ms() const;

    /// @brief 指定协议（"v2" / "v3"）的帧；协议非法时按 V3
    const QStringList& frames_of(const QString& protocol) const;

    /// @brief 由段落序列生成 V3 帧
    static QStringList generate_frames(const std::vector<WaveSection>& sections);

    /// @brief 规范化 HEX 帧：去空白与分隔符、去 0x 前缀、统一转大写；不合规返回空串
    /// @param text 原始帧文本
    /// @param byte_count 期望字节数（V3 传 8；V2 传 0 表示任意偶数长度）
    /// @return 规范化后的帧，不合规返回空串
    static QString normalize_hex_frame(const QString& text, int byte_count);

    /// @brief 波形来源转 JSON 字符串
    static const char* source_name(WaveSource source);
    /// @brief JSON 字符串转波形来源（未知取值按用户波形）
    static WaveSource source_from_name(const std::string& name);

    /// @brief 段落与帧互转的 JSON
    nlohmann::json to_json() const;
    static Wave from_json(const std::string& name, const nlohmann::json& json);
};
