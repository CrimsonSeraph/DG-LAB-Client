/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "LogExporter.h"

#include <QDialog>

// 前置声明
class QCheckBox;
class QComboBox;
class QGroupBox;
class QLineEdit;
class QPushButton;
class QSpinBox;

// ============================================
// LogExportSettingsDialog - 日志导出设置对话框
// 自动日志设置（级别过滤、位置、数量、大小上限）与手动日志设置（级别过滤、位置）
// ============================================
class LogExportSettingsDialog : public QDialog {
    Q_OBJECT

public:
    // -------------------- 构造/析构 --------------------
    /// @brief 构造函数
    /// @param auto_settings 当前自动日志设置
    /// @param manual_settings 当前手动日志设置
    /// @param parent 父窗口指针
    explicit LogExportSettingsDialog(const LogExporter::AutoSettings& auto_settings,
        const LogExporter::ManualSettings& manual_settings, QWidget* parent = nullptr);

    // -------------------- 公共接口（获取编辑结果）--------------------
    /// @brief 获取编辑后的自动日志设置
    /// @return 自动日志设置
    LogExporter::AutoSettings get_auto_settings() const;

    /// @brief 获取编辑后的手动日志设置
    /// @return 手动日志设置
    LogExporter::ManualSettings get_manual_settings() const;

private:
    // -------------------- 成员变量 --------------------
    // 自动日志控件
    QComboBox* auto_level_combo_ = nullptr;      ///< 自动日志级别下拉框
    QCheckBox* auto_only_check_ = nullptr;       ///< 自动日志仅指定级别
    QComboBox* auto_above_combo_ = nullptr;      ///< 自动日志范围
    QLineEdit* auto_dir_edit_ = nullptr;         ///< 自动日志位置
    QPushButton* auto_browse_btn_ = nullptr;     ///< 自动日志位置浏览
    QSpinBox* auto_retain_spin_ = nullptr;       ///< 自动日志保留数量
    QSpinBox* auto_max_size_spin_ = nullptr;     ///< 自动日志大小上限（MB）
    // 手动日志控件
    QComboBox* manual_level_combo_ = nullptr;    ///< 手动日志级别下拉框
    QCheckBox* manual_only_check_ = nullptr;     ///< 手动日志仅指定级别
    QComboBox* manual_above_combo_ = nullptr;    ///< 手动日志范围
    QLineEdit* manual_dir_edit_ = nullptr;       ///< 手动日志位置
    QPushButton* manual_browse_btn_ = nullptr;   ///< 手动日志位置浏览

    // -------------------- 私有辅助函数 --------------------
    /// @brief 构建界面（自动/手动两个分组）
    /// @param auto_settings 当前自动日志设置
    /// @param manual_settings 当前手动日志设置
    void setup_ui(const LogExporter::AutoSettings& auto_settings,
        const LogExporter::ManualSettings& manual_settings);
    /// @brief 构建单个日志设置分组（级别/仅指定/范围/位置 公共部分）
    /// @param title 分组标题
    /// @param level_combo 级别下拉框输出
    /// @param only_check 仅指定级别勾选框输出
    /// @param above_combo 范围下拉框输出
    /// @param dir_edit 位置输入框输出
    /// @param browse_btn 浏览按钮输出
    /// @param initial 初始设置
    void build_filter_group(QGroupBox* group, QComboBox*& level_combo, QCheckBox*& only_check,
        QComboBox*& above_combo, QLineEdit*& dir_edit, QPushButton*& browse_btn,
        int level, bool only, bool above, const std::string& dir);
};
