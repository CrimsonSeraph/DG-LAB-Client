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
class QLineEdit;
class QPushButton;
class QSpinBox;

// ============================================
// LogExportSettingsDialog - 日志导出设置对话框
// 配置导出日志级别、过滤方式、导出位置、保留数量与大小上限
// ============================================
class LogExportSettingsDialog : public QDialog {
    Q_OBJECT

public:
    // -------------------- 构造/析构 --------------------
    /// @brief 构造函数
    /// @param settings 当前导出设置
    /// @param parent 父窗口指针
    explicit LogExportSettingsDialog(const LogExporter::Settings& settings,
        QWidget* parent = nullptr);

    // -------------------- 公共接口（获取编辑结果）--------------------
    /// @brief 获取编辑后的导出设置
    /// @return 导出设置
    LogExporter::Settings get_settings() const;

private:
    // -------------------- 成员变量 --------------------
    QComboBox* level_combo_ = nullptr;      ///< 导出日志级别下拉框
    QCheckBox* only_level_check_ = nullptr; ///< 仅导出指定级别勾选框
    QComboBox* above_combo_ = nullptr;      ///< 指定级别以上/以下下拉框
    QLineEdit* dir_edit_ = nullptr;         ///< 导出位置输入框
    QPushButton* browse_btn_ = nullptr;     ///< 导出位置浏览按钮
    QSpinBox* retain_spin_ = nullptr;       ///< 保留日志数量
    QSpinBox* max_size_spin_ = nullptr;     ///< 单个日志大小上限（MB）

    // -------------------- 私有辅助函数 --------------------
    /// @brief 构建界面
    /// @param settings 当前导出设置
    void setup_ui(const LogExporter::Settings& settings);
};
