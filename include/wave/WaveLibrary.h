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

/// @brief 导入冲突处理策略：覆盖 / 重命名 / 跳过
enum class WaveConflictPolicy {
    Overwrite, ///< 覆盖同名的用户波形（内置官方波形仍拒绝覆盖）
    Rename,    ///< 生成不重名的新名称后写入
    Skip       ///< 跳过该条
};

/// @brief 导入解析结果：待写入条目、逐条跳过原因与名称冲突列表
struct WaveImportPlan {
    std::vector<Wave> waves; ///< 解析成功的波形（来源一律按用户波形）
    QStringList skipped;     ///< 逐条跳过原因
    QStringList conflicts;   ///< 与库中重名的波形名
    QString error;           ///< 致命错误（文件不可读 / 扩展名不支持 / 文本解析失败）
};

/// @brief 导入写入结果
struct WaveImportResult {
    int added = 0;        ///< 新增条数
    int overwritten = 0;  ///< 覆盖条数
    int renamed = 0;      ///< 重命名条数
    int skipped = 0;      ///< 跳过条数
    QStringList notes;    ///< 逐条说明（跳过原因 / 重命名结果 / 缺帧标记）
    QStringList warnings; ///< 提示里必须出现的原因（跳过与缺帧不可发送）

    /// @brief 汇总文案：导入 N 条：新增 / 覆盖 / 重命名 / 跳过，附跳过与缺帧原因
    QString summary() const;
};

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

    /// @brief 解析导入文件（按扩展名分派 .json / .pulse），只解析不写入
    WaveImportPlan plan_import(const QString& path) const;
    /// @brief 按冲突策略把解析结果写入波形库
    /// @param plan plan_import 的结果
    /// @param policy 冲突处理策略
    /// @param rename_to 重命名策略下的指定名称（仅单条导入时生效，空串则按序号自动生成）
    WaveImportResult apply_import(const WaveImportPlan& plan, WaveConflictPolicy policy,
        const QString& rename_to = QString());
    /// @brief 生成与库中不重名的候选名称（名称 (2)、名称 (3)…）
    std::string suggest_name(const std::string& name) const;
    /// @brief 导出指定波形为 JSON 文件；names 为空表示导出全部波形
    bool export_json(const QString& path, const std::vector<std::string>& names, QString& error,
        QStringList& notes) const;
    /// @brief 导出单个波形为 .pulse 文件（内容为 raw 原文）
    bool export_pulse(const QString& path, const std::string& name, QString& error) const;

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
