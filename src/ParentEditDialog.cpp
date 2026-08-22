/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "ParentEditDialog.h"

#include "DGLABClient_utils.hpp"
#include "DebugLog.h"
#include "RuleManager.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

// ============================================
// 构造/析构（public）
// ============================================

ParentEditDialog::ParentEditDialog(const std::string& rule_name, QWidget* parent)
    : QDialog(parent)
    , rule_name_(rule_name) {
    LOG_MODULE("ParentEditDialog", "ParentEditDialog", LOG_DEBUG,
        "开始构建父级编辑对话框: " << rule_name);
    setup_ui();
    setWindowTitle(QString::fromStdString("编辑父级 - " + rule_name));
    resize(420, 420);
    LOG_MODULE("ParentEditDialog", "ParentEditDialog", LOG_DEBUG, "父级编辑对话框构建完成");
}

// ============================================
// 公共接口（public）
// ============================================

std::string ParentEditDialog::get_channel() const {
    QString text = channel_combo_->currentText();
    return (text == "A" || text == "B") ? text.toStdString() : "";
}

std::vector<int> ParentEditDialog::get_selected_rules() const {
    std::vector<int> result;
    for (int i = 0; i < rules_list_->count(); ++i) {
        QListWidgetItem* item = rules_list_->item(i);
        if (item->checkState() == Qt::Checked) {
            result.push_back(item->data(Qt::UserRole).toInt());
        }
    }
    return result;
}

// ============================================
// 私有辅助函数实现（private）
// ============================================

void ParentEditDialog::setup_ui() {
    auto& rule_manager = RuleManager::instance();

    QVBoxLayout* main_layout = new QVBoxLayout(this);
    main_layout->setSpacing(10);
    main_layout->setContentsMargins(15, 15, 15, 15);

    // 说明标签
    QLabel* tip = new QLabel("父级为该规则结果的接收者：通道父级（结果发送给通道）与规则父级（结果推送给引用本规则的规则）。", this);
    tip->setWordWrap(true);
    main_layout->addWidget(tip);

    // 通道父级（单选）
    QHBoxLayout* channel_row = new QHBoxLayout();
    channel_row->setSpacing(8);
    channel_row->addWidget(new QLabel("通道父级:", this));
    channel_combo_ = new QComboBox(this);
    channel_combo_->addItem("无");
    channel_combo_->addItem("A");
    channel_combo_->addItem("B");
    // 应用下拉框弹出样式处理（规避弹出列表边缘黑色）
    DGLABClientUtil::apply_combo_popup_style(channel_combo_);
    // 当前通道父级
    std::string current_channel = rule_manager.get_rule_channel(rule_name_);
    int channel_index = current_channel.empty() ? 0 : (current_channel == "A" ? 1 : 2);
    channel_combo_->setCurrentIndex(channel_index);
    channel_row->addWidget(channel_combo_);
    channel_row->addStretch();
    main_layout->addLayout(channel_row);

    // 规则父级（多选）：勾选后对应规则的值模式将引用本规则（{rule:本规则序号}）
    QLabel* rules_tip = new QLabel("规则父级（勾选后该规则的值模式将引用本规则 {rule:xx}）:", this);
    rules_tip->setWordWrap(true);
    main_layout->addWidget(rules_tip);

    rules_list_ = new QListWidget(this);
    int self_index = rule_manager.get_rule_index(rule_name_);
    for (const auto& other_name : rule_manager.get_rule_names()) {
        if (other_name == rule_name_) {
            // 排除自身，避免自引用
            continue;
        }
        int other_index = rule_manager.get_rule_index(other_name);
        // 该规则的值模式是否已引用本规则
        std::string pattern = rule_manager.get_rule_value_pattern(other_name);
        bool referenced = pattern.find("{rule:" + std::to_string(self_index) + "}") != std::string::npos;
        QListWidgetItem* item = new QListWidgetItem(
            QString("%1 [#%2]").arg(QString::fromStdString(other_name)).arg(other_index), rules_list_);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(referenced ? Qt::Checked : Qt::Unchecked);
        item->setData(Qt::UserRole, other_index);
    }
    main_layout->addWidget(rules_list_, 1);

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
