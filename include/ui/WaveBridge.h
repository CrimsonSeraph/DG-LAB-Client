/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "Wave.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include <vector>

class DeviceController;

// WaveBridge - 波形库与波形编辑器桥接（QML 上下文属性 waveBridge）
// 职责：波形库列表、A/B 通道当前波形与预览、目标通道选择，
//      以及波形编辑器草稿（段落列表 -> V3 帧生成 -> 原始帧微调 -> 保存/发送）。
class WaveBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList waves READ waves NOTIFY dataChanged)
    Q_PROPERTY(QString currentA READ current_a NOTIFY dataChanged)
    Q_PROPERTY(QString currentB READ current_b NOTIFY dataChanged)
    Q_PROPERTY(QVariantList pointsA READ points_a NOTIFY dataChanged)
    Q_PROPERTY(QVariantList pointsB READ points_b NOTIFY dataChanged)
    Q_PROPERTY(QVariantList amplitudeA READ amplitude_a NOTIFY dataChanged)
    Q_PROPERTY(QVariantList amplitudeB READ amplitude_b NOTIFY dataChanged)
    Q_PROPERTY(QString targetChannel READ target_channel NOTIFY dataChanged)
    Q_PROPERTY(QVariantList draftSections READ draft_sections NOTIFY draftChanged)
    Q_PROPERTY(int selectedSectionIndex READ selected_section_index NOTIFY draftChanged)
    Q_PROPERTY(int sectionDuration READ section_duration NOTIFY draftChanged)
    Q_PROPERTY(int sectionFreqStart READ section_freq_start NOTIFY draftChanged)
    Q_PROPERTY(int sectionFreqEnd READ section_freq_end NOTIFY draftChanged)
    Q_PROPERTY(int sectionStrengthStart READ section_strength_start NOTIFY draftChanged)
    Q_PROPERTY(int sectionStrengthEnd READ section_strength_end NOTIFY draftChanged)
    Q_PROPERTY(QString draftName READ draft_name NOTIFY draftChanged)
    Q_PROPERTY(QVariantList draftPoints READ draft_points NOTIFY draftChanged)
    Q_PROPERTY(QString draftFramesText READ draft_frames_text NOTIFY draftChanged)
    Q_PROPERTY(int draftFrameCount READ draft_frame_count NOTIFY draftChanged)
    Q_PROPERTY(int draftDurationMs READ draft_duration_ms NOTIFY draftChanged)
    Q_PROPERTY(QString lastError READ last_error NOTIFY draftChanged)
    Q_PROPERTY(bool draftDirty READ draft_dirty NOTIFY draftChanged)
    Q_PROPERTY(QString protocol READ protocol NOTIFY dataChanged)
    Q_PROPERTY(bool draftSendable READ draft_sendable NOTIFY draftChanged)

public:
    explicit WaveBridge(DeviceController* device, QObject* parent = nullptr);

    void initialize();

    QVariantList waves() const;
    QString current_a() const;
    QString current_b() const;
    QVariantList points_a() const;
    QVariantList points_b() const;
    /// @brief 通道 A 当前绑定波形的逐帧强度均值（每帧 4 个强度字节取均值，0~100）
    QVariantList amplitude_a() const;
    /// @brief 通道 B 当前绑定波形的逐帧强度均值（每帧 4 个强度字节取均值，0~100）
    QVariantList amplitude_b() const;
    QString target_channel() const { return target_channel_; }
    QVariantList draft_sections() const;
    int selected_section_index() const { return selected_section_; }
    int section_duration() const;
    int section_freq_start() const;
    int section_freq_end() const;
    int section_strength_start() const;
    int section_strength_end() const;
    QString draft_name() const { return draft_name_; }
    QVariantList draft_points() const;
    QString draft_frames_text() const;
    int draft_frame_count() const { return static_cast<int>(draft_frames_v3_.size()); }
    int draft_duration_ms() const { return static_cast<int>(draft_frames_v3_.size()) * 100; }
    /// @brief 最近一次保存/发送失败的原因（成功后清空）
    QString last_error() const { return last_error_; }
    /// @brief 草稿与已保存内容是否不一致（从未保存过的草稿视为不一致）
    bool draft_dirty() const;
    /// @brief 当前发送协议（"v2" / "v3"，读配置 app.wave.protocol）
    QString protocol() const;
    /// @brief 当前协议下草稿是否具备可发送的帧
    bool draft_sendable() const;

    /// @brief 写入发送协议（只接受 v2 / v3）
    Q_INVOKABLE void setProtocol(const QString& protocol);
    /// @brief 设置选择/指派目标通道（"A"/"B"）
    Q_INVOKABLE void setTargetChannel(const QString& channel);
    /// @brief 将库中波形指派给目标通道
    Q_INVOKABLE void assignWave(const QString& name);
    /// @brief 删除库中波形（内置官方波形不可删除）
    Q_INVOKABLE void deleteWave(const QString& name);
    /// @brief 发送库中波形到指定通道
    Q_INVOKABLE void sendWave(const QString& name, const QString& channel);

    // 编辑器草稿
    Q_INVOKABLE void beginCreate();
    Q_INVOKABLE void addSection();
    Q_INVOKABLE void removeSelectedSection();
    Q_INVOKABLE void selectSection(int index);
    /// @brief 将编辑器中的段落参数写入当前选中段落并重新生成帧
    Q_INVOKABLE void applySectionEdits(int duration, int freqStart, int freqEnd, int strengthStart, int strengthEnd);
    /// @brief 用原始 V3 帧文本替换草稿（每行一个 8 字节 HEX）
    Q_INVOKABLE void applyRawFrames(const QString& text);
    /// @brief 保存草稿到波形库
    Q_INVOKABLE bool saveDraft(const QString& name);
    /// @brief 发送当前草稿到指定通道
    Q_INVOKABLE void sendDraft(const QString& channel);
    /// @brief 从库中波形载入编辑器（复用其段落与 V2/V3 帧）
    Q_INVOKABLE void loadIntoEditor(const QString& name);

signals:
    void dataChanged();
    void draftChanged();
    /// @brief 草稿发送成功（channel 为 A/B，seconds 为波形时长）
    void draftSent(const QString& channel, int seconds);

private:
    QVariantList points_from_frames(const QStringList& frames) const;
    QVariantList amplitudes_from_frames(const QStringList& frames) const;
    /// @brief 按当前协议取库中波形的待发送帧；缺帧时写入错误原因并返回空
    QStringList library_frames_for_send(const Wave& wave, QString& error) const;
    /// @brief 按当前协议取草稿的待发送帧；缺帧时写入错误原因并返回空
    QStringList draft_frames_for_send(QString& error) const;
    void regenerate_frames();
    /// @brief 草稿内容指纹：用于比较草稿是否偏离已保存内容
    QString draft_fingerprint() const;
    QString section_summary(const WaveSection& section) const;
    WaveSection* selected_section();

    DeviceController* device_ = nullptr;
    QString target_channel_ = QStringLiteral("A");
    std::vector<WaveSection> draft_sections_;
    QStringList draft_frames_v2_; ///< 草稿 V2 帧（来自库中波形，段落生成不产出）
    QStringList draft_frames_v3_; ///< 草稿 V3 帧
    int selected_section_ = -1;
    QString draft_name_ = QStringLiteral("新波形");
    QString last_error_;
    QString saved_fingerprint_; ///< 上次保存（或载入）时的草稿指纹
    bool has_saved_ = false;    ///< 草稿是否对应过库中内容
};
