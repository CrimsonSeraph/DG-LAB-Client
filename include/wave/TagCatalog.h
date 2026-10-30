/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QString>
#include <QStringList>

#include <vector>

/// @brief 内置分类定义：分类名与该分类下的内置标签（列表顺序即展示顺序）
struct TagCategoryDef {
    QString name;     ///< 分类名（内置分类名不可改）
    QStringList tags; ///< 该分类下的内置标签
};

// TagCatalog - 内置标签数据的唯一编辑入口
// 内置分类（固定七类，名称与顺序不可调）、每个内置分类下的内置标签、官方波形按内置表
// 自动挂载的标签，全部定义在 src/wave/TagCatalog.cpp 的两张代码表里；分类顺序决定
// 波形上标签的显示顺序。维护内置标签数据只改这一处。
class TagCatalog {
public:
    /// @brief 内置分类定义（固定七类，顺序即波形上标签的显示顺序）
    static const std::vector<TagCategoryDef>& builtin_categories();
    /// @brief 内置分类名（按固定顺序）
    static QStringList builtin_category_names();
    /// @brief 内置分类数量（固定七类）
    static int builtin_category_count();
    /// @brief 是否内置分类名（大小写不敏感）
    static bool is_builtin_category(const QString& name);
    /// @brief 用户自定义分类的名字（内置分类之一，名字不可改）
    static QString user_category_name();
    /// @brief 内置标签全集（按分类顺序展开，大小写不敏感去重）
    static QStringList builtin_tags();
    /// @brief 是否内置标签（来自代码表；与所在分类是否为内置分类无关）
    static bool is_builtin_tag(const QString& tag);
    /// @brief 内置标签所属的分类名；不是内置标签返回空串
    static QString category_of_builtin_tag(const QString& tag);
    /// @brief 官方波形按内置表自动挂载的标签（表中没有该波形时返回空列表）
    static QStringList builtin_tags_of_official_wave(const QString& wave_name);
};
