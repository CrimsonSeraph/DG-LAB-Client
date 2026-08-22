/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "LogExportSettingsDialog.h"

#include "DGLABClient_utils.hpp"
#include "DebugLog.h"

#include <algorithm>

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

// ============================================
// 构造/析构（public）
// ============================================

LogExportSettingsDialog::LogExportSettingsDialog(const LogExporter::AutoSettings& auto_settings,
    const LogExporter::ManualSettings& manual_settings, QWidget* parent)
    : QDialog(parent) {
    LOG_MODULE("LogExportSettingsDialog", "LogExportSettingsDialog", LOG_DEBUG,
        "开始构建日志导出设置对话框");
    setup_ui(auto_settings, manual_settings);
    setWindowTitle("日志导出设置");
    resize(520, 560);
    LOG_MODULE("LogExportSettingsDialog", "LogExportSettingsDialog", LOG_DEBUG,
        "日志导出设置对话框构建完成");
}

// ============================================
// 公共接口（public）
// ============================================

LogExporter::AutoSettings LogExportSettingsDialog::get_auto_settings() const {
    LogExporter::AutoSettings settings;
    settings.level = auto_level_combo_->currentIndex();
    settings.only_level = auto_only_check_->isChecked();
    settings.level_above = (auto_above_combo_->currentIndex() == 0);
    settings.dir = auto_dir_edit_->text().trimmed().toStdString();
    if (settings.dir.empty()) {
        settings.dir = "./log";
    }
    settings.retain_count = auto_retain_spin_->value();
    settings.max_size = static_cast<qint64>(auto_max_size_spin_->value()) * 1024 * 1024;
    return settings;
}

LogExporter::ManualSettings LogExportSettingsDialog::get_manual_settings() const {
    LogExporter::ManualSettings settings;
    settings.level = manual_level_combo_->currentIndex();
    settings.only_level = manual_only_check_->isChecked();
    settings.level_above = (manual_above_combo_->currentIndex() == 0);
    settings.dir = manual_dir_edit_->text().trimmed().toStdString();
    if (settings.dir.empty()) {
        settings.dir = "./log/handle";
    }
    return settings;
}

// ============================================
// 私有辅助函数实现（private）
// ============================================

void LogExportSettingsDialog::setup_ui(const LogExporter::AutoSettings& auto_settings,
    const LogExporter::ManualSettings& manual_settings) {
    QVBoxLayout* main_layout = new QVBoxLayout(this);
    main_layout->setSpacing(10);
    main_layout->setContentsMargins(15, 15, 15, 15);

    // 自动日志分组
    QGroupBox* auto_group = new QGroupBox("自动日志（程序启动后持续记录，受数量与大小限制）", this);
    build_filter_group(auto_group, auto_level_combo_, auto_only_check_, auto_above_combo_,
        auto_dir_edit_, auto_browse_btn_, auto_settings.level, auto_settings.only_level,
        auto_settings.level_above, auto_settings.dir);
    // 自动日志特有：保留数量与大小上限
    QFormLayout* auto_form = qobject_cast<QFormLayout*>(auto_group->layout());
    auto_retain_spin_ = new QSpinBox(auto_group);
    auto_retain_spin_->setRange(1, 99);
    auto_retain_spin_->setValue(std::max(1, auto_settings.retain_count));
    auto_form->addRow("保留日志数量:", auto_retain_spin_);
    auto_max_size_spin_ = new QSpinBox(auto_group);
    auto_max_size_spin_->setRange(1, 9999);
    auto_max_size_spin_->setSuffix(" MB");
    auto_max_size_spin_->setValue(std::max(1, static_cast<int>(auto_settings.max_size / (1024 * 1024))));
    auto_form->addRow("单个日志大小上限:", auto_max_size_spin_);
    main_layout->addWidget(auto_group);

    // 手动日志分组
    QGroupBox* manual_group = new QGroupBox("手动日志（点击导出时写入，不受数量与大小限制）", this);
    build_filter_group(manual_group, manual_level_combo_, manual_only_check_, manual_above_combo_,
        manual_dir_edit_, manual_browse_btn_, manual_settings.level, manual_settings.only_level,
        manual_settings.level_above, manual_settings.dir);
    main_layout->addWidget(manual_group);

    // 说明标签
    QLabel* tip = new QLabel("自动日志默认输出到程序目录 log/，超出大小上限时分片写入多个文件（视为一份），"
                             "并自动清理多余日志仅保留最新 N 份；手动日志默认输出到 log/handle/，不受限制。", this);
    tip->setWordWrap(true);
    main_layout->addWidget(tip);

    // 按钮
    QHBoxLayout* btn_row = new QHBoxLayout();
    QPushButton* ok_btn = new QPushButton("确定", this);
    QPushButton* cancel_btn = new QPushButton("取消", this);
    ok_btn->setDefault(true);
    connect(ok_btn, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancel_btn, &QPushButton::clicked, this, &QDialog::reject);
    btn_row->addStretch();
    btn_row->addWidget(ok_btn);
    btn_row->addWidget(cancel_btn);
    main_layout->addLayout(btn_row);
}

void LogExportSettingsDialog::build_filter_group(QGroupBox* group, QComboBox*& level_combo,
    QCheckBox*& only_check, QComboBox*& above_combo, QLineEdit*& dir_edit,
    QPushButton*& browse_btn, int level, bool only, bool above, const std::string& dir) {
    QFormLayout* form = new QFormLayout(group);
    form->setSpacing(8);

    // 导出日志级别
    level_combo = new QComboBox(group);
    level_combo->addItem("DEBUG");
    level_combo->addItem("INFO");
    level_combo->addItem("WARN");
    level_combo->addItem("ERROR");
    level_combo->setCurrentIndex(std::clamp(level, 0, 3));
    // 应用下拉框弹出样式处理（规避弹出列表边缘黑色）
    DGLABClientUtil::apply_combo_popup_style(level_combo);
    form->addRow("导出日志级别:", level_combo);

    // 是否只导出指定级别
    only_check = new QCheckBox("仅导出指定级别", group);
    only_check->setChecked(only);
    form->addRow("过滤方式:", only_check);

    // 指定级别及以上/以下
    above_combo = new QComboBox(group);
    above_combo->addItem("指定级别及以上");
    above_combo->addItem("指定级别以下");
    above_combo->setCurrentIndex(above ? 0 : 1);
    DGLABClientUtil::apply_combo_popup_style(above_combo);
    form->addRow("级别范围:", above_combo);

    // 导出位置
    QHBoxLayout* dir_row = new QHBoxLayout();
    dir_row->setSpacing(6);
    dir_edit = new QLineEdit(QString::fromStdString(dir), group);
    browse_btn = new QPushButton("浏览...", group);
    connect(browse_btn, &QPushButton::clicked, this, [this, dir_edit]() {
        QString dir = QFileDialog::getExistingDirectory(this, "选择导出目录",
            dir_edit->text().trimmed().isEmpty() ? QDir::homePath() : dir_edit->text().trimmed());
        if (!dir.isEmpty()) {
            dir_edit->setText(dir);
        }
    });
    dir_row->addWidget(dir_edit, 1);
    dir_row->addWidget(browse_btn);
    form->addRow("导出位置:", dir_row);
}
