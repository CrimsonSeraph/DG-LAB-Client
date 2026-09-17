/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "WaveLibrary.h"

#include "AppConfig.h"
#include "DebugLog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <fstream>

WaveLibrary& WaveLibrary::instance() {
    static WaveLibrary library;
    return library;
}

std::string WaveLibrary::file_path() const {
    return dir_ + "/waves.json";
}

void WaveLibrary::initialize() {
    if (initialized_) {
        return;
    }
    dir_ = AppConfig::instance().get_value<std::string>("wave.path", "./config/waves");
    try {
        QDir().mkpath(QString::fromStdString(dir_));
    }
    catch (const std::exception& e) {
        LOG_MODULE("WaveLibrary", "initialize", LOG_ERROR, "创建波形目录失败: " << e.what());
    }
    load();
    initialized_ = true;
    LOG_MODULE("WaveLibrary", "initialize", LOG_INFO,
        "波形库已加载，共 " << waves_.size() << " 个波形");
}

void WaveLibrary::load() {
    waves_.clear();
    current_a_.clear();
    current_b_.clear();

    std::ifstream input(file_path());
    if (!input.is_open()) {
        ensure_builtin_presets();
        persist();
        return;
    }

    try {
        nlohmann::json json;
        input >> json;
        if (json.contains("waves") && json["waves"].is_object()) {
            for (auto it = json["waves"].begin(); it != json["waves"].end(); ++it) {
                waves_[it.key()] = Wave::from_json(it.key(), it.value());
            }
        }
        if (json.contains("current") && json["current"].is_object()) {
            current_a_ = json["current"].value("A", "");
            current_b_ = json["current"].value("B", "");
        }
    }
    catch (const std::exception& e) {
        LOG_MODULE("WaveLibrary", "load", LOG_ERROR, "波形库解析失败: " << e.what());
        ensure_builtin_presets();
    }

    if (waves_.empty()) {
        ensure_builtin_presets();
        persist();
    }
}

void WaveLibrary::persist() const {
    nlohmann::json json;
    json["version"] = "1.0";
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

void WaveLibrary::ensure_builtin_presets() {
    auto add = [this](const std::string& name, const std::vector<WaveSection>& sections) {
        Wave wave;
        wave.name = name;
        wave.sections = sections;
        wave.frames = Wave::generate_frames(sections);
        waves_[name] = wave;
    };

    // 雨水冲刷：轻雨渐强 -> 密集重雨 -> 静音间隙（参照 out/波形说明.md 案例）
    add("雨水冲刷", { { 3900, 14, 14, 33, 100 }, { 3600, 58, 58, 100, 100 }, { 300, 10, 10, 0, 0 } });
    // 渐强脉冲：低频渐强后回落
    add("渐强脉冲", { { 2000, 20, 20, 0, 60 }, { 1500, 40, 40, 60, 100 }, { 500, 10, 10, 0, 0 } });
    // 低频涌动：频率与强度缓慢起伏
    add("低频涌动", { { 4000, 12, 24, 20, 80 }, { 1000, 12, 12, 0, 0 } });
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

bool WaveLibrary::save(const Wave& wave) {
    if (wave.name.empty()) {
        return false;
    }
    waves_[wave.name] = wave;
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
    return wave == nullptr ? QStringList() : wave->frames;
}

int WaveLibrary::frame_count(const std::string& name) const {
    const Wave* wave = get(name);
    return wave == nullptr ? 0 : static_cast<int>(wave->frames.size());
}

int WaveLibrary::duration_ms(const std::string& name) const {
    const Wave* wave = get(name);
    return wave == nullptr ? 0 : wave->duration_ms();
}
