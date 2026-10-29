/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QByteArray>
#include <QString>
#include <QtGlobal>

#include <algorithm>

// OpossumFrames - 负鼠振动控制器 V1 的指令构造与波形转换（纯函数）
// GATT 与郊狼 V3 相同（0x180C / 写 0x150A / 通知 0x150B），指令集见上游
// DG-LAB-OPENSOURCE/opossum-vibrate-controller/README_CN.md。
namespace opossum_frames {

    constexpr quint8 kNoChange = 0xFF; ///< B3「该通道强度不改」
    constexpr int kWaveMax = 100;      ///< B0 单字节波形强度上限（0x00 ~ 0x64）
    constexpr int kStrengthMax = 200;  ///< B3 通道强度上限（0x00 ~ 0xC8）
    constexpr int kB0Size = 20;        ///< B0 指令长度
    constexpr int kB3Size = 3;         ///< B3 指令长度
    constexpr int kB2Size = 24;        ///< B2 指令完整长度
    constexpr int kB2ShortSize = 20;   ///< B2 指令截断长度（写特征标称上限）

    // 判断字符是否为十六进制数字
    inline bool is_hex_digit(QChar ch) {
        const ushort code = ch.unicode();
        return (code >= '0' && code <= '9') || (code >= 'A' && code <= 'F') || (code >= 'a' && code <= 'f');
    }

    // 判断 4 字节波形数据是否全部落在 0 ~ 100
    inline bool wave_bytes_valid(const QByteArray& bytes) {
        if (bytes.size() != 4) {
            return false;
        }
        for (const char raw : bytes) {
            if (static_cast<quint8>(raw) > kWaveMax) {
                return false;
            }
        }
        return true;
    }

    /// @brief 郊狼 V3 八字节帧转负鼠四字节波形数据（取第 4~7 字节的强度，频率丢弃）
    /// @param frame_hex 八字节 HEX 帧（频率 4B + 强度 4B）
    /// @return 强度数据；帧长度不是 8 字节或含非 HEX 字符时返回空
    inline QByteArray wave_bytes_from_v3_frame(const QString& frame_hex) {
        const QString text = frame_hex.trimmed();
        if (text.size() != 16) {
            return QByteArray();
        }
        for (const QChar ch : text) {
            if (!is_hex_digit(ch)) {
                return QByteArray();
            }
        }
        const QByteArray bytes = QByteArray::fromHex(text.toLatin1());
        return bytes.size() == 8 ? bytes.mid(4, 4) : QByteArray();
    }

    /// @brief 构造 B0 指令（20 字节）：头 + 7 字节 0 + A 通道 4 字节 + 4 字节 0 + B 通道 4 字节
    /// @param channel_a A 通道 4 字节波形数据
    /// @param channel_b B 通道 4 字节波形数据
    /// @return 指令字节；越界或长度不对的通道整条弃用为全 0（静音）
    inline QByteArray build_b0(const QByteArray& channel_a, const QByteArray& channel_b) {
        QByteArray frame;
        frame.append(static_cast<char>(0xB0));
        frame.append(QByteArray(7, '\0'));
        frame.append(wave_bytes_valid(channel_a) ? channel_a : QByteArray(4, '\0'));
        frame.append(QByteArray(4, '\0'));
        frame.append(wave_bytes_valid(channel_b) ? channel_b : QByteArray(4, '\0'));
        return frame;
    }

    // 强度值转 1 字节：负数表示该通道不改（0xFF），其余钳制在 0 ~ 200
    inline char intensity_byte(int value) {
        if (value < 0) {
            return static_cast<char>(kNoChange);
        }
        return static_cast<char>(std::clamp(value, 0, kStrengthMax));
    }

    /// @brief 构造 B3 指令（3 字节）：头 + A 通道强度 + B 通道强度，0xFF 表示该通道不改
    /// @param channel_a A 通道强度（负数表示不改）
    /// @param channel_b B 通道强度（负数表示不改）
    /// @return 指令字节
    inline QByteArray build_b3(int channel_a, int channel_b) {
        QByteArray frame;
        frame.append(static_cast<char>(0xB3));
        frame.append(intensity_byte(channel_a));
        frame.append(intensity_byte(channel_b));
        return frame;
    }

    // B2 指令的 21 字节固定写入段
    inline QByteArray b2_fixed_block() {
        return QByteArray::fromHex("FFFF00FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF0809");
    }

    /// @brief 构造 B2 指令（24 字节）：头 + 21 字节固定段 + A 通道强度 + B 通道强度
    /// @param channel_a A 通道强度（0 ~ 200）
    /// @param channel_b B 通道强度（0 ~ 200）
    /// @return 指令字节
    inline QByteArray build_b2(int channel_a, int channel_b) {
        QByteArray frame;
        frame.append(static_cast<char>(0xB2));
        frame.append(b2_fixed_block());
        frame.append(static_cast<char>(std::clamp(channel_a, 0, kStrengthMax)));
        frame.append(static_cast<char>(std::clamp(channel_b, 0, kStrengthMax)));
        return frame;
    }

    /// @brief 构造 B2 指令的截断形态（20 字节）：完整形态的前 20 字节
    /// @param channel_a A 通道强度（0 ~ 200）
    /// @param channel_b B 通道强度（0 ~ 200）
    /// @return 指令字节
    inline QByteArray build_b2_short(int channel_a, int channel_b) {
        return build_b2(channel_a, channel_b).left(kB2ShortSize);
    }

} // namespace opossum_frames
