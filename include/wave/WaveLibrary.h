/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "Wave.h"

#include <QObject>
#include <QString>
#include <QStringList>

#include <map>
#include <string>
#include <vector>

// ============================================
// WaveLibrary - 波形库（单例）
// ============================================
// 负责波形库的加载/保存（默认 config/waves/waves.json）与 A/B 通道当前波形记录；
// 首次运行且库为空时写入内置波形。
class WaveLibrary : public QObject {
    Q_OBJECT

public:
    static WaveLibrary& instance();

    /// @brief 初始化：读取波形库（缺失时写入内置波形）
    void initialize();

    std::vector<std::string> names() const;
    const Wave* get(const std::string& name) const;
    bool has(const std::string& name) const;

    bool save(const Wave& wave);
    bool remove(const std::string& name);

    /// @brief 通道（"A"/"B"）当前波形名称（未选择返回空）
    QString current(const std::string& channel) const;
    void set_current(const std::string& channel, const std::string& name);
    QStringList frames_for(const std::string& channel) const;
    int frame_count(const std::string& name) const;
    int duration_ms(const std::string& name) const;

signals:
    void library_changed();
    void current_changed();

private:
    WaveLibrary() = default;

    void load();
    void persist() const;
    void ensure_builtin_presets();
    std::string file_path() const;

    std::map<std::string, Wave> waves_;
    std::string dir_;
    std::string current_a_;
    std::string current_b_;
    bool initialized_ = false;
};
