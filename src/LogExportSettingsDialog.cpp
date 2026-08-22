/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "LogExportSettingsDialog.h"

#include "DebugLog.h"

#include <algorithm>

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

// ============================================
// 构造/析构（public）
// ============================================

LogExportSettingsDialog::LogExportSettingsDialog(const LogExporter::Settings& settings,
    QWidget* parent)
    : QDialog(parent) {
    LOG_MODULE("LogExportSettingsDialog", "LogExportSettingsDialog", LOG_DEBUG,
        "开始构建日志导出设置对话框");
    setup_ui(settings);
    setWindowTitle("日志导出设置");
    resize(460, 320);
    LOG_MODULE("LogExportSettingsDialog", "LogExportSettingsDialog", LOG_DEBUG,
        "日志导出设置对话框构建完成");
}

// ============================================
// 公共接口（public）
// ============================================

LogExporter::Settings LogExportSettingsDialog::get_settings() const {
    LogExporter::Settings settings;
    settings.level = level_combo_->currentIndex();
    settings.only_level = only_level_check_->isChecked();
    settings.level_above = (above_combo_->currentIndex() == 0);
    settings.dir = dir_edit_->text().trimmed().toStdString();
    if (settings.dir.empty()) {
        settings.dir = "./log";
    }
    settings.retain_count = retain_spin_->value();
    settings.max_size = static_cast<qint64>(max_size_spin_->value()) * 1024 * 1024;
    return settings;
}

// ============================================
// 私有辅助函数实现（private）
// ============================================

void LogExportSettingsDialog::setup_ui(const LogExporter::Settings& settings) {
    QVBoxLayout* main_layout = new QVBoxLayout(this);
    main_layout->setSpacing(10);
    main_layout->setContentsMargins(15, 15, 15, 15);

    QFormLayout* form = new QFormLayout();
    form->setSpacing(10);

    // 导出日志级别
    level_combo_ = new QComboBox(this);
    level_combo_->addItem("DEBUG");
    level_combo_->addItem("INFO");
    level_combo_->addItem("WARN");
    level_combo_->addItem("ERROR");
    level_combo_->setCurrentIndex(std::clamp(settings.level, 0, 3));
    form->addRow("导出日志级别:", level_combo_);

    // 是否只导出指定级别
    only_level_check_ = new QCheckBox("仅导出指定级别", this);
    only_level_check_->setChecked(settings.only_level);
    form->addRow("过滤方式:", only_level_check_);

    // 指定级别及以上/以下
    above_combo_ = new QComboBox(this);
    above_combo_->addItem("指定级别及以上");
    above_combo_->addItem("指定级别以下");
    above_combo_->setCurrentIndex(settings.level_above ? 0 : 1);
    form->addRow("级别范围:", above_combo_);

    // 导出位置
    QHBoxLayout* dir_row = new QHBoxLayout();
    dir_row->setSpacing(6);
    dir_edit_ = new QLineEdit(QString::fromStdString(settings.dir), this);
    browse_btn_ = new QPushButton("浏览...", this);
    connect(browse_btn_, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, "选择导出目录",
            dir_edit_->text().trimmed().isEmpty() ? QDir::homePath() : dir_edit_->text().trimmed());
        if (!dir.isEmpty()) {
            dir_edit_->setText(dir);
        }
    });
    dir_row->addWidget(dir_edit_, 1);
    dir_row->addWidget(browse_btn_);
    form->addRow("导出位置:", dir_row);

    // 保留日志数量
    retain_spin_ = new QSpinBox(this);
    retain_spin_->setRange(1, 99);
    retain_spin_->setValue(std::max(1, settings.retain_count));
    form->addRow("保留日志数量:", retain_spin_);

    // 单个日志大小上限（MB）
    max_size_spin_ = new QSpinBox(this);
    max_size_spin_->setRange(1, 9999);
    max_size_spin_->setSuffix(" MB");
    max_size_spin_->setValue(std::max(1, static_cast<int>(settings.max_size / (1024 * 1024))));
    form->addRow("单个日志大小上限:", max_size_spin_);

    main_layout->addLayout(form);

    // 说明标签
    QLabel* tip = new QLabel("日志大小超出上限时将分片写入多个文件（视为一份日志）；"
                             "程序退出时自动清理多余日志，仅保留最新 N 份。", this);
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
