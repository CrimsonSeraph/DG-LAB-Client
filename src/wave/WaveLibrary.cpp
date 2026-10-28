/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "WaveLibrary.h"

#include "AppConfig.h"
#include "DebugLog.h"

#include <QDir>
#include <QFile>

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

} // namespace

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

std::string WaveLibrary::official_resource_path() {
    return std::string(OFFICIAL_RESOURCE);
}

void WaveLibrary::load() {
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
                    if (wave.frames_v2.isEmpty() && wave.frames_v3.isEmpty()) {
                        LOG_MODULE("WaveLibrary", "load", LOG_WARN,
                            "波形库条目没有任何合规帧，已跳过: " << wave.name);
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
