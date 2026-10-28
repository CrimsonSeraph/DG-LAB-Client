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

    // 标签库分两层：内置标签是代码常量（随版本走，用户不可删除），
    // 自定义标签存在 config/waves/tags.json；波形按名字引用标签，读到时统一规范化。
    /// @brief 内置标签（代码常量，随版本走，用户不可删除；本批为空，结构已就位）
    QStringList builtin_tags() const;
    /// @brief 自定义标签（config/waves/tags.json 里的顺序表）
    QStringList custom_tags() const;
    /// @brief 全部可选标签：内置在前、自定义在后，再补上被波形引用但未入库的标签
    QStringList all_tags() const;
    /// @brief 是否内置标签（不可删除）
    bool is_builtin_tag(const QString& tag) const;
    /// @brief 标签库里的统一写法；库中没有时返回规范化后的输入，非法（空 / 超长）返回空串
    QString canonical_tag(const QString& tag) const;
    /// @brief 使用该标签的波形数量（大小写不敏感）
    int tag_usage(const QString& tag) const;

    /// @brief 新增自定义标签（大小写不敏感去重，重复写入沿用库里的写法）；成功或已存在均返回 true
    /// @param error 失败原因（超长在此拒绝）
    bool add_custom_tag(const QString& tag, QString* error = nullptr);
    /// @brief 删除自定义标签，并从使用中的波形上一并移除该标签（内置标签拒绝）
    /// @return 从波形上移除该标签的波形数；拒绝或不存在返回 -1，原因写 error
    int remove_custom_tag(const QString& tag, QString* error = nullptr);
    /// @brief 给指定波形加标签（同时登记进标签库）；官方内置波形只读，拒绝
    /// @param wave_name 波形名
    /// @param tag 标签名（大小写不敏感匹配库里已有写法）
    /// @param error 失败原因
    bool attach_tag(const std::string& wave_name, const QString& tag, QString* error = nullptr);
    /// @brief 从指定波形上移除该标签（不动标签库）
    bool detach_tag(const std::string& wave_name, const QString& tag);
    /// @brief 批量自动登记标签（导入用）：只写非内置且未入库的标签，返回新增条数
    int register_tags(const QStringList& tags);
    /// @brief 注入内置标签（仅测试用：验证内置标签不可删除）
    void set_builtin_tags_for_test(const QStringList& tags);

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
    /// @brief 导出多个波形为 zip：每个波形一个 <波形名>.pulse，另附 index.json 名称清单
    /// @param path 归档文件路径
    /// @param names 待导出的波形名；为空表示导出库中全部波形
    /// @param error 失败原因
    /// @param notes 逐条说明（raw 为空而未写入 zip 的波形）
    bool export_zip(const QString& path, const std::vector<std::string>& names, QString& error,
        QStringList& notes) const;

signals:
    void library_changed();
    void current_changed();
    void tags_changed();

private:
    WaveLibrary() = default;

    /// @brief 官方内置波形资源路径（qrc）
    static std::string official_resource_path();

    void load();
    void load_official();
    void persist() const;
    std::string file_path() const;
    /// @brief 自定义标签库文件路径（config/waves/tags.json）
    std::string tags_file_path() const;
    /// @brief 读取自定义标签库（文件缺失或损坏时为空，不报错）
    void load_tags();
    /// @brief 写入自定义标签库
    void persist_tags() const;

    std::map<std::string, Wave> waves_;
    QStringList custom_tags_; ///< 自定义标签（顺序即标签库展示顺序）
    std::string dir_;
    std::string current_a_;
    std::string current_b_;
    bool initialized_ = false;
};
