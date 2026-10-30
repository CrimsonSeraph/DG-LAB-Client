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
#include <utility>
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

/// @brief 标签分类：名称 + 是否内置分类 + 该分类下的标签（内置分类下内置标签在前、用户新增标签在后）
struct WaveTagCategory {
    QString name;     ///< 分类名（内置分类名不可改）
    bool builtin = false; ///< 是否内置分类（名称与顺序固定，排在最前）
    QStringList tags; ///< 该分类下的标签名（顺序即分类内展示顺序）
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

    // 标签库按分类组织：内置七类（名称与顺序来自 TagCatalog，固定在前）与用户自建分类
    // （排在其后，顺序可调）。标签名全库唯一，用名字引用；波形按名字持有标签。
    // 官方内置波形的标签 = 内置表自动挂载的内置标签 + 用户在 official_wave_tags 里的叠加标签：
    // 前者界面锁定不可移除，后者可增删。
    /// @brief 内置标签（代码表 TagCatalog，用户不可删除）
    QStringList builtin_tags() const;
    /// @brief 用户标签全集（各分类下的用户新增标签，按分类顺序展开）
    QStringList custom_tags() const;
    /// @brief 全部可选标签：按分类顺序展开，再补上被波形引用但未入库的标签
    QStringList all_tags() const;
    /// @brief 是否内置标签（来自代码表，与所在分类是否为内置分类无关）
    bool is_builtin_tag(const QString& tag) const;
    /// @brief 标签库里的统一写法；库中没有时返回规范化后的输入，非法（空 / 超长）返回空串
    QString canonical_tag(const QString& tag) const;
    /// @brief 使用该标签的波形数量（大小写不敏感）
    int tag_usage(const QString& tag) const;

    // 分类模型
    /// @brief 有序分类列表：内置七类固定在前，自建分类按用户调整后的顺序排在其后
    std::vector<WaveTagCategory> tag_categories() const;
    /// @brief 分类名（与 tag_categories 同序）
    QStringList category_names() const;
    /// @brief 指定分类下的标签（分类不存在返回空列表）
    QStringList tags_of_category(const QString& category) const;
    /// @brief 标签所属分类名（不在标签库中返回空串）
    QString category_of_tag(const QString& tag) const;
    /// @brief 内置分类数量（固定七类）
    int builtin_category_count() const;
    /// @brief 某波形上的标签按分类顺序排好的显示顺序（未入库的标签排在末尾）
    QStringList tags_in_display_order(const std::string& wave_name) const;
    /// @brief 该标签是否由内置表挂在该波形上（官方波形 + 内置标签表，界面锁定不可移除）
    bool is_tag_builtin_mounted(const std::string& wave_name, const QString& tag) const;
    /// @brief 创建自建分类：追加在自建分类末尾；名称与任何现有分类重名则拒绝（大小写不敏感）
    /// @param error 失败原因
    bool create_category(const QString& name, QString* error = nullptr);
    /// @brief 自建分类改名（内置分类拒绝；新名字不得与任何现有分类重名）
    /// @param name 原分类名
    /// @param new_name 新分类名
    /// @param error 失败原因
    bool rename_category(const QString& name, const QString& new_name, QString* error = nullptr);
    /// @brief 调整自建分类顺序
    /// @param name 分类名
    /// @param target_index 完整分类列表里的目标下标（不得落到内置分类之前）
    /// @param error 失败原因（内置分类、越界到内置区段、分类不存在）
    bool move_category(const QString& name, int target_index, QString* error = nullptr);
    /// @brief 删除自建分类：其下标签一并从标签库与所有波形（含官方波形叠加段）上移除
    /// @param name 分类名
    /// @param error 失败原因（内置分类拒绝）
    bool remove_category(const QString& name, QString* error = nullptr);
    /// @brief 在指定分类下新建标签（标签名全库唯一，大小写不敏感）
    /// @param category 目标分类名
    /// @param tag 标签名
    /// @param error 失败原因（分类不存在 / 名字非法 / 已属于别的分类）
    bool create_tag(const QString& category, const QString& tag, QString* error = nullptr);
    /// @brief 在「用户自定义」分类下新建标签（导入登记与勾选新建的统一入口）
    bool create_user_tag(const QString& tag, QString* error = nullptr);
    /// @brief 删除标签，并从使用中的波形上一并移除（内置标签拒绝）
    /// @return 从波形上移除该标签的波形数；拒绝或不存在返回 -1，原因写 error
    int remove_tag(const QString& tag, QString* error = nullptr);

    /// @brief 给指定波形加标签（官方波形的用户叠加标签存进 tags.json，帧与名称仍只读）
    /// @param wave_name 波形名
    /// @param tag 标签名（大小写不敏感匹配库里已有写法）
    /// @param error 失败原因
    bool attach_tag(const std::string& wave_name, const QString& tag, QString* error = nullptr);
    /// @brief 从指定波形上移除标签（不动标签库；内置表挂上的标签拒绝，官方波形改写叠加段）
    /// @param wave_name 波形名
    /// @param tag 标签名
    /// @param error 失败原因
    bool detach_tag(const std::string& wave_name, const QString& tag, QString* error = nullptr);
    /// @brief 批量自动登记标签（导入用）：只写标签库里没有的名字，登记进「用户自定义」分类
    int register_tags(const QStringList& tags);
    /// @brief 注入内置标签（仅测试用）：追加到指定内置分类，验证内置标签不可删与不落盘
    void add_builtin_tags_for_test(const QString& category, const QStringList& tags);
    /// @brief 注入官方波形的内置标签（仅测试用）：按内置表的同一条路径自动挂载
    void add_builtin_wave_tags_for_test(const std::string& wave_name, const QStringList& tags);

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
    /// @brief 按内置表给官方波形挂上内置标签（资源重建后调用，排在用户叠加标签之前）
    void apply_builtin_official_tags();
    /// @brief 应用官方波形的标签叠加数据（tags.json 的 official_wave_tags 段，资源重建后调用）
    void load_official_tags();
    void persist() const;
    std::string file_path() const;
    /// @brief 标签库文件路径（config/waves/tags.json）
    std::string tags_file_path() const;
    /// @brief 读取标签库：version 1 的扁平标签整体迁入「用户自定义」分类，文件缺失或损坏按空库处理
    void load_tags();
    /// @brief 写入标签库（version 2：只写自建分类、内置分类下的用户标签与官方波形叠加标签）
    void persist_tags() const;
    /// @brief 从代码表重建分类（内置七类在前、内置标签在内），已存在的用户标签按原顺序保留
    void rebuild_categories();
    /// @brief 按分类顺序合并（分类名 → 标签）分组：内置分类名并入其用户标签，其余建成自建分类
    void merge_category_groups(const std::vector<std::pair<QString, QStringList>>& groups);
    /// @brief 标签所在分类的下标（大小写不敏感），不在标签库中返回 -1
    int category_index_of_tag(const QString& tag) const;
    /// @brief 官方波形按内置表（含测试注入）应挂上的内置标签
    QStringList builtin_tags_of_wave(const std::string& wave_name) const;

    std::map<std::string, Wave> waves_;
    std::vector<WaveTagCategory> categories_; ///< 分类与标签（内置七类在前，自建分类在后）
    std::map<QString, QStringList> builtin_extra_tags_; ///< 仅测试：注入到内置分类的内置标签
    std::map<std::string, QStringList> builtin_extra_wave_tags_; ///< 仅测试：注入到官方波形的内置标签
    bool tags_need_migrate_ = false; ///< 标签库读成 version 1，加载完成后按 version 2 重写一次
    std::string dir_;
    std::string current_a_;
    std::string current_b_;
    bool initialized_ = false;
};
