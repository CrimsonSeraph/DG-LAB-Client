/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QColor>
#include <QDialog>
#include <QGridLayout>
#include <QMap>
#include <QString>

 // ============================================
 // 主题枚举
 // ============================================
enum Theme {
    LIGHT = 0,  ///< 浅色模式
    NIGHT = 1,  ///< 深色模式
    CHARCOAL_PINK = 2,  ///< 炭黑甜粉
    DEEPSEA_CREAM = 3,  ///< 深海奶白
    VINE_PURPLE_TEA_GREEN = 4,  ///< 藤紫钛绿
    OFFWHITE_CAMELLIA = 5,  ///< 无白茶花
    DARK_BLUE_CLEAR_BLUE = 6,   ///< 捣蓝清水
    KLEIN_YELLOW = 7,   ///< 克莱因黄
    MARS_GREEN_ROSE = 8,    ///< 马尔斯玫瑰
    HERMES_ORANGE_NAVY = 9, ///< 爱马仕深蓝
    TIFFANY_BLUE_CHEESE = 10,   ///< 蒂芙尼奶酪
    CHINA_RED_YELLOW = 11,  ///< 中国红黄
    VANDYKE_BROWN_KHAKI = 12,   ///< 凡戴克棕卡其
    PRUSSIAN_BLUE_FOG = 13  ///< 普鲁士雾灰
};

 // ============================================
 // ThemeSelectorDialog - 主题选择对话框
 // ============================================
class ThemeSelectorDialog : public QDialog {
    Q_OBJECT

public:
    // -------------------- 构造/析构 --------------------
    explicit ThemeSelectorDialog(QWidget* parent = nullptr);

signals:
    /// @brief 当用户选择某个主题时发出
    void theme_selected(Theme theme);

private:
    // -------------------- 成员变量 --------------------
    QMap<Theme, QString> chinese_names_;    ///< 主题中文名映射
    QMap<Theme, QString> mode_names_;   ///< 主题英文模式名映射
    QMap<Theme, QColor> primary_colors_;    ///< 主题主色映射
    QMap<Theme, QColor> secondary_colors_;  ///< 主题副色映射

    // -------------------- 私有辅助函数 --------------------
    void setup_ui();    ///< 初始化界面
    void create_theme_card(Theme theme, ///< 创建主题卡片并添加到布局
        const QString& chinese_name,
        const QString& mode_name,
        const QColor& primary_color,
        const QColor& secondary_color,
        QGridLayout* layout,
        int row, int col);
};
