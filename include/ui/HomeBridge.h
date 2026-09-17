/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

class DeviceController;

// ============================================
// HomeBridge - 首页通道面板桥接（QML 上下文属性 home）
// ============================================
// 职责：汇总 A/B 通道的强度、上限、启用状态、模块摘要与规则摘要，
//      并转发通道启停与强度调整操作。界面只读取属性、调用 Q_INVOKABLE。
class HomeBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(int strengthA READ strength_a NOTIFY dataChanged)
    Q_PROPERTY(int strengthB READ strength_b NOTIFY dataChanged)
    Q_PROPERTY(int limitA READ limit_a NOTIFY dataChanged)
    Q_PROPERTY(int limitB READ limit_b NOTIFY dataChanged)
    Q_PROPERTY(bool runningA READ running_a NOTIFY dataChanged)
    Q_PROPERTY(bool runningB READ running_b NOTIFY dataChanged)
    Q_PROPERTY(QVariantList modulesA READ modules_a NOTIFY dataChanged)
    Q_PROPERTY(QVariantList modulesB READ modules_b NOTIFY dataChanged)
    Q_PROPERTY(QVariantList rulesA READ rules_a NOTIFY dataChanged)
    Q_PROPERTY(QVariantList rulesB READ rules_b NOTIFY dataChanged)
    Q_PROPERTY(QString waveA READ wave_a NOTIFY dataChanged)
    Q_PROPERTY(QString waveB READ wave_b NOTIFY dataChanged)

public:
    explicit HomeBridge(DeviceController* device, QObject* parent = nullptr);

    /// @brief 接入设备回传与模块/规则变化信号
    void initialize();

    int strength_a() const { return strength_a_; }
    int strength_b() const { return strength_b_; }
    int limit_a() const { return limit_a_; }
    int limit_b() const { return limit_b_; }
    bool running_a() const;
    bool running_b() const;
    QVariantList modules_a() const;
    QVariantList modules_b() const;
    QVariantList rules_a() const;
    QVariantList rules_b() const;
    QString wave_a() const;
    QString wave_b() const;

    /// @brief 切换通道启用状态（同步到规则引擎的通道启用变量）
    Q_INVOKABLE void toggleChannel(const QString& channel);
    /// @brief 设置通道目标强度（自动按上限钳制）
    Q_INVOKABLE void setChannelStrength(const QString& channel, int value);
    /// @brief 按增量调整通道强度
    Q_INVOKABLE void adjustChannelStrength(const QString& channel, int delta);
    /// @brief 请求选择通道波形（阶段 6 接入波形库对话框）
    Q_INVOKABLE void requestWaveSelect(const QString& channel);

signals:
    void dataChanged();
    void channelWaveSelectRequested(const QString& channel);

private:
    int channel_index(const QString& channel) const;
    int clamp_strength(const QString& channel, int value) const;
    void apply_strength(const QString& channel, int value);
    QVariantList build_modules(const QString& channel) const;
    QVariantList build_rules(const QString& channel) const;

    DeviceController* device_ = nullptr;
    int strength_a_ = 0;
    int strength_b_ = 0;
    int limit_a_ = 200;
    int limit_b_ = 200;
};
