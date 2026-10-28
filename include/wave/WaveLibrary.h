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

// WaveLibrary - 波形库（单例）
// 负责波形库的加载/保存（默认 config/waves/waves.json）、官方内置波形资源
// （assets/waves/official_waves.json，只读）与 A/B 通道当前波形记录；
// 每条波形同时持有 V2 与 V3 帧，发送协议由配置 app.wave.protocol 决定。
class WaveLibrary : public QObject {
    Q_OBJECT

public:
    static WaveLibrary& instance();

    /// @brief 初始化：读取波形库与官方内置波形资源（用户库文件缺失时写入一次）
    void initialize();

    /// @brief 重新读取波形库与官方内置波形资源
    void reload();

    std::vector<std::string> names() const;
    const Wave* get(const std::string& name) const;
    bool has(const std::string& name) const;
    /// @brief 是否官方内置波形（只读：不可删除、不可改名、不可覆盖）
    bool is_official(const std::string& name) const;
    /// @brief 已加载的官方内置波形数量
    int official_count() const;

    bool save(const Wave& wave);
    bool remove(const std::string& name);

    /// @brief 当前发送协议（"v2" / "v3"），读配置 app.wave.protocol，缺省 v3
    QString protocol() const;
    /// @brief 写入发送协议（只接受 v2 / v3，其他取值忽略）
    void set_protocol(const QString& protocol);

    /// @brief 通道（"A"/"B"）当前波形名称（未选择返回空）
    QString current(const std::string& channel) const;
    void set_current(const std::string& channel, const std::string& name);
    /// @brief 通道当前波形的 V3 帧（界面预览用）
    QStringList frames_for(const std::string& channel) const;
    /// @brief 指定波形在指定协议下的帧；缺帧返回空，不回落另一版本
    QStringList frames_of(const std::string& name, const QString& protocol) const;
    int frame_count(const std::string& name) const;
    int duration_ms(const std::string& name) const;

signals:
    void library_changed();
    void current_changed();

private:
    WaveLibrary() = default;

    /// @brief 官方内置波形资源路径（qrc）
    static std::string official_resource_path();

    void load();
    void load_official();
    void persist() const;
    std::string file_path() const;

    std::map<std::string, Wave> waves_;
    std::string dir_;
    std::string current_a_;
    std::string current_b_;
    bool initialized_ = false;
};
