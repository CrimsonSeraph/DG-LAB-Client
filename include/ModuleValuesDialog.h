/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "ModuleValue.h"

#include <QDialog>
#include <QString>

#include <string>
#include <vector>

class QComboBox;
class QGridLayout;
class QLabel;
class Module;

// ============================================
// ModuleValuesDialog - 模块数值展示对话框
// 点击模块后弹出，显示该模块可获取的数值列表（每行两个数值框）
// ============================================
class ModuleValuesDialog : public QDialog {
    Q_OBJECT

public:
    // -------------------- 构造/析构 --------------------
    /// @brief 构造函数
    /// @param module_name 模块名称
    /// @param parent 父窗口指针
    explicit ModuleValuesDialog(const std::string& module_name, QWidget* parent = nullptr);

private slots:
    /// @brief 周期下拉框选择变化时应用新的查询周期
    /// @param index 下拉框索引
    void on_period_combo_changed(int index);

    /// @brief 周期设置变化时同步刷新所有下拉框显示
    void on_period_changed();

private:
    // -------------------- 成员变量 --------------------
    /// @brief 单个数值框的控件集合
    struct ValueBox {
        QLabel* name_label = nullptr;        ///< 数值名称标签
        QLabel* value_label = nullptr;       ///< 当前数值标签
        QLabel* field_label = nullptr;       ///< 底层字段名标签（小字）
        QComboBox* period_combo = nullptr;   ///< 查询周期下拉框
    };

    std::string module_name_;                ///< 模块名称
    std::vector<ValueBox> value_boxes_;      ///< 数值框列表
    std::vector<std::string> value_ids_;     ///< 数值 ID 列表（与数值框一一对应）
    bool syncing_combos_ = false;            ///< 正在同步下拉框（防止信号循环）

    // -------------------- 私有辅助函数 --------------------
    /// @brief 构建界面（根据模块数值每行两个生成数值框）
    /// @param module 模块对象
    void setup_ui(const Module& module);
    /// @brief 创建单个数值框并放入网格布局
    /// @param value 数值对象
    /// @param layout 目标网格布局
    /// @param row 行号
    /// @param col 列号
    void create_value_box(const ModuleValue& value, QGridLayout* layout, int row, int col);
    /// @brief 刷新所有数值框的当前值与周期显示
    void refresh_all_boxes();
};