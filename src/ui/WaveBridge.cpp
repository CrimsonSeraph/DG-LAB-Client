/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "WaveBridge.h"

#include "DebugLog.h"
#include "DeviceController.h"
#include "WaveLibrary.h"

#include <QDir>
#include <QStandardPaths>
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
    // 导出文案依赖库内容与通道当前波形：库变化时一并刷新
    connect(&library, &WaveLibrary::library_changed, this, &WaveBridge::exportChanged);
    connect(&library, &WaveLibrary::current_changed, this, &WaveBridge::exportChanged);
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

int WaveBridge::export_format_index() const {
    if (export_format_ == QStringLiteral("pulse")) {
        return 1;
    }
    return export_format_ == QStringLiteral("zip") ? 2 : 0;
}

int WaveBridge::export_scope_index() const {
    return export_scope_ == QStringLiteral("all") ? 1 : 0;
}

bool WaveBridge::export_allowed() const {
    if (export_format_ == QStringLiteral("pulse")) {
        // .pulse 一次只装一个波形：范围必须是当前波形且该通道已绑定波形
        return export_scope_ == QStringLiteral("current") && !target_wave_name().isEmpty();
    }
    if (export_format_ == QStringLiteral("zip")) {
        // zip 是多波形批量归档：范围必须是全部波形且库中有波形
        return export_scope_ == QStringLiteral("all") && !WaveLibrary::instance().names().empty();
    }
    if (export_scope_ == QStringLiteral("all")) {
        return !WaveLibrary::instance().names().empty();
    }
    return !target_wave_name().isEmpty();
}

QString WaveBridge::export_hint() const {
    const QString current = target_wave_name();
    if (export_format_ == QStringLiteral("pulse")) {
        if (export_scope_ == QStringLiteral("all")) {
            return QStringLiteral(
                "「全部波形」只能导出为 JSON：.pulse 一个文件只装一个波形，"
                "请改选「当前波形」逐条导出，或把格式改回 JSON。");
        }
        if (current.isEmpty()) {
            return QStringLiteral("目标通道 %1 还没有绑定波形，无法导出 .pulse。").arg(target_channel_);
        }
        return QStringLiteral("将把通道 %1 的《%2》导出为 .pulse，文件内容为该波形的 raw 原文。")
            .arg(target_channel_, current);
    }
    if (export_format_ == QStringLiteral("zip")) {
        if (export_scope_ != QStringLiteral("all")) {
            return QStringLiteral(
                "zip 是多波形批量归档：请把范围改为「全部波形」。单波形请用 .pulse，"
                "需要 V2 / V3 帧数据请用 JSON。");
        }
        const int count = static_cast<int>(WaveLibrary::instance().names().size());
        return QStringLiteral("将把波形库中全部 %1 个波形导出为一个 zip：每个波形一个 .pulse，"
                              "另附 index.json 名称清单（与官方 App 的 .pulse 互通）。")
            .arg(count);
    }
    if (export_scope_ == QStringLiteral("all")) {
        const int count = static_cast<int>(WaveLibrary::instance().names().size());
        return QStringLiteral("将把波形库中全部 %1 个波形导出为一个 JSON 数组（V2 与 V3 帧同时写出）。")
            .arg(count);
    }
    if (current.isEmpty()) {
        return QStringLiteral("目标通道 %1 还没有绑定波形，请先在波形库中指派一个。").arg(target_channel_);
    }
    return QStringLiteral("将把通道 %1 的《%2》导出为 JSON 数组（V2 与 V3 帧同时写出）。")
        .arg(target_channel_, current);
}

QString WaveBridge::export_suggested_path() const {
    const QString directory = export_directory();
    const QString suffix = export_suffix();
    QString base = target_wave_name();
    if (export_format_ != QStringLiteral("pulse") && export_scope_ == QStringLiteral("all")) {
        base = QStringLiteral("waves");
    }
    if (base.isEmpty()) {
        base = QStringLiteral("wave");
    }
    return directory + QStringLiteral("/") + base + QLatin1Char('.') + suffix;
}

int WaveBridge::conflict_count() const {
    return static_cast<int>(conflict_names_.size());
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

void WaveBridge::setExportFormat(int index) {
    QString format = QStringLiteral("json");
    if (index == 1) {
        format = QStringLiteral("pulse");
    }
    else if (index == 2) {
        format = QStringLiteral("zip");
    }
    if (format == export_format_) {
        return;
    }
    export_format_ = format;
    emit exportChanged();
}

void WaveBridge::setExportScope(int index) {
    const QString scope = (index == 1) ? QStringLiteral("all") : QStringLiteral("current");
    if (scope == export_scope_) {
        return;
    }
    export_scope_ = scope;
    emit exportChanged();
}

void WaveBridge::exportToFile(const QString& path) {
    auto& library = WaveLibrary::instance();
    QString target = path.trimmed();
    if (target.isEmpty()) {
        emit operationFailed(QStringLiteral("未选择导出文件"));
        return;
    }
    // 保存对话框可能补上别的波形扩展名（如按格式切换前的 .json）：先去掉已知后缀再补目标后缀
    const QString suffix = export_suffix();
    for (const QString& known : { QStringLiteral("json"), QStringLiteral("pulse"), QStringLiteral("zip") }) {
        if (target.endsWith(QLatin1Char('.') + known, Qt::CaseInsensitive)) {
            target.chop(known.size() + 1);
            break;
        }
    }
    target += QLatin1Char('.') + suffix;

    QString error;
    QStringList notes;
    bool ok = false;
    if (export_format_ == QStringLiteral("pulse")) {
        const QString name = target_wave_name();
        if (name.isEmpty()) {
            emit operationFailed(QStringLiteral("目标通道没有绑定波形，未导出"));
            return;
        }
        ok = library.export_pulse(target, name.toStdString(), error);
    }
    else if (export_format_ == QStringLiteral("zip")) {
        std::vector<std::string> names;
        if (export_scope_ == QStringLiteral("all")) {
            names = library.names();
        }
        else {
            const QString name = target_wave_name();
            if (name.isEmpty()) {
                emit operationFailed(QStringLiteral("目标通道没有绑定波形，未导出"));
                return;
            }
            names.push_back(name.toStdString());
        }
        ok = library.export_zip(target, names, error, notes);
    }
    else {
        std::vector<std::string> names;
        if (export_scope_ == QStringLiteral("all")) {
            names = library.names();
        }
        else {
            const QString name = target_wave_name();
            if (name.isEmpty()) {
                emit operationFailed(QStringLiteral("目标通道没有绑定波形，未导出"));
                return;
            }
            names.push_back(name.toStdString());
        }
        ok = library.export_json(target, names, error, notes);
    }
    if (!ok) {
        emit operationFailed(error);
        return;
    }
    QString summary = QStringLiteral("已导出 %1").arg(target);
    if (!notes.isEmpty()) {
        summary += QStringLiteral("（%1）").arg(notes.join(QStringLiteral("；")));
    }
    emit exportFinished(summary);
}

void WaveBridge::importFromFile(const QString& path) {
    auto& library = WaveLibrary::instance();
    const WaveImportPlan plan = library.plan_import(path);
    if (!plan.error.isEmpty()) {
        emit operationFailed(plan.error);
        return;
    }
    if (plan.waves.empty()) {
        // 全部条目都解析失败：不弹冲突弹窗，直接把逐条原因汇总回去
        WaveImportResult result;
        result.notes = plan.skipped;
        result.skipped = static_cast<int>(plan.skipped.size());
        emit importFinished(result.summary(), false);
        return;
    }

    pending_import_ = plan;
    has_pending_import_ = true;
    conflict_names_ = plan.conflicts;
    pending_rename_ = QString::fromStdString(library.suggest_name(plan.waves.front().name));
    emit importChanged();
    if (!plan.conflicts.isEmpty()) {
        emit importConflictRequested(static_cast<int>(plan.conflicts.size()));
        return;
    }
    apply_pending_import(QStringLiteral("skip"), QString());
}

void WaveBridge::resolveConflict(const QString& action, const QString& renameTo) {
    if (!has_pending_import_) {
        return;
    }
    apply_pending_import(action, renameTo);
}

void WaveBridge::cancelPendingImport() {
    if (!has_pending_import_) {
        return;
    }
    pending_import_ = WaveImportPlan();
    has_pending_import_ = false;
    conflict_names_.clear();
    pending_rename_.clear();
    emit importChanged();
    LOG_MODULE("WaveBridge", "cancelPendingImport", LOG_INFO, "已取消待写入的导入波形");
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

QString WaveBridge::target_wave_name() const {
    const QString name = WaveLibrary::instance().current(target_channel_.toStdString());
    // 通道记录里可能是已删除（或已移除的占位）波形名：只有库中存在的才算当前波形
    if (name.isEmpty() || WaveLibrary::instance().get(name.toStdString()) == nullptr) {
        return QString();
    }
    return name;
}

QString WaveBridge::export_suffix() const {
    if (export_format_ == QStringLiteral("pulse")) {
        return QStringLiteral("pulse");
    }
    return export_format_ == QStringLiteral("zip") ? QStringLiteral("zip") : QStringLiteral("json");
}

QString WaveBridge::export_directory() {
    QString directory = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (directory.isEmpty()) {
        directory = QDir::homePath();
    }
    return directory;
}

void WaveBridge::apply_pending_import(const QString& action, const QString& rename_to) {
    WaveConflictPolicy policy = WaveConflictPolicy::Skip;
    if (action == QStringLiteral("overwrite")) {
        policy = WaveConflictPolicy::Overwrite;
    }
    else if (action == QStringLiteral("rename")) {
        policy = WaveConflictPolicy::Rename;
    }
    const WaveImportResult result
        = WaveLibrary::instance().apply_import(pending_import_, policy, rename_to);
    pending_import_ = WaveImportPlan();
    has_pending_import_ = false;
    conflict_names_.clear();
    pending_rename_.clear();
    emit importChanged();
    emit importFinished(result.summary(), result.skipped == 0);
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
