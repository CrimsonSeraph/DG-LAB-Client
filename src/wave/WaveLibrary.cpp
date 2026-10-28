/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "WaveLibrary.h"

#include "AppConfig.h"
#include "DebugLog.h"
#include "WaveImportExport.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>

#include <exception>
#include <fstream>

namespace {

    /// @brief 官方内置波形资源路径（qrc，只读）
    const char* const OFFICIAL_RESOURCE = ":/waves/assets/waves/official_waves.json";

    /// @brief 旧版内置占位波形名：加载时移除
    const char* const REMOVED_PLACEHOLDERS[] = { "渐强脉冲", "低频涌动" };

    /// @brief 发送协议配置键
    const char* const PROTOCOL_KEY = "app.wave.protocol";

    /// @brief 协议取值规范化：只接受 v2 / v3，其余返回空串
    QString normalize_protocol(const QString& value) {
        const QString lower = value.trimmed().toLower();
        if (lower == QStringLiteral("v2") || lower == QStringLiteral("v3")) {
            return lower;
        }
        return QString();
    }

    /// @brief 自定义标签库文件名
    const char* const TAGS_FILE_NAME = "tags.json";

    /// @brief 标签库文件结构版本
    const int TAGS_FILE_VERSION = 1;

    /// @brief 内置标签存储：代码常量随版本走；本批为空，官方 App 的标签清单到位后在此逐条追加
    QStringList& builtin_tags_storage() {
        static QStringList tags;
        return tags;
    }

    /// @brief 名称在列表中的下标（大小写不敏感），未找到返回 -1
    int index_of_tag(const QStringList& tags, const QString& tag) {
        for (int index = 0; index < tags.size(); ++index) {
            if (tags.at(index).compare(tag, Qt::CaseInsensitive) == 0) {
                return index;
            }
        }
        return -1;
    }

    /// @brief 逐条规范化 JSON 帧数组；任一帧不合规即判整条失败
    bool read_frames_strict(const nlohmann::json& array, int byte_count, QStringList& out) {
        out.clear();
        if (!array.is_array() || array.empty()) {
            return false;
        }
        for (const auto& item : array) {
            if (!item.is_string()) {
                return false;
            }
            const QString frame
                = Wave::normalize_hex_frame(QString::fromStdString(item.get<std::string>()), byte_count);
            if (frame.isEmpty()) {
                return false;
            }
            out.append(frame);
        }
        return true;
    }

    /// @brief zip 归档内的名称清单文件名
    const char* const INDEX_ENTRY_NAME = "index.json";

    /// @brief 以二进制写入文件
    bool write_bytes_file(const QString& path, const QByteArray& data, QString& error) {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            error = QStringLiteral("无法写入文件：%1").arg(path);
            return false;
        }
        const qint64 written = file.write(data);
        file.close();
        if (written != data.size()) {
            error = QStringLiteral("写入文件失败：%1").arg(path);
            return false;
        }
        return true;
    }

    /// @brief 以 UTF-8 无 BOM 写入文本文件
    bool write_text_file(const QString& path, const QString& text, QString& error) {
        return write_bytes_file(path, text.toUtf8(), error);
    }

    /// @brief 波形名转换为安全的文件名主干：非法字符替换为下划线，去掉结尾的点与空白
    QString sanitize_file_stem(const QString& name) {
        QString stem;
        for (const QChar ch : name) {
            if (ch.unicode() < 0x20 || QStringLiteral("\\/:*?\"<>|").contains(ch)) {
                stem.append(QLatin1Char('_'));
            }
            else {
                stem.append(ch);
            }
        }
        stem = stem.trimmed();
        while (stem.endsWith(QLatin1Char('.')) || stem.endsWith(QLatin1Char(' '))) {
            stem.chop(1);
        }
        if (stem.isEmpty()) {
            stem = QStringLiteral("wave");
        }
        return stem;
    }

    /// @brief 生成归档内不重复的文件名主干（重复时追加 (2)、(3)…，比较忽略大小写）
    QString unique_file_stem(const QString& stem, QStringList& used) {
        QString candidate = stem;
        int index = 2;
        while (used.contains(candidate, Qt::CaseInsensitive)) {
            candidate = QStringLiteral("%1 (%2)").arg(stem).arg(index);
            ++index;
        }
        used.append(candidate);
        return candidate;
    }

    /// @brief 读取 zip 内 index.json 清单：条目名到波形名的映射与清单列出的文件顺序
    void read_index_manifest(const QString& text, QHash<QString, QString>& names, QStringList& files) {
        names.clear();
        files.clear();
        nlohmann::json root;
        try {
            root = nlohmann::json::parse(text.toStdString());
        }
        catch (const std::exception&) {
            return;
        }
        const auto waves = root.find("waves");
        if (waves == root.end() || !waves->is_array()) {
            return;
        }
        for (const auto& item : *waves) {
            if (!item.is_object()) {
                continue;
            }
            const auto file = item.find("file");
            if (file == item.end() || !file->is_string()) {
                continue;
            }
            const QString file_name = QString::fromStdString(file->get<std::string>());
            if (file_name.isEmpty()) {
                continue;
            }
            files.append(file_name);
            const auto name = item.find("name");
            if (name != item.end() && name->is_string()) {
                const QString wave_name = QString::fromStdString(name->get<std::string>());
                if (!wave_name.isEmpty()) {
                    names.insert(file_name, wave_name);
                }
            }
        }
    }

} // namespace

QString WaveImportResult::summary() const {
    const int total = added + overwritten + renamed + skipped;
    QString text = QStringLiteral("导入 %1 条：新增 %2 · 覆盖 %3 · 重命名 %4 · 跳过 %5")
                       .arg(total)
                       .arg(added)
                       .arg(overwritten)
                       .arg(renamed)
                       .arg(skipped);
    if (warnings.isEmpty()) {
        return text;
    }
    // Toast 只有一行宽度：逐条原因最多展开两条，其余折叠成计数
    const QStringList shown = warnings.mid(0, 2);
    QString details = shown.join(QStringLiteral("；"));
    if (warnings.size() > shown.size()) {
        details += QStringLiteral("；等 %1 条").arg(warnings.size());
    }
    text += QStringLiteral("：") + details;
    return text;
}

WaveLibrary& WaveLibrary::instance() {
    static WaveLibrary library;
    return library;
}

void WaveLibrary::initialize() {
    if (initialized_) {
        return;
    }
    reload();
    initialized_ = true;
}

void WaveLibrary::reload() {
    dir_ = AppConfig::instance().get_value<std::string>("wave.path", "./config/waves");
    try {
        QDir().mkpath(QString::fromStdString(dir_));
    }
    catch (const std::exception& e) {
        LOG_MODULE("WaveLibrary", "reload", LOG_ERROR, "创建波形目录失败: " << e.what());
    }
    load();
    emit library_changed();
    emit current_changed();
    LOG_MODULE("WaveLibrary", "reload", LOG_INFO,
        "波形库已加载，共 " << waves_.size() << " 个波形（官方 " << official_count() << " 个）");
}

std::vector<std::string> WaveLibrary::names() const {
    std::vector<std::string> result;
    result.reserve(waves_.size());
    for (const auto& [name, _] : waves_) {
        result.push_back(name);
    }
    return result;
}

const Wave* WaveLibrary::get(const std::string& name) const {
    auto it = waves_.find(name);
    return it == waves_.end() ? nullptr : &it->second;
}

bool WaveLibrary::has(const std::string& name) const {
    return waves_.find(name) != waves_.end();
}

bool WaveLibrary::is_official(const std::string& name) const {
    const Wave* wave = get(name);
    return wave != nullptr && wave->is_official();
}

int WaveLibrary::official_count() const {
    int count = 0;
    for (const auto& [_, wave] : waves_) {
        if (wave.is_official()) {
            ++count;
        }
    }
    return count;
}

bool WaveLibrary::save(const Wave& wave) {
    if (wave.name.empty()) {
        return false;
    }
    if (is_official(wave.name)) {
        LOG_MODULE("WaveLibrary", "save", LOG_WARN, "内置官方波形不可覆盖: " << wave.name);
        return false;
    }
    Wave stored = wave;
    stored.source = WaveSource::User;
    stored.tags = Wave::normalize_tags(stored.tags);
    waves_[stored.name] = stored;
    persist();
    emit library_changed();
    LOG_MODULE("WaveLibrary", "save", LOG_INFO, "已保存波形: " << wave.name);
    return true;
}

bool WaveLibrary::remove(const std::string& name) {
    auto it = waves_.find(name);
    if (it == waves_.end()) {
        return false;
    }
    if (it->second.is_official()) {
        LOG_MODULE("WaveLibrary", "remove", LOG_WARN, "内置官方波形不可删除: " << name);
        return false;
    }
    waves_.erase(it);
    if (current_a_ == name) {
        current_a_.clear();
    }
    if (current_b_ == name) {
        current_b_.clear();
    }
    persist();
    emit library_changed();
    emit current_changed();
    return true;
}

QString WaveLibrary::protocol() const {
    const QString stored
        = QString::fromStdString(AppConfig::instance().get_value<std::string>(PROTOCOL_KEY, "v3"));
    const QString normalized = normalize_protocol(stored);
    return normalized.isEmpty() ? QStringLiteral("v3") : normalized;
}

void WaveLibrary::set_protocol(const QString& protocol) {
    const QString normalized = normalize_protocol(protocol);
    if (normalized.isEmpty()) {
        LOG_MODULE("WaveLibrary", "set_protocol", LOG_WARN,
            "无效的波形发送协议: " << protocol.toStdString());
        return;
    }
    AppConfig::instance().set_value(PROTOCOL_KEY, normalized.toStdString());
    LOG_MODULE("WaveLibrary", "set_protocol", LOG_INFO,
        "波形发送协议已切换为 " << normalized.toStdString());
}

QString WaveLibrary::current(const std::string& channel) const {
    const std::string& name = (channel == "A") ? current_a_ : current_b_;
    return QString::fromStdString(name);
}

void WaveLibrary::set_current(const std::string& channel, const std::string& name) {
    if (channel == "A") {
        current_a_ = name;
    }
    else if (channel == "B") {
        current_b_ = name;
    }
    else {
        return;
    }
    persist();
    emit current_changed();
}

QStringList WaveLibrary::frames_for(const std::string& channel) const {
    const QString name = current(channel);
    const Wave* wave = name.isEmpty() ? nullptr : get(name.toStdString());
    return wave == nullptr ? QStringList() : wave->frames_v3;
}

QStringList WaveLibrary::frames_of(const std::string& name, const QString& protocol) const {
    const Wave* wave = get(name);
    if (wave == nullptr) {
        return QStringList();
    }
    const QString normalized = normalize_protocol(protocol);
    if (normalized.isEmpty()) {
        return QStringList();
    }
    return wave->frames_of(normalized);
}

int WaveLibrary::frame_count(const std::string& name) const {
    const Wave* wave = get(name);
    return wave == nullptr ? 0 : wave->frame_count();
}

int WaveLibrary::duration_ms(const std::string& name) const {
    const Wave* wave = get(name);
    return wave == nullptr ? 0 : wave->duration_ms();
}

QStringList WaveLibrary::builtin_tags() const {
    return Wave::normalize_tags(builtin_tags_storage());
}

QStringList WaveLibrary::custom_tags() const {
    return custom_tags_;
}

QStringList WaveLibrary::all_tags() const {
    QStringList result = builtin_tags();
    for (const QString& tag : custom_tags_) {
        if (index_of_tag(result, tag) < 0) {
            result.append(tag);
        }
    }
    // 波形上引用了但没入库的标签（手工编辑过 waves.json / 删过 tags.json）同样列出来，避免选中态无法取消
    for (const auto& [_, wave] : waves_) {
        for (const QString& tag : wave.tags) {
            const QString normalized = Wave::normalize_tag(tag);
            if (!normalized.isEmpty() && index_of_tag(result, normalized) < 0) {
                result.append(normalized);
            }
        }
    }
    return result;
}

bool WaveLibrary::is_builtin_tag(const QString& tag) const {
    const QString normalized = Wave::normalize_tag(tag);
    return !normalized.isEmpty() && index_of_tag(builtin_tags(), normalized) >= 0;
}

QString WaveLibrary::canonical_tag(const QString& tag) const {
    const QString normalized = Wave::normalize_tag(tag);
    if (normalized.isEmpty()) {
        return QString();
    }
    const QStringList builtin = builtin_tags();
    const int builtin_index = index_of_tag(builtin, normalized);
    if (builtin_index >= 0) {
        return builtin.at(builtin_index);
    }
    const int custom_index = index_of_tag(custom_tags_, normalized);
    if (custom_index >= 0) {
        return custom_tags_.at(custom_index);
    }
    return normalized;
}

int WaveLibrary::tag_usage(const QString& tag) const {
    const QString normalized = Wave::normalize_tag(tag);
    if (normalized.isEmpty()) {
        return 0;
    }
    int count = 0;
    for (const auto& [_, wave] : waves_) {
        if (wave.has_tag(normalized)) {
            ++count;
        }
    }
    return count;
}

bool WaveLibrary::add_custom_tag(const QString& tag, QString* error) {
    const QString normalized = Wave::normalize_tag(tag);
    if (normalized.isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("标签名不能为空，且最长 %1 个字符").arg(Wave::TAG_MAX_LENGTH);
        }
        return false;
    }
    // 已存在（含大小写不同的写法）时沿用库里的写法，不产生第二条
    if (is_builtin_tag(normalized) || index_of_tag(custom_tags_, normalized) >= 0) {
        return true;
    }
    custom_tags_.append(normalized);
    persist_tags();
    emit tags_changed();
    LOG_MODULE("WaveLibrary", "add_custom_tag", LOG_INFO, "已新增自定义标签: " << normalized.toStdString());
    return true;
}

int WaveLibrary::remove_custom_tag(const QString& tag, QString* error) {
    const QString normalized = Wave::normalize_tag(tag);
    if (normalized.isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("标签名不合法，未删除");
        }
        return -1;
    }
    if (is_builtin_tag(normalized)) {
        if (error != nullptr) {
            *error = QStringLiteral("「%1」是内置标签，不可删除").arg(normalized);
        }
        LOG_MODULE("WaveLibrary", "remove_custom_tag", LOG_WARN,
            "内置标签不可删除: " << normalized.toStdString());
        return -1;
    }
    const int index = index_of_tag(custom_tags_, normalized);
    if (index < 0) {
        if (error != nullptr) {
            *error = QStringLiteral("自定义标签库里没有「%1」").arg(normalized);
        }
        return -1;
    }
    custom_tags_.removeAt(index);
    persist_tags();

    // 删除标签同时把它从使用中的波形上移除：界面上的标签始终指向存在的库条目，不留悬空引用
    int detached = 0;
    for (auto& [_, wave] : waves_) {
        if (!wave.has_tag(normalized)) {
            continue;
        }
        QStringList kept;
        for (const QString& existing : wave.tags) {
            if (existing.compare(normalized, Qt::CaseInsensitive) != 0) {
                kept.append(existing);
            }
        }
        wave.tags = kept;
        ++detached;
    }
    if (detached > 0) {
        persist();
    }
    emit tags_changed();
    if (detached > 0) {
        emit library_changed();
    }
    LOG_MODULE("WaveLibrary", "remove_custom_tag", LOG_INFO,
        "已删除自定义标签: " << normalized.toStdString() << "，从 " << detached << " 个波形上移除");
    return detached;
}

bool WaveLibrary::attach_tag(const std::string& wave_name, const QString& tag, QString* error) {
    auto it = waves_.find(wave_name);
    if (it == waves_.end()) {
        if (error != nullptr) {
            *error = QStringLiteral("波形《%1》不在波形库中").arg(QString::fromStdString(wave_name));
        }
        return false;
    }
    if (it->second.is_official()) {
        if (error != nullptr) {
            *error = QStringLiteral("《%1》是内置官方波形，不可加标签").arg(QString::fromStdString(wave_name));
        }
        return false;
    }
    const QString normalized = Wave::normalize_tag(tag);
    if (normalized.isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("标签名不能为空，且最长 %1 个字符").arg(Wave::TAG_MAX_LENGTH);
        }
        return false;
    }
    if (it->second.has_tag(normalized)) {
        return true;
    }
    it->second.tags.append(canonical_tag(normalized));
    persist();
    emit library_changed();
    emit tags_changed();
    return true;
}

bool WaveLibrary::detach_tag(const std::string& wave_name, const QString& tag) {
    auto it = waves_.find(wave_name);
    if (it == waves_.end() || it->second.is_official()) {
        return false;
    }
    const QString normalized = Wave::normalize_tag(tag);
    if (normalized.isEmpty() || !it->second.has_tag(normalized)) {
        return false;
    }
    QStringList kept;
    for (const QString& existing : it->second.tags) {
        if (existing.compare(normalized, Qt::CaseInsensitive) != 0) {
            kept.append(existing);
        }
    }
    it->second.tags = kept;
    persist();
    emit library_changed();
    emit tags_changed();
    return true;
}

int WaveLibrary::register_tags(const QStringList& tags) {
    int added = 0;
    for (const QString& tag : tags) {
        const QString normalized = Wave::normalize_tag(tag);
        if (normalized.isEmpty() || is_builtin_tag(normalized)
            || index_of_tag(custom_tags_, normalized) >= 0) {
            continue;
        }
        custom_tags_.append(normalized);
        ++added;
    }
    if (added > 0) {
        persist_tags();
        emit tags_changed();
    }
    return added;
}

void WaveLibrary::set_builtin_tags_for_test(const QStringList& tags) {
    builtin_tags_storage() = Wave::normalize_tags(tags);
    emit tags_changed();
}

WaveImportPlan WaveLibrary::plan_import(const QString& path) const {
    WaveImportPlan plan;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        plan.error = QStringLiteral("无法读取文件：%1").arg(path);
        LOG_MODULE("WaveLibrary", "plan_import", LOG_ERROR, plan.error.toStdString());
        return plan;
    }
    const QByteArray data = file.readAll();
    file.close();
    const QString text = QString::fromUtf8(data);
    const QFileInfo info(path);
    const QString suffix = info.suffix().toLower();
    if (suffix == QStringLiteral("json")) {
        wave_io::parse_json(text, plan.waves, plan.skipped, plan.error);
    }
    else if (suffix == QStringLiteral("pulse")) {
        Wave wave;
        QString error;
        if (wave_io::parse_pulse(text, info.completeBaseName(), wave, error)) {
            plan.waves.push_back(wave);
        }
        else {
            plan.error = error;
        }
    }
    else if (suffix == QStringLiteral("zip")) {
        std::vector<wave_io::ZipEntry> entries;
        QStringList zip_skipped;
        QString zip_error;
        if (!wave_io::parse_zip(data, entries, zip_skipped, zip_error)) {
            plan.error = zip_error;
        }
        else {
            plan.skipped = zip_skipped;
            // index.json 清单不是波形：读它只为恢复被文件名字符替换掉的原名称
            QHash<QString, QString> manifest_names;
            QStringList manifest_files;
            for (const wave_io::ZipEntry& entry : entries) {
                if (entry.name.compare(QString::fromLatin1(INDEX_ENTRY_NAME), Qt::CaseInsensitive) == 0) {
                    read_index_manifest(QString::fromUtf8(entry.data), manifest_names, manifest_files);
                    break;
                }
            }
            for (const wave_io::ZipEntry& entry : entries) {
                if (entry.name.compare(QString::fromLatin1(INDEX_ENTRY_NAME), Qt::CaseInsensitive) == 0) {
                    continue;
                }
                const QFileInfo entry_info(entry.name);
                if (entry_info.suffix().compare(QStringLiteral("pulse"), Qt::CaseInsensitive) != 0) {
                    plan.skipped.append(QStringLiteral("「%1」不是 .pulse 文件，已跳过").arg(entry.name));
                    continue;
                }
                const QString file_name = entry_info.fileName();
                const QString wave_name = manifest_names.value(entry.name, entry_info.completeBaseName());
                Wave wave;
                QString reason;
                if (wave_io::parse_pulse(QString::fromUtf8(entry.data), wave_name, wave, reason)) {
                    plan.waves.push_back(wave);
                }
                else {
                    plan.skipped.append(QStringLiteral("「%1」导入失败：%2").arg(file_name, reason));
                }
            }
            for (const QString& file : manifest_files) {
                bool present = false;
                for (const wave_io::ZipEntry& entry : entries) {
                    if (entry.name.compare(file, Qt::CaseInsensitive) == 0) {
                        present = true;
                        break;
                    }
                }
                if (!present) {
                    plan.skipped.append(QStringLiteral("index.json 列出的「%1」在包内不存在，已跳过").arg(file));
                }
            }
        }
    }
    else {
        plan.error = QStringLiteral("不支持的文件类型：.%1（只支持 .json / .pulse / .zip）").arg(suffix);
    }
    if (!plan.error.isEmpty()) {
        LOG_MODULE("WaveLibrary", "plan_import", LOG_ERROR, plan.error.toStdString());
        return plan;
    }
    for (const auto& wave : plan.waves) {
        if (has(wave.name)) {
            plan.conflicts.append(QString::fromStdString(wave.name));
        }
    }
    LOG_MODULE("WaveLibrary", "plan_import", LOG_INFO,
        "导入解析完成：" << plan.waves.size() << " 条可写入，" << plan.skipped.size() << " 条跳过，"
                         << plan.conflicts.size() << " 条重名");
    return plan;
}

WaveImportResult WaveLibrary::apply_import(const WaveImportPlan& plan, WaveConflictPolicy policy,
    const QString& rename_to) {
    WaveImportResult result;
    result.notes = plan.skipped;
    result.warnings = plan.skipped;
    result.skipped = static_cast<int>(plan.skipped.size());
    const bool single = plan.waves.size() == 1;
    const QString requested = rename_to.trimmed();
    bool changed = false;
    QStringList written_tags; ///< 已写入波形的标签（导入后自动登记进自定义标签库）

    for (Wave wave : plan.waves) {
        wave.source = WaveSource::User;
        const QString original = QString::fromStdString(wave.name);

        if (!has(wave.name)) {
            waves_[wave.name] = wave;
            ++result.added;
            changed = true;
        }
        else if (policy == WaveConflictPolicy::Overwrite) {
            if (is_official(wave.name)) {
                ++result.skipped;
                const QString reason
                    = QStringLiteral("《%1》是内置官方波形，不可覆盖，已跳过").arg(original);
                result.notes.append(reason);
                result.warnings.append(reason);
                continue;
            }
            waves_[wave.name] = wave;
            ++result.overwritten;
            result.notes.append(QStringLiteral("《%1》已覆盖库中同名波形").arg(original));
            changed = true;
        }
        else if (policy == WaveConflictPolicy::Rename) {
            std::string target = (single && !requested.isEmpty()) ? requested.toStdString()
                                                                  : suggest_name(wave.name);
            if (has(target) || is_official(target)) {
                target = suggest_name(wave.name);
            }
            if (has(target) || is_official(target)) {
                ++result.skipped;
                const QString reason
                    = QStringLiteral("《%1》没有可用的新名称，已跳过").arg(original);
                result.notes.append(reason);
                result.warnings.append(reason);
                continue;
            }
            wave.name = target;
            waves_[target] = wave;
            ++result.renamed;
            result.notes.append(QStringLiteral("《%1》已重命名为《%2》")
                                    .arg(original, QString::fromStdString(target)));
            changed = true;
        }
        else {
            ++result.skipped;
            const QString reason = QStringLiteral("《%1》已存在同名波形，已跳过").arg(original);
            result.notes.append(reason);
            result.warnings.append(reason);
            continue;
        }

        if (wave.frames_v2.isEmpty() && wave.frames_v3.isEmpty()) {
            const QString warning = QStringLiteral("《%1》缺少帧，已标记为不可发送")
                                        .arg(QString::fromStdString(wave.name));
            result.notes.append(warning);
            result.warnings.append(warning);
        }
        written_tags.append(wave.tags);
    }

    if (changed) {
        persist();
        emit library_changed();
    }
    // JSON 导入里出现且不属于内置标签的标签名自动登记（.pulse 与 zip 不带标签，天然不受影响）
    const int registered = register_tags(written_tags);
    if (registered > 0) {
        LOG_MODULE("WaveLibrary", "apply_import", LOG_INFO, "导入时自动登记自定义标签 " << registered << " 个");
    }
    LOG_MODULE("WaveLibrary", "apply_import", LOG_INFO,
        "导入写入完成：新增 " << result.added << " 覆盖 " << result.overwritten << " 重命名 "
                              << result.renamed << " 跳过 " << result.skipped);
    return result;
}

std::string WaveLibrary::suggest_name(const std::string& name) const {
    for (int index = 2; index <= 999; ++index) {
        const std::string candidate = name + " (" + std::to_string(index) + ")";
        if (!has(candidate)) {
            return candidate;
        }
    }
    return name + " (副本)";
}

bool WaveLibrary::export_json(const QString& path, const std::vector<std::string>& names, QString& error,
    QStringList& notes) const {
    error.clear();
    notes.clear();
    std::vector<Wave> waves;
    if (names.empty()) {
        for (const auto& [name, wave] : waves_) {
            waves.push_back(wave);
        }
    }
    else {
        for (const auto& name : names) {
            const Wave* wave = get(name);
            if (wave == nullptr) {
                error = QStringLiteral("波形不存在：%1").arg(QString::fromStdString(name));
                return false;
            }
            waves.push_back(*wave);
        }
    }
    if (waves.empty()) {
        error = QStringLiteral("波形库为空，没有可导出的波形");
        return false;
    }
    const QString text = wave_io::build_json(waves, notes);
    if (!write_text_file(path, text, error)) {
        LOG_MODULE("WaveLibrary", "export_json", LOG_ERROR, error.toStdString());
        return false;
    }
    LOG_MODULE("WaveLibrary", "export_json", LOG_INFO,
        "已导出 " << waves.size() << " 个波形到 " << path.toStdString());
    return true;
}

bool WaveLibrary::export_pulse(const QString& path, const std::string& name, QString& error) const {
    error.clear();
    const Wave* wave = get(name);
    if (wave == nullptr) {
        error = QStringLiteral("波形不存在：%1").arg(QString::fromStdString(name));
        return false;
    }
    QString text;
    if (!wave_io::build_pulse(*wave, text, error)) {
        LOG_MODULE("WaveLibrary", "export_pulse", LOG_WARN, error.toStdString());
        return false;
    }
    if (!write_text_file(path, text, error)) {
        LOG_MODULE("WaveLibrary", "export_pulse", LOG_ERROR, error.toStdString());
        return false;
    }
    LOG_MODULE("WaveLibrary", "export_pulse", LOG_INFO,
        "已导出波形 " << name << " 到 " << path.toStdString());
    return true;
}

bool WaveLibrary::export_zip(const QString& path, const std::vector<std::string>& names, QString& error,
    QStringList& notes) const {
    error.clear();
    notes.clear();
    std::vector<Wave> waves;
    if (names.empty()) {
        for (const auto& [name, wave] : waves_) {
            waves.push_back(wave);
        }
    }
    else {
        for (const auto& name : names) {
            const Wave* wave = get(name);
            if (wave == nullptr) {
                error = QStringLiteral("波形不存在：%1").arg(QString::fromStdString(name));
                return false;
            }
            waves.push_back(*wave);
        }
    }
    if (waves.empty()) {
        error = QStringLiteral("波形库为空，没有可导出的波形");
        return false;
    }

    std::vector<wave_io::ZipEntry> entries;
    QStringList used_stems;
    nlohmann::json manifest;
    manifest["version"] = "1.0";
    manifest["generator"] = "DG-LAB-Client";
    manifest["waves"] = nlohmann::json::array();
    for (const Wave& wave : waves) {
        const QString label = QString::fromStdString(wave.name);
        if (wave.raw.empty()) {
            notes.append(QStringLiteral("《%1》没有原始串（raw 为空），未写入 zip").arg(label));
            continue;
        }
        const QString stem = unique_file_stem(sanitize_file_stem(label), used_stems);
        wave_io::ZipEntry entry;
        entry.name = stem + QStringLiteral(".pulse");
        entry.data = QString::fromStdString(wave.raw).toUtf8();
        entries.push_back(entry);
        manifest["waves"].push_back({ { "name", wave.name },
            { "file", entry.name.toStdString() },
            { "durationMs", wave.duration_ms() },
            { "frameCountV2", static_cast<int>(wave.frames_v2.size()) },
            { "frameCountV3", static_cast<int>(wave.frames_v3.size()) } });
    }
    if (entries.empty()) {
        error = QStringLiteral("没有可写入 zip 的波形（原始串均为空）");
        return false;
    }
    wave_io::ZipEntry index_entry;
    index_entry.name = QString::fromLatin1(INDEX_ENTRY_NAME);
    index_entry.data = QString::fromStdString(manifest.dump(4)).toUtf8();
    entries.push_back(index_entry);

    QByteArray archive;
    if (!wave_io::build_zip(entries, archive, error)) {
        LOG_MODULE("WaveLibrary", "export_zip", LOG_ERROR, error.toStdString());
        return false;
    }
    if (!write_bytes_file(path, archive, error)) {
        LOG_MODULE("WaveLibrary", "export_zip", LOG_ERROR, error.toStdString());
        return false;
    }
    LOG_MODULE("WaveLibrary", "export_zip", LOG_INFO,
        "已导出 " << entries.size() - 1 << " 个波形到 zip：" << path.toStdString());
    return true;
}

std::string WaveLibrary::official_resource_path() {
    return std::string(OFFICIAL_RESOURCE);
}

void WaveLibrary::load() {
    load_tags();
    waves_.clear();
    current_a_.clear();
    current_b_.clear();

    bool needs_persist = false;
    std::ifstream input(file_path());
    if (!input.is_open()) {
        needs_persist = true;
    }
    else {
        try {
            nlohmann::json json;
            input >> json;
            if (json.contains("waves") && json["waves"].is_object()) {
                for (auto it = json["waves"].begin(); it != json["waves"].end(); ++it) {
                    const Wave wave = Wave::from_json(it.key(), it.value());
                    if (wave.name.empty()) {
                        LOG_MODULE("WaveLibrary", "load", LOG_WARN, "波形库条目缺少名称，已跳过");
                        needs_persist = true;
                        continue;
                    }
                    // 只有 raw 没有帧的条目（如 .pulse 导入）保留：仍可再编辑与导出 .pulse，只是不可发送
                    if (wave.frames_v2.isEmpty() && wave.frames_v3.isEmpty() && wave.raw.empty()) {
                        LOG_MODULE("WaveLibrary", "load", LOG_WARN,
                            "波形库条目既没有帧也没有 raw，已跳过: " << wave.name);
                        needs_persist = true;
                        continue;
                    }
                    waves_[wave.name] = wave;
                }
            }
            if (json.contains("current") && json["current"].is_object()) {
                current_a_ = json["current"].value("A", "");
                current_b_ = json["current"].value("B", "");
            }
        }
        catch (const std::exception& e) {
            LOG_MODULE("WaveLibrary", "load", LOG_ERROR, "波形库解析失败: " << e.what());
            waves_.clear();
            needs_persist = true;
        }
    }

    // 旧版内置占位波形不再保留
    for (const char* name : REMOVED_PLACEHOLDERS) {
        if (waves_.erase(name) > 0) {
            LOG_MODULE("WaveLibrary", "load", LOG_INFO, "已移除旧版占位波形: " << name);
            needs_persist = true;
        }
    }

    // 官方内置波形以资源为准，同名用户条目被覆盖
    load_official();

    if (needs_persist) {
        persist();
    }
}

void WaveLibrary::load_official() {
    QFile file(QString::fromStdString(official_resource_path()));
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_MODULE("WaveLibrary", "load_official", LOG_ERROR,
            "官方内置波形资源不可读: " << official_resource_path());
        return;
    }
    const QByteArray data = file.readAll();
    file.close();

    nlohmann::json json;
    try {
        json = nlohmann::json::parse(data.constData(), data.constData() + data.size());
    }
    catch (const std::exception& e) {
        LOG_MODULE("WaveLibrary", "load_official", LOG_ERROR, "官方内置波形解析失败: " << e.what());
        return;
    }
    if (!json.is_array()) {
        LOG_MODULE("WaveLibrary", "load_official", LOG_ERROR, "官方内置波形资源不是数组");
        return;
    }

    int loaded = 0;
    int skipped = 0;
    for (const auto& item : json) {
        if (!item.is_object()) {
            ++skipped;
            LOG_MODULE("WaveLibrary", "load_official", LOG_ERROR, "官方内置波形条目不是对象，已跳过");
            continue;
        }
        Wave wave;
        wave.name = item.value("name", std::string());
        if (wave.name.empty()) {
            ++skipped;
            LOG_MODULE("WaveLibrary", "load_official", LOG_ERROR, "官方内置波形条目缺少名称，已跳过");
            continue;
        }
        wave.source = WaveSource::Official;
        wave.raw = item.value("raw", std::string());

        QStringList frames_v2;
        QStringList frames_v3;
        const bool v2_ok = item.contains("expectedV2")
            && read_frames_strict(item["expectedV2"], 0, frames_v2);
        const bool v3_ok = item.contains("expectedV3")
            && read_frames_strict(item["expectedV3"], 8, frames_v3);
        if (!v2_ok || !v3_ok) {
            ++skipped;
            LOG_MODULE("WaveLibrary", "load_official", LOG_ERROR,
                "官方内置波形帧不合规，已跳过: " << wave.name);
            continue;
        }
        if (frames_v2.size() != frames_v3.size()) {
            LOG_MODULE("WaveLibrary", "load_official", LOG_WARN,
                "官方内置波形 V2/V3 帧数不一致: " << wave.name << " " << frames_v2.size() << "/"
                                                  << frames_v3.size());
        }
        wave.frames_v2 = frames_v2;
        wave.frames_v3 = frames_v3;
        waves_[wave.name] = wave;
        ++loaded;
    }

    LOG_MODULE("WaveLibrary", "load_official", LOG_INFO,
        "官方内置波形加载完成: " << loaded << " 个，跳过 " << skipped << " 个");
}

void WaveLibrary::persist() const {
    nlohmann::json json;
    json["version"] = "1.1";
    json["current"] = { { "A", current_a_ }, { "B", current_b_ } };

    nlohmann::json waves = nlohmann::json::object();
    for (const auto& [name, wave] : waves_) {
        waves[name] = wave.to_json();
    }
    json["waves"] = waves;

    std::ofstream output(file_path(), std::ios::trunc);
    if (!output.is_open()) {
        LOG_MODULE("WaveLibrary", "persist", LOG_ERROR, "无法写入波形库文件");
        return;
    }
    output << json.dump(4);
}

std::string WaveLibrary::file_path() const {
    return dir_ + "/waves.json";
}

std::string WaveLibrary::tags_file_path() const {
    return dir_ + "/" + TAGS_FILE_NAME;
}

void WaveLibrary::load_tags() {
    custom_tags_.clear();
    QFile file(QString::fromStdString(tags_file_path()));
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        LOG_MODULE("WaveLibrary", "load_tags", LOG_INFO, "标签库文件缺失，自定义标签按空处理");
        return;
    }
    const QByteArray data = file.readAll();
    file.close();

    nlohmann::json json;
    try {
        json = nlohmann::json::parse(data.constData(), data.constData() + data.size());
    }
    catch (const std::exception& e) {
        LOG_MODULE("WaveLibrary", "load_tags", LOG_ERROR, "标签库解析失败: " << e.what());
        return;
    }
    if (!json.contains("tags") || !json["tags"].is_array()) {
        LOG_MODULE("WaveLibrary", "load_tags", LOG_WARN, "标签库缺少 tags 数组，按空处理");
        return;
    }
    QStringList collected;
    for (const auto& item : json["tags"]) {
        if (item.is_string()) {
            collected.append(QString::fromStdString(item.get<std::string>()));
        }
    }
    custom_tags_ = Wave::normalize_tags(collected);
    LOG_MODULE("WaveLibrary", "load_tags", LOG_INFO,
        "自定义标签已加载，共 " << custom_tags_.size() << " 个");
}

void WaveLibrary::persist_tags() const {
    nlohmann::json json;
    json["version"] = TAGS_FILE_VERSION;
    nlohmann::json tags = nlohmann::json::array();
    for (const QString& tag : custom_tags_) {
        tags.push_back(tag.toStdString());
    }
    json["tags"] = tags;

    std::ofstream output(tags_file_path(), std::ios::trunc);
    if (!output.is_open()) {
        LOG_MODULE("WaveLibrary", "persist_tags", LOG_ERROR, "无法写入标签库文件");
        return;
    }
    output << json.dump(4);
}
