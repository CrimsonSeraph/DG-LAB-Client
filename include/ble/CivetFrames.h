/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QByteArray>
#include <QtGlobal>

// CivetFrames - 灵猫边缘控制传感器 V1 的指令构造与气压消息解析（纯函数）
// GATT：服务 0x180C / 写 0x150A / 通知 0x150B，电量 0x180A / 0x1500（广播名 47L124000）；
// 字节布局取自上游 DG-LAB-OPENSOURCE/civet-edging-sensor/README_CN.md。
// 文档矛盾处不做推算：D0 标称 17 字节而示例串只有 15 字节，解析按实际长度宽容
// （长度 >= 10 字节才取第 8、9 字节），不足 10 字节整帧丢弃；
// 66 指令正文标称 13 字节但示例的 int16 字节序与正文相反，按 13 / 14 字节两形态给出；
// 采样率、量程与温补文档未给，只做线性换算，不做量程假设。
namespace civet_frames {

    constexpr quint8 kHeadReportCommand = 0x50; ///< 50 指令：开启 / 停止气压上报
    constexpr quint8 kHeadPressure = 0xD0;      ///< D0 消息：气压上报
    constexpr quint8 kHeadCommand66 = 0x66;     ///< 66 指令：气压读值重置 / 屏幕显示方向翻转

    constexpr int kReportCommandSize = 17;    ///< 50 指令总长度（1 + 1 + 1 + 14）
    constexpr int kReportFillLength = 14;     ///< 50 指令的 0x00 填充字节数
    constexpr int kDeclaredReportSize = 17;   ///< 文档标称的 D0 消息长度
    constexpr int kMinReportSize = 10;        ///< 可解析气压的最小长度（含第 8、9 字节）
    constexpr int kPressureOffset = 8;        ///< 气压值在 D0 消息里的起始下标（第 8 字节）
    constexpr int kReportIntervalMs = 100;    ///< 文档声明的气压上报间隔（10Hz）
    constexpr int kDeclaredCommand66Size = 13; ///< 文档标称的 66 指令长度
    constexpr int kCommand66FillLength = 9;   ///< 66 指令的 0x00 填充字节数
    constexpr int kCommand66WideSize = 14;    ///< 66 指令的回退形态长度（填众多 1 字节）

    constexpr quint8 kDefaultIndicatorColor = 0x01; ///< 缺省指示灯颜色（黄色）
    constexpr quint8 kReportStart = 0xD0;           ///< 50 指令：启动气压上报
    constexpr quint8 kReportStop = 0x00;            ///< 50 指令：停止气压上报

    constexpr quint8 kScreenRotateNormal = 0x01;   ///< 屏幕显示方向：正向
    constexpr quint8 kScreenRotateFlipped = 0x03;  ///< 屏幕显示方向：翻转
    constexpr quint8 kScreenRotateKeep = 0x00;     ///< 只重置气压读值、不改显示方向
    constexpr quint16 kPressureResetWord = 0x0002; ///< 66 指令的气压读值重置值
    constexpr quint16 kPressureKeepWord = 0x0000;  ///< 66 指令：不重置气压读值

    constexpr int kPressureUnitPerKpa = 100; ///< 气压单位：1 kPa = 100 个 0.01 kPa
    constexpr int kBatteryUnknown = -1;      ///< 电量未知

    /// @brief 构造 50 指令（17 字节）：头 + 指示灯颜色 + 开启/停止 + 14 字节 0x00 填充
    /// @param indicator_color 指示灯颜色（协议示例 0x01 为黄色）
    /// @param start true 为启动气压上报（0xD0），false 为停止上报（0x00）
    /// @return 17 字节指令
    inline QByteArray build_report_command(quint8 indicator_color, bool start) {
        QByteArray frame;
        frame.reserve(kReportCommandSize);
        frame.append(static_cast<char>(kHeadReportCommand));
        frame.append(static_cast<char>(indicator_color));
        frame.append(static_cast<char>(start ? kReportStart : kReportStop));
        frame.append(QByteArray(kReportFillLength, '\0'));
        return frame;
    }

    /// @brief 构造缺省颜色下的启动上报指令（50 指令，17 字节）
    /// @return 17 字节指令
    inline QByteArray build_report_start() {
        return build_report_command(kDefaultIndicatorColor, true);
    }

    /// @brief 构造缺省颜色下的停止上报指令（50 指令，17 字节）
    /// @return 17 字节指令
    inline QByteArray build_report_stop() {
        return build_report_command(kDefaultIndicatorColor, false);
    }

    /// @brief 有符号 2 字节小端转整数
    /// @param low 低字节
    /// @param high 高字节
    /// @return -32768~32767 的整数
    inline int signed_word_le(quint8 low, quint8 high) {
        const quint16 raw = static_cast<quint16>((static_cast<quint16>(high) << 8) | low);
        return static_cast<qint16>(raw);
    }

    /// @brief 构造 13 字节 66 指令：头 + 9 字节 0x00 + 屏幕方向 + 重置值（高位在前）
    /// @param screen_rotate 屏幕显示方向（0x01 正向 / 0x03 翻转 / 0x00 只重置气压）
    /// @param reset_word 气压读值重置值（0x0002 重置 / 0x0000 未使用）
    /// @return 13 字节指令；字节序按文档示例的字面值写出
    inline QByteArray build_command66(quint8 screen_rotate, quint16 reset_word) {
        QByteArray frame;
        frame.reserve(kDeclaredCommand66Size);
        frame.append(static_cast<char>(kHeadCommand66));
        frame.append(QByteArray(kCommand66FillLength, '\0'));
        frame.append(static_cast<char>(screen_rotate));
        frame.append(static_cast<char>((reset_word >> 8) & 0xFF));
        frame.append(static_cast<char>(reset_word & 0xFF));
        return frame;
    }

    /// @brief 构造 14 字节 66 指令回退形态：在 9 字节填充后再补 1 字节 0x00
    /// @param screen_rotate 屏幕显示方向
    /// @param reset_word 气压读值重置值
    /// @return 14 字节指令
    inline QByteArray build_command66_wide(quint8 screen_rotate, quint16 reset_word) {
        QByteArray frame;
        frame.reserve(kCommand66WideSize);
        frame.append(static_cast<char>(kHeadCommand66));
        frame.append(QByteArray(kCommand66FillLength + 1, '\0'));
        frame.append(static_cast<char>(screen_rotate));
        frame.append(static_cast<char>((reset_word >> 8) & 0xFF));
        frame.append(static_cast<char>(reset_word & 0xFF));
        return frame;
    }

    /// @brief D0 气压消息的解析结果
    struct PressureReport {
        bool valid = false;       ///< 是否为可解析的气压消息
        int size = 0;             ///< 实际长度（文档标称 17，示例串 15）
        int indicator_color = -1; ///< 指示灯颜色（第 2 字节）
        int pressure_raw = 0;     ///< 气压原始值（第 8、9 字节有符号小端，单位 0.01 kPa）
    };

    /// @brief 按实际长度宽容解析 D0 气压消息
    /// @param data 通知特征值原始字节
    /// @return 解析结果；头不符或长度不足 kMinReportSize 时 valid 为 false
    inline PressureReport parse_report(const QByteArray& data) {
        PressureReport report;
        report.size = static_cast<int>(data.size());
        if (report.size < kMinReportSize) {
            return report;
        }
        if (static_cast<quint8>(data.at(0)) != kHeadPressure) {
            return report;
        }
        report.valid = true;
        report.indicator_color = static_cast<quint8>(data.at(1));
        report.pressure_raw = signed_word_le(static_cast<quint8>(data.at(kPressureOffset)),
            static_cast<quint8>(data.at(kPressureOffset + 1)));
        return report;
    }

    /// @brief 气压原始值换算为 kPa
    /// @param pressure_raw 气压原始值（单位 0.01 kPa）
    /// @return 以 kPa 为单位的浮点值
    inline double pressure_kpa(int pressure_raw) {
        return static_cast<double>(pressure_raw) / kPressureUnitPerKpa;
    }

    /// @brief 解析电量特征值（0x180A / 0x1500，1 字节百分比）
    /// @param data 读或通知到的原始字节
    /// @return 0~100 的百分比；空值返回 kBatteryUnknown
    inline int battery_percent(const QByteArray& data) {
        if (data.isEmpty()) {
            return kBatteryUnknown;
        }
        const int percent = static_cast<quint8>(data.at(0));
        return percent > 100 ? 100 : percent;
    }

} // namespace civet_frames
