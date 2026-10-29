/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

class DeviceController;

// HomeBridge - 首页通道面板桥接（QML 上下文属性 home）
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
    Q_PROPERTY(QString channelAState READ channel_a_state NOTIFY dataChanged)
    Q_PROPERTY(QString channelBState READ channel_b_state NOTIFY dataChanged)
    Q_PROPERTY(QVariantList outputDevices READ output_devices NOTIFY deviceChanged)
    Q_PROPERTY(QString defaultDevice READ default_device NOTIFY deviceChanged)
    Q_PROPERTY(int defaultDeviceIndex READ default_device_index NOTIFY deviceChanged)
    Q_PROPERTY(bool waveSendingA READ wave_sending_a NOTIFY waveStateChanged)
    Q_PROPERTY(int waveFrameA READ wave_frame_a NOTIFY waveStateChanged)
    Q_PROPERTY(int waveFramesA READ wave_frames_a NOTIFY waveStateChanged)
    Q_PROPERTY(bool waveSendingB READ wave_sending_b NOTIFY waveStateChanged)
    Q_PROPERTY(int waveFrameB READ wave_frame_b NOTIFY waveStateChanged)
    Q_PROPERTY(int waveFramesB READ wave_frames_b NOTIFY waveStateChanged)
    Q_PROPERTY(bool miniWindowLocked READ mini_window_locked NOTIFY miniWindowLockedChanged)
    Q_PROPERTY(qreal miniWindowOpacity READ mini_window_opacity NOTIFY miniWindowOpacityChanged)
    Q_PROPERTY(QString miniWindowSize READ mini_window_size NOTIFY miniWindowSizeChanged)
    Q_PROPERTY(QVariantList miniWindowWaveA READ mini_window_wave_a NOTIFY miniWindowWaveChanged)
    Q_PROPERTY(QVariantList miniWindowWaveB READ mini_window_wave_b NOTIFY miniWindowWaveChanged)

public:
    explicit HomeBridge(DeviceController* device, QObject* parent = nullptr);

    /// @brief 接入设备回传与模块/规则变化信号
    void initialize();

    int strength_a() const { return strength_a_; }
    int strength_b() const { return strength_b_; }
    /// @brief 通道 A 展示上限（全局通道配置的 min(limit, safety)）
    int limit_a() const;
    /// @brief 通道 B 展示上限（全局通道配置的 min(limit, safety)）
    int limit_b() const;
    /// @brief 在线输出设备列表（默认设备下拉的数据源）
    QVariantList output_devices() const;
    /// @brief 当前默认设备 MAC（无在线输出设备时为空串）
    QString default_device() const;
    /// @brief 默认设备在在线输出设备列表中的下标（无默认设备时为 -1）
    int default_device_index() const;
    bool running_a() const;
    bool running_b() const;
    QVariantList modules_a() const;
    QVariantList modules_b() const;
    QVariantList rules_a() const;
    QVariantList rules_b() const;
    QString wave_a() const;
    QString wave_b() const;
    /// @brief 通道状态：idle / running / runningNoOutput
    QString channel_a_state() const { return channel_state(QStringLiteral("A")); }
    QString channel_b_state() const { return channel_state(QStringLiteral("B")); }
    /// @brief A 通道波形是否正在下发
    bool wave_sending_a() const { return wave_sending_a_; }
    /// @brief A 通道当前下发帧序号（1 起，未下发时为 0）
    int wave_frame_a() const { return wave_frame_a_; }
    /// @brief A 通道下发波形总帧数（未下发时为 0）
    int wave_frames_a() const { return wave_frames_a_; }
    /// @brief B 通道波形是否正在下发
    bool wave_sending_b() const { return wave_sending_b_; }
    /// @brief B 通道当前下发帧序号（1 起，未下发时为 0）
    int wave_frame_b() const { return wave_frame_b_; }
    /// @brief B 通道下发波形总帧数（未下发时为 0）
    int wave_frames_b() const { return wave_frames_b_; }
    /// @brief 迷你窗是否已锁定（锁定后不可拖动与调整尺寸）
    bool mini_window_locked() const { return mini_window_locked_; }
    /// @brief 迷你窗上次保存的位置与尺寸，未保存过时为 0
    int mini_window_x() const { return mini_window_x_; }
    int mini_window_y() const { return mini_window_y_; }
    int mini_window_width() const { return mini_window_width_; }
    int mini_window_height() const { return mini_window_height_; }
    /// @brief 迷你窗透明度（0.3~1.0）
    qreal mini_window_opacity() const { return mini_window_opacity_; }
    /// @brief 迷你窗大小档位（small / medium / large）
    QString mini_window_size() const { return mini_window_size_; }
    /// @brief 迷你窗 A 通道滚动波形（最近若干帧的强度均值，0~100）
    QVariantList mini_window_wave_a() const { return mini_window_wave_a_; }
    /// @brief 迷你窗 B 通道滚动波形
    QVariantList mini_window_wave_b() const { return mini_window_wave_b_; }

    /// @brief 保存迷你窗位置、尺寸、透明度与大小档位到 user.json 的 app.ui.mini_window
    void saveMiniWindowState(int x, int y, int width, int height);
    /// @brief 更新迷你窗透明度（只改内存，落盘由 UiConnector 合并）
    void setMiniWindowOpacity(qreal opacity);
    /// @brief 更新迷你窗大小档位 small / medium / large（只改内存，落盘由 UiConnector 合并）
    void setMiniWindowSize(const QString& size);
    /// @brief 更新迷你窗 A/B 滚动波形
    void setMiniWindowWave(const QVariantList& wave_a, const QVariantList& wave_b);

    /// @brief 切换通道启用状态（同步到规则引擎的通道启用变量）
    Q_INVOKABLE void toggleChannel(const QString& channel);
    /// @brief 设置通道目标强度（自动按上限钳制）
    Q_INVOKABLE void setChannelStrength(const QString& channel, int value);
    /// @brief 按增量调整通道强度
    Q_INVOKABLE void adjustChannelStrength(const QString& channel, int delta);
    /// @brief 请求选择通道波形（阶段 6 接入波形库对话框）
    Q_INVOKABLE void requestWaveSelect(const QString& channel);
    /// @brief 切换迷你窗锁定状态并持久化
    Q_INVOKABLE void toggleMiniWindowLock();
    /// @brief 按在线输出设备列表下标切换默认设备
    /// @param index 列表下标，越界时忽略
    Q_INVOKABLE void setDefaultDeviceIndex(int index);

signals:
    void dataChanged();
    void waveStateChanged();
    void miniWindowLockedChanged();
    void channelWaveSelectRequested(const QString& channel);
    void deviceChanged();
    void miniWindowOpacityChanged();
    void miniWindowSizeChanged();
    void miniWindowWaveChanged();

private:
    int channel_index(const QString& channel) const;
    int clamp_strength(const QString& channel, int value) const;
    QString channel_state(const QString& channel) const;
    void apply_strength(const QString& channel, int value);
    QVariantList build_modules(const QString& channel) const;
    QVariantList build_rules(const QString& channel) const;
    /// @brief 重新读取 A/B 波形下发状态并广播
    void refresh_wave_state();

    DeviceController* device_ = nullptr;
    int strength_a_ = 0;
    int strength_b_ = 0;
    bool wave_sending_a_ = false; ///< A 通道是否正在下发波形
    int wave_frame_a_ = 0; ///< A 通道当前下发帧序号
    int wave_frames_a_ = 0; ///< A 通道下发波形总帧数
    bool wave_sending_b_ = false; ///< B 通道是否正在下发波形
    int wave_frame_b_ = 0; ///< B 通道当前下发帧序号
    int wave_frames_b_ = 0; ///< B 通道下发波形总帧数
    bool mini_window_locked_ = false;
    int mini_window_x_ = 0;
    int mini_window_y_ = 0;
    int mini_window_width_ = 0;
    int mini_window_height_ = 0;
    qreal mini_window_opacity_ = 1.0;
    QString mini_window_size_ = QStringLiteral("medium");
    QVariantList mini_window_wave_a_;
    QVariantList mini_window_wave_b_;
    bool mini_window_state_dirty_ = false;
};
