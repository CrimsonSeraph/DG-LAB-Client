/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QByteArray>
#include <QtGlobal>

#include <algorithm>

// PawFrames - 爪印无线按钮传感器 V1.1 的指令构造与回应消息解析（纯函数）
// GATT 与郊狼 V3 相同（服务 0x180C / 写 0x150A / 通知 0x150B），没有 0x180A 电量服务，
// 电量由 51 消息携带；字节布局取自上游 DG-LAB-OPENSOURCE/paw-prints/README_CN.md。
// 文档自相矛盾的两处不做推算：0x0F 外部电压模式的参数映射公式只原样保留原始参数，
// F1 角度阈值除以 2 后的取整规则未定，只按有符号整数截断给出展示值。
namespace paw_frames {

    constexpr quint8 kHeadConfig = 0x50;         ///< 50 指令：主指示灯颜色 + 触发模式设置
    constexpr quint8 kHeadDeviceInfo = 0x51;     ///< 51 消息：指示灯颜色 + 设备类型 + 电量
    constexpr quint8 kHeadTriggerStart = 0x5A;   ///< 5A 消息：进入触发状态
    constexpr quint8 kHeadTriggerCancel = 0x5B;  ///< 5B 消息：取消触发状态
    constexpr quint8 kHeadParamChange = 0x5C;    ///< 5C 消息：触发参数变化
    constexpr quint8 kHeadPhysical = 0xD0;       ///< D0 消息：10Hz 全量物理数据
    constexpr quint8 kHeadAngleSelfTest = 0xF1;  ///< F1 消息：自动检测上报的角度阈值范围

    constexpr int kConfigSize = 17;             ///< 50 指令总长度（1 + 1 + 15）
    constexpr int kTriggerSettingsSize = 15;    ///< 触发模式设置长度（1 + 1 + 1 + 12）
    constexpr int kDeviceInfoSize = 4;          ///< 51 消息长度
    constexpr int kTriggerStartSize = 4;        ///< 5A 消息长度
    constexpr int kTriggerCancelSize = 3;       ///< 5B 消息长度
    constexpr int kParamChangeSize = 4;         ///< 5C 消息长度
    constexpr int kPhysicalSize = 9;            ///< D0 消息长度
    constexpr int kAngleSelfTestSize = 14;      ///< F1 消息长度（1 + 1 + 4 + 4 + 4）

    constexpr quint8 kDeviceTypePawV11 = 0x03;  ///< 51 消息里的设备类型：爪印 V1.1
    constexpr quint8 kAngleSelfTestFixed = 0x61; ///< F1 消息第 2 字节固定值

    constexpr quint8 kModeNone = 0x00;          ///< 触发模式：无任何触发
    constexpr quint8 kModeButton = 0x05;        ///< 触发模式：按下/抬起 + 加速度/角度

    constexpr quint8 kDefaultIndicatorColor = 0x01; ///< 缺省主指示灯颜色（黄色，协议范围 0x01~0x06）
    constexpr quint8 kDefaultButtonEventId = 0x01;  ///< 缺省按下/抬起触发事件 ID（协议范围 1~24）
    constexpr quint8 kDefaultParamJump = 0xFF;      ///< 缺省按下时参数立刻增加到上限、抬起时降回 0

    /// @brief 构造「无任何触发」的 15 字节触发设置
    /// @return 1 字节 0x00 模式 + 14 字节 0
    inline QByteArray trigger_settings_none() {
        return QByteArray(kTriggerSettingsSize, '\0');
    }

    /// @brief 组合按下/抬起与加速度/角度触发的设置集合字节
    /// @param release_trigger 是否抬起触发（false 为按下触发）
    /// @param angle_submode 是否角度子模式（false 为加速度子模式）
    /// @param trigger_debounce 触发消抖（0~7，映射 0~0.7 秒）
    /// @param cancel_debounce 取消触发消抖（0~7，映射 0~0.7 秒）
    /// @return 设置集合字节：bit7 抬起 / bit6 角度子模式 / bit5~3 触发消抖 / bit2~0 取消消抖
    inline quint8 button_settings_byte(bool release_trigger, bool angle_submode, int trigger_debounce,
        int cancel_debounce) {
        const quint8 release_bit = release_trigger ? 0x80 : 0x00;
        const quint8 angle_bit = angle_submode ? 0x40 : 0x00;
        const quint8 trigger_bits = static_cast<quint8>((std::clamp(trigger_debounce, 0, 7) & 0x07) << 3);
        const quint8 cancel_bits = static_cast<quint8>(std::clamp(cancel_debounce, 0, 7) & 0x07);
        return static_cast<quint8>(release_bit | angle_bit | trigger_bits | cancel_bits);
    }

    /// @brief 0x05 触发模式的参数（按下/抬起触发与加速度/角度触发共用一条 15 字节设置）
    struct ButtonTriggerSettings {
        quint8 event_id = kDefaultButtonEventId;      ///< 按下/抬起触发事件 ID（1~24，0 表示不启用）
        quint8 settings = 0x00;                       ///< 设置集合字节（按下触发、不启用加速度/角度）
        quint8 param_up_speed = 0x00;                 ///< 触发期间参数增加速度（0 表示不持续增加）
        quint8 param_cancel_down = kDefaultParamJump; ///< 取消触发时参数立刻下降值
        quint8 param_down_speed = 0x00;               ///< 取消触发后参数持续下降速度（0 表示不持续下降）
        quint8 param_up_value = kDefaultParamJump;    ///< 触发时参数立刻增加值
        quint8 motion_event_id = 0x00;                ///< 加速度/角度触发事件 ID（0 表示不启用）
        QByteArray motion_thresholds = QByteArray(6, '\0'); ///< 加速度阈值或 XYZ 角度阈值范围（6 字节）
        quint8 param_map_range = 0x00;                ///< 加速度/角度模式的参数映射范围
    };

    /// @brief 构造 0x05 触发模式的 15 字节设置
    /// @param settings 触发参数
    /// @return 设置字节；motion_thresholds 长度不是 6 时返回空
    inline QByteArray trigger_settings_button(const ButtonTriggerSettings& settings) {
        if (settings.motion_thresholds.size() != 6) {
            return QByteArray();
        }
        QByteArray bytes;
        bytes.reserve(kTriggerSettingsSize);
        bytes.append(static_cast<char>(kModeButton));
        bytes.append(static_cast<char>(settings.event_id));
        bytes.append(static_cast<char>(settings.settings));
        bytes.append(static_cast<char>(settings.param_up_speed));
        bytes.append(static_cast<char>(settings.param_cancel_down));
        bytes.append(static_cast<char>(settings.param_down_speed));
        bytes.append(static_cast<char>(settings.param_up_value));
        bytes.append(static_cast<char>(settings.motion_event_id));
        bytes.append(settings.motion_thresholds);
        bytes.append(static_cast<char>(settings.param_map_range));
        return bytes;
    }

    /// @brief 构造 50 指令（17 字节）：头 + 主指示灯颜色 + 15 字节触发模式设置
    /// @param indicator_color 主指示灯颜色（协议范围 0x01~0x06）
    /// @param trigger_settings 15 字节触发模式设置
    /// @return 指令字节；触发设置长度不是 15 字节时返回空
    inline QByteArray build_config(quint8 indicator_color, const QByteArray& trigger_settings) {
        if (trigger_settings.size() != kTriggerSettingsSize) {
            return QByteArray();
        }
        QByteArray frame;
        frame.reserve(kConfigSize);
        frame.append(static_cast<char>(kHeadConfig));
        frame.append(static_cast<char>(indicator_color));
        frame.append(trigger_settings);
        return frame;
    }

    /// @brief 有符号 2 字节大端转整数
    /// @param high 高字节
    /// @param low 低字节
    /// @return -32768~32767 的整数
    inline int signed_word(quint8 high, quint8 low) {
        const quint16 raw = static_cast<quint16>((static_cast<quint16>(high) << 8) | low);
        return static_cast<qint16>(raw);
    }

    /// @brief F1 上报的角度阈值换算为展示值
    /// @param raw 上报的有符号原始值
    /// @return 除以 2 后的值（有符号整数截断，取整规则待上游确认）
    inline int halve_threshold(int raw) {
        return raw / 2;
    }

    /// @brief 回应消息类别
    enum class MessageKind {
        Unknown = 0,     ///< 未识别（长度不符、固定字节不符或未知头）
        DeviceInfo,      ///< 51
        TriggerStart,    ///< 5A
        TriggerCancel,   ///< 5B
        ParamChange,     ///< 5C
        Physical,        ///< D0
        AngleSelfTest    ///< F1
    };

    /// @brief 回应消息（字段按类别有效，未涉及的字段保持 -1 / 0）
    struct Message {
        MessageKind kind = MessageKind::Unknown; ///< 消息类别
        int indicator_color = -1; ///< 主指示灯颜色
        int device_type = -1;     ///< 设备类型（51，爪印 V1.1 为 0x03）
        int battery = -1;         ///< 电量百分比（51）
        int event_id = -1;        ///< 触发事件 ID（5A / 5B / 5C）
        int param = -1;           ///< 触发参数（5A / 5C）
        int sequence = -1;        ///< 上报序号（D0）
        int pressed = 0;          ///< 按下状态原始字节（D0，0x00 表示未按下）
        int accel = 0;            ///< 加速度值（D0）
        int angle_x = 0;          ///< X 轴角度值（D0）
        int angle_y = 0;          ///< Y 轴角度值（D0）
        int angle_z = 0;          ///< Z 轴角度值（D0）
        int voltage = 0;          ///< 外部电压值（D0）
        int threshold_low[3] = {0, 0, 0};  ///< XYZ 角度阈值下限原始值（F1，有符号）
        int threshold_high[3] = {0, 0, 0}; ///< XYZ 角度阈值上限原始值（F1，有符号）
    };

    /// @brief 按字节布局解析一条回应消息
    /// @param data 通知特征值原始字节
    /// @return 解析结果；长度不符、固定字节不符或未知头一律返回 MessageKind::Unknown
    inline Message parse_message(const QByteArray& data) {
        Message message;
        if (data.isEmpty()) {
            return message;
        }
        const quint8 head = static_cast<quint8>(data.at(0));
        const int size = static_cast<int>(data.size());
        switch (head) {
        case kHeadDeviceInfo:
            if (size != kDeviceInfoSize) {
                return message;
            }
            message.kind = MessageKind::DeviceInfo;
            message.indicator_color = static_cast<quint8>(data.at(1));
            message.device_type = static_cast<quint8>(data.at(2));
            message.battery = static_cast<quint8>(data.at(3));
            return message;
        case kHeadTriggerStart:
            if (size != kTriggerStartSize) {
                return message;
            }
            message.kind = MessageKind::TriggerStart;
            message.indicator_color = static_cast<quint8>(data.at(1));
            message.event_id = static_cast<quint8>(data.at(2));
            message.param = static_cast<quint8>(data.at(3));
            return message;
        case kHeadTriggerCancel:
            if (size != kTriggerCancelSize) {
                return message;
            }
            message.kind = MessageKind::TriggerCancel;
            message.indicator_color = static_cast<quint8>(data.at(1));
            message.event_id = static_cast<quint8>(data.at(2));
            return message;
        case kHeadParamChange:
            if (size != kParamChangeSize) {
                return message;
            }
            message.kind = MessageKind::ParamChange;
            message.indicator_color = static_cast<quint8>(data.at(1));
            message.event_id = static_cast<quint8>(data.at(2));
            message.param = static_cast<quint8>(data.at(3));
            return message;
        case kHeadPhysical:
            if (size != kPhysicalSize) {
                return message;
            }
            message.kind = MessageKind::Physical;
            message.indicator_color = static_cast<quint8>(data.at(1));
            message.sequence = static_cast<quint8>(data.at(2));
            message.pressed = static_cast<quint8>(data.at(3));
            message.accel = static_cast<quint8>(data.at(4));
            message.angle_x = static_cast<quint8>(data.at(5));
            message.angle_y = static_cast<quint8>(data.at(6));
            message.angle_z = static_cast<quint8>(data.at(7));
            message.voltage = static_cast<quint8>(data.at(8));
            return message;
        case kHeadAngleSelfTest:
            if (size != kAngleSelfTestSize || static_cast<quint8>(data.at(1)) != kAngleSelfTestFixed) {
                return message;
            }
            message.kind = MessageKind::AngleSelfTest;
            for (int axis = 0; axis < 3; ++axis) {
                const int base = 2 + axis * 4;
                message.threshold_low[axis] = signed_word(static_cast<quint8>(data.at(base)),
                    static_cast<quint8>(data.at(base + 1)));
                message.threshold_high[axis] = signed_word(static_cast<quint8>(data.at(base + 2)),
                    static_cast<quint8>(data.at(base + 3)));
            }
            return message;
        default:
            return message;
        }
    }

} // namespace paw_frames
