/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QComboBox>

// ============================================
// StyledComboBox - 统一下拉框控件
// 规则表格下拉框委托、模块页、父级/日志设置等所有展开式下拉框统一使用本类，
// 内置弹出样式处理（规避弹出列表边缘黑色），保证各页面下拉菜单外观一致
// ============================================
class StyledComboBox : public QComboBox {
    Q_OBJECT

public:
    // -------------------- 构造/析构 --------------------
    /// @brief 构造函数
    /// @param parent 父窗口指针
    explicit StyledComboBox(QWidget* parent = nullptr);

    // -------------------- 重写事件 --------------------
    /// @brief 重写弹出事件：弹出后为容器设置圆角裁剪，保证弹出菜单圆角外观
    void showPopup() override;
};
