/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "WaveBridge.h"

#include "DebugLog.h"
#include "DeviceController.h"
#include "WaveLibrary.h"

#include <QVariantMap>

#include <algorithm>

WaveBridge::WaveBridge(DeviceController* device, QObject* parent)
    : QObject(parent)
    , device_(device) {
}

void WaveBridge::initialize() {
    auto& library = WaveLibrary::instance();
    connect(&library, &WaveLibrary::library_changed, this, &WaveBridge::dataChanged);
    connect(&library, &WaveLibrary::current_changed, this, &WaveBridge::dataChanged);
    beginCreate();
    LOG_MODULE("WaveBridge", "initialize", LOG_INFO, "波形桥接初始化完成");
}

QVariantList WaveBridge::waves() const {
    QVariantList list;
    auto& library = WaveLibrary::instance();
    const QString active_protocol = library.protocol();
    for (const auto& name : library.names()) {
        const Wave* wave = library.get(name);
        if (wave == nullptr) {
            continue;
        }
        QVariantMap item;
        item.insert(QStringLiteral("name"), QString::fromStdString(name));
        item.insert(QStringLiteral("source"), QString::fromLatin1(Wave::source_name(wave->source)));
        item.insert(QStringLiteral("official"), wave->is_official());
        item.insert(QStringLiteral("frameCount"), wave->frame_count());
        item.insert(QStringLiteral("frameCountV2"), static_cast<int>(wave->frames_v2.size()));
        item.insert(QStringLiteral("frameCountV3"), static_cast<int>(wave->frames_v3.size()));
        item.insert(QStringLiteral("durationMs"), wave->duration_ms());
        // 当前协议缺帧时列表项不可发送
        item.insert(QStringLiteral("sendable"), !wave->frames_of(active_protocol).isEmpty());
        list.append(item);
    }
    return list;
}

QString WaveBridge::current_a() const {
    return WaveLibrary::instance().current("A");
}

QString WaveBridge::current_b() const {
    return WaveLibrary::instance().current("B");
}

QVariantList WaveBridge::points_a() const {
    return points_from_frames(WaveLibrary::instance().frames_for("A"));
}

QVariantList WaveBridge::points_b() const {
    return points_from_frames(WaveLibrary::instance().frames_for("B"));
}

QVariantList WaveBridge::points_from_frames(const QStringList& frames) const {
    QVariantList points;
    for (int frame_index = 0; frame_index < frames.size(); ++frame_index) {
        const QString& frame = frames.at(frame_index);
        if (frame.size() < 16) {
            continue;
        }
        for (int unit = 0; unit < 4; ++unit) {
            bool ok_freq = false;
            bool ok_strength = false;
            const int freq = frame.mid(unit * 2, 2).toInt(&ok_freq, 16);
            const int strength = frame.mid(8 + unit * 2, 2).toInt(&ok_strength, 16);
            if (!ok_freq || !ok_strength) {
                continue;
            }
            QVariantMap point;
            point.insert(QStringLiteral("t"), frame_index * 100 + unit * 25);
            point.insert(QStringLiteral("freq"), freq);
            point.insert(QStringLiteral("strength"), strength);
            points.append(point);
        }
    }
    return points;
}

QVariantList WaveBridge::amplitude_a() const {
    return amplitudes_from_frames(WaveLibrary::instance().frames_for("A"));
}

QVariantList WaveBridge::amplitude_b() const {
    return amplitudes_from_frames(WaveLibrary::instance().frames_for("B"));
}

QVariantList WaveBridge::amplitudes_from_frames(const QStringList& frames) const {
    QVariantList amplitudes;
    for (const QString& frame : frames) {
        if (frame.size() < 16) {
            continue;
        }
        int total = 0;
        int count = 0;
        for (int unit = 0; unit < 4; ++unit) {
            bool ok = false;
            const int strength = frame.mid(8 + unit * 2, 2).toInt(&ok, 16);
            if (!ok) {
                continue;
            }
            total += strength;
            ++count;
        }
        if (count > 0) {
            amplitudes.append(total / count);
        }
    }
    return amplitudes;
}

QString WaveBridge::protocol() const {
    return WaveLibrary::instance().protocol();
}

bool WaveBridge::draft_sendable() const {
    const QString active_protocol = WaveLibrary::instance().protocol();
    const QStringList& frames
        = active_protocol == QStringLiteral("v2") ? draft_frames_v2_ : draft_frames_v3_;
    return !frames.isEmpty();
}

void WaveBridge::setProtocol(const QString& protocol) {
    WaveLibrary::instance().set_protocol(protocol);
    last_error_.clear();
    emit dataChanged();
    emit draftChanged();
}

QStringList WaveBridge::library_frames_for_send(const Wave& wave, QString& error) const {
    const QString active_protocol = WaveLibrary::instance().protocol();
    const QStringList frames = wave.frames_of(active_protocol);
    if (frames.isEmpty()) {
        error = QStringLiteral("波形《%1》缺少 %2 帧，未发送")
                    .arg(QString::fromStdString(wave.name), active_protocol.toUpper());
    }
    return frames;
}

QStringList WaveBridge::draft_frames_for_send(QString& error) const {
    const QString active_protocol = WaveLibrary::instance().protocol();
    const QStringList& frames
        = active_protocol == QStringLiteral("v2") ? draft_frames_v2_ : draft_frames_v3_;
    if (frames.isEmpty()) {
        error = QStringLiteral("当前发送协议为 %1，草稿缺少 %1 帧，未发送")
                    .arg(active_protocol.toUpper());
    }
    return frames;
}

void WaveBridge::setTargetChannel(const QString& channel) {
    if (channel != QStringLiteral("A") && channel != QStringLiteral("B")) {
        return;
    }
    target_channel_ = channel;
    emit dataChanged();
}

void WaveBridge::assignWave(const QString& name) {
    if (name.isEmpty()) {
        return;
    }
    WaveLibrary::instance().set_current(target_channel_.toStdString(), name.toStdString());
    LOG_MODULE("WaveBridge", "assignWave", LOG_INFO,
        "通道 " + target_channel_.toStdString() + " 已指派波形: " + name.toStdString());
    emit dataChanged();
}

void WaveBridge::deleteWave(const QString& name) {
    if (name.isEmpty()) {
        return;
    }
    if (WaveLibrary::instance().is_official(name.toStdString())) {
        last_error_ = QStringLiteral("「%1」是内置官方波形，不可删除").arg(name);
        LOG_MODULE("WaveBridge", "deleteWave", LOG_WARN, last_error_.toStdString());
        emit draftChanged();
        return;
    }
    WaveLibrary::instance().remove(name.toStdString());
    emit dataChanged();
}

void WaveBridge::sendWave(const QString& name, const QString& channel) {
    const Wave* wave = WaveLibrary::instance().get(name.toStdString());
    if (wave == nullptr || device_ == nullptr) {
        return;
    }
    QString error;
    const QStringList frames = library_frames_for_send(*wave, error);
    if (frames.isEmpty()) {
        last_error_ = error;
        LOG_MODULE("WaveBridge", "sendWave", LOG_WARN, error.toStdString());
        emit draftChanged();
        return;
    }
    const int channel_index = (channel == QStringLiteral("B")) ? 2 : 1;
    const int seconds = std::max(1, (static_cast<int>(frames.size()) * 100 + 999) / 1000);
    device_->sendWave(channel_index, frames, seconds);
    WaveLibrary::instance().set_current(channel.toStdString(), name.toStdString());
    last_error_.clear();
    LOG_MODULE("WaveBridge", "sendWave", LOG_INFO, "已发送波形: " + name.toStdString());
    emit dataChanged();
    emit draftChanged();
}

void WaveBridge::beginCreate() {
    WaveSection section;
    draft_sections_ = { section };
    selected_section_ = 0;
    draft_name_ = QStringLiteral("新波形");
    // 新草稿还没落库：保留「未保存」语义，并清掉上一份草稿遗留的错误
    has_saved_ = false;
    saved_fingerprint_.clear();
    last_error_.clear();
    regenerate_frames();
    emit draftChanged();
}

void WaveBridge::addSection() {
    WaveSection section;
    if (!draft_sections_.empty() && selected_section_ >= 0) {
        const WaveSection& last = draft_sections_[static_cast<size_t>(selected_section_)];
        section = last;
        section.freq_start = last.freq_end;
        section.strength_start = last.strength_end;
        section.freq_end = last.freq_end;
        section.strength_end = last.strength_end;
    }
    draft_sections_.push_back(section);
    selected_section_ = static_cast<int>(draft_sections_.size()) - 1;
    regenerate_frames();
    emit draftChanged();
}

void WaveBridge::removeSelectedSection() {
    if (selected_section_ < 0 || selected_section_ >= static_cast<int>(draft_sections_.size())) {
        return;
    }
    if (draft_sections_.size() <= 1) {
        return;
    }
    draft_sections_.erase(draft_sections_.begin() + selected_section_);
    selected_section_ = std::min(selected_section_, static_cast<int>(draft_sections_.size()) - 1);
    regenerate_frames();
    emit draftChanged();
}

void WaveBridge::selectSection(int index) {
    if (index < 0 || index >= static_cast<int>(draft_sections_.size())) {
        return;
    }
    selected_section_ = index;
    emit draftChanged();
}

WaveSection* WaveBridge::selected_section() {
    if (selected_section_ < 0 || selected_section_ >= static_cast<int>(draft_sections_.size())) {
        return nullptr;
    }
    return &draft_sections_[static_cast<size_t>(selected_section_)];
}

void WaveBridge::applySectionEdits(int duration, int freqStart, int freqEnd, int strengthStart,
    int strengthEnd) {
    WaveSection* section = selected_section();
    if (section == nullptr) {
        return;
    }
    section->duration_ms = std::clamp(duration, 100, 60000);
    section->freq_start = std::clamp(freqStart, 10, 240);
    section->freq_end = std::clamp(freqEnd, 10, 240);
    section->strength_start = std::clamp(strengthStart, 0, 100);
    section->strength_end = std::clamp(strengthEnd, 0, 100);
    regenerate_frames();
    emit draftChanged();
}

void WaveBridge::applyRawFrames(const QString& text) {
    QStringList frames;
    const QStringList lines = text.split(QRegularExpression(QStringLiteral("[\\s,;]+")), Qt::SkipEmptyParts);
    for (const auto& line : lines) {
        const QString frame = line.trimmed().toUpper();
        if (frame.size() == 16 && QRegularExpression(QStringLiteral("^[0-9A-F]{16}$")).match(frame).hasMatch()) {
            frames.append(frame);
        }
    }
    if (frames.isEmpty()) {
        return;
    }
    draft_sections_.clear();
    selected_section_ = -1;
    draft_frames_v2_.clear();
    draft_frames_v3_ = frames;
    emit draftChanged();
}

bool WaveBridge::saveDraft(const QString& name) {
    if (name.isEmpty()) {
        last_error_ = QStringLiteral("波形名称为空，请填写名称后再保存");
        emit draftChanged();
        return false;
    }
    if (WaveLibrary::instance().is_official(name.toStdString())) {
        last_error_ = QStringLiteral("「%1」是内置官方波形，不可覆盖，请换一个名称").arg(name);
        emit draftChanged();
        return false;
    }
    if (draft_frames_v3_.isEmpty() && draft_frames_v2_.isEmpty()) {
        last_error_ = QStringLiteral("当前草稿没有可保存的波形帧");
        emit draftChanged();
        return false;
    }
    Wave wave;
    wave.name = name.toStdString();
    wave.source = WaveSource::User;
    wave.sections = draft_sections_;
    wave.frames_v2 = draft_frames_v2_;
    wave.frames_v3 = draft_frames_v3_;
    draft_name_ = name;
    const bool ok = WaveLibrary::instance().save(wave);
    if (ok) {
        last_error_.clear();
        has_saved_ = true;
        saved_fingerprint_ = draft_fingerprint();
    }
    else {
        last_error_ = QStringLiteral("写入波形库失败：") + name;
    }
    emit draftChanged();
    return ok;
}

void WaveBridge::sendDraft(const QString& channel) {
    const QString channel_label = (channel == QStringLiteral("B")) ? QStringLiteral("B") : QStringLiteral("A");
    if (device_ == nullptr) {
        last_error_ = QStringLiteral("设备未就绪，波形未发送");
        emit draftChanged();
        return;
    }
    QString error;
    const QStringList frames = draft_frames_for_send(error);
    if (frames.isEmpty()) {
        last_error_ = error;
        LOG_MODULE("WaveBridge", "sendDraft", LOG_WARN, error.toStdString());
        emit draftChanged();
        return;
    }
    const int channel_index = (channel_label == QStringLiteral("B")) ? 2 : 1;
    const int seconds = std::max(1, (static_cast<int>(frames.size()) * 100 + 999) / 1000);
    device_->sendWave(channel_index, frames, seconds);
    last_error_.clear();
    LOG_MODULE("WaveBridge", "sendDraft", LOG_INFO, "已发送草稿波形");
    emit draftChanged();
    emit draftSent(channel_label, seconds);
}

void WaveBridge::loadIntoEditor(const QString& name) {
    const Wave* wave = WaveLibrary::instance().get(name.toStdString());
    if (wave == nullptr) {
        return;
    }
    draft_name_ = name;
    draft_sections_ = wave->sections;
    draft_frames_v2_ = wave->frames_v2;
    draft_frames_v3_ = wave->frames_v3;
    selected_section_ = draft_sections_.empty() ? -1 : 0;
    // 载入的正是库中内容：草稿与已保存内容一致
    has_saved_ = true;
    saved_fingerprint_ = draft_fingerprint();
    last_error_.clear();
    emit draftChanged();
}

bool WaveBridge::draft_dirty() const {
    return !has_saved_ || draft_fingerprint() != saved_fingerprint_;
}

QString WaveBridge::draft_fingerprint() const {
    QString fingerprint = draft_name_;
    for (const auto& section : draft_sections_) {
        fingerprint += QStringLiteral("|%1,%2,%3,%4,%5")
                           .arg(section.duration_ms)
                           .arg(section.freq_start)
                           .arg(section.freq_end)
                           .arg(section.strength_start)
                           .arg(section.strength_end);
    }
    fingerprint += QLatin1Char('#');
    fingerprint += draft_frames_v2_.join(QLatin1Char(','));
    fingerprint += QLatin1Char('#');
    fingerprint += draft_frames_v3_.join(QLatin1Char(','));
    return fingerprint;
}

void WaveBridge::regenerate_frames() {
    if (draft_sections_.empty()) {
        return;
    }
    // 段落只生成 V3 帧，V2 帧置空由发送前按协议校验
    draft_frames_v2_.clear();
    draft_frames_v3_ = Wave::generate_frames(draft_sections_);
}

QString WaveBridge::section_summary(const WaveSection& section) const {
    return QStringLiteral("%1ms  %2->%3Hz  %4->%5")
        .arg(section.duration_ms)
        .arg(section.freq_start)
        .arg(section.freq_end)
        .arg(section.strength_start)
        .arg(section.strength_end);
}

QVariantList WaveBridge::draft_sections() const {
    QVariantList list;
    for (int index = 0; index < static_cast<int>(draft_sections_.size()); ++index) {
        const WaveSection& section = draft_sections_[static_cast<size_t>(index)];
        QVariantMap item;
        item.insert(QStringLiteral("index"), index);
        item.insert(QStringLiteral("summary"), section_summary(section));
        item.insert(QStringLiteral("durationMs"), section.duration_ms);
        item.insert(QStringLiteral("freqStart"), section.freq_start);
        item.insert(QStringLiteral("freqEnd"), section.freq_end);
        item.insert(QStringLiteral("strengthStart"), section.strength_start);
        item.insert(QStringLiteral("strengthEnd"), section.strength_end);
        item.insert(QStringLiteral("selected"), index == selected_section_);
        list.append(item);
    }
    return list;
}

int WaveBridge::section_duration() const {
    const WaveSection* section = const_cast<WaveBridge*>(this)->selected_section();
    return section == nullptr ? 1000 : section->duration_ms;
}

int WaveBridge::section_freq_start() const {
    const WaveSection* section = const_cast<WaveBridge*>(this)->selected_section();
    return section == nullptr ? 50 : section->freq_start;
}

int WaveBridge::section_freq_end() const {
    const WaveSection* section = const_cast<WaveBridge*>(this)->selected_section();
    return section == nullptr ? 50 : section->freq_end;
}

int WaveBridge::section_strength_start() const {
    const WaveSection* section = const_cast<WaveBridge*>(this)->selected_section();
    return section == nullptr ? 50 : section->strength_start;
}

int WaveBridge::section_strength_end() const {
    const WaveSection* section = const_cast<WaveBridge*>(this)->selected_section();
    return section == nullptr ? 50 : section->strength_end;
}

QVariantList WaveBridge::draft_points() const {
    return points_from_frames(draft_frames_v3_);
}

QString WaveBridge::draft_frames_text() const {
    return draft_frames_v3_.join(QStringLiteral("\n"));
}
