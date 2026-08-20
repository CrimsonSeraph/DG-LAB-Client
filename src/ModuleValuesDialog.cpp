/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "ModuleValuesDialog.h"

#include "DebugLog.h"
#include "Module.h"
#include "ModuleManager.h"

#include <QComboBox>
#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QVBoxLayout>

// ============================================
// 构造/析构（public）
// ============================================

ModuleValuesDialog::ModuleValuesDialog(const std::string& module_name, QWidget* parent)
    : QDialog(parent)
    , module_name_(module_name) {
    LOG_MODULE("ModuleValuesDialog", "ModuleValuesDialog", LOG_DEBUG,
        "开始构建模块数值对话框: " << module_name);

    auto& manager = ModuleManager::instance();
    const Module* module = manager.get_module(module_name_);
    if (!module) {
        LOG_MODULE("ModuleValuesDialog", "ModuleValuesDialog", LOG_ERROR,
            "模块不存在: " << module_name_);
        return;
    }

    setWindowTitle(QString::fromStdString(module->get_name()) + " - 可获取数值");
    setup_ui(*module);

    // 立即查询一次所有数值，用于首次展示当前值
    for (size_t i = 0; i < value_ids_.size(); ++i) {
        int value = manager.query_value(module_name_, value_ids_[i]);
        value_boxes_[i].value_label->setText(QString::number(value));
    }

    // 监听数值变化与周期变化，实时刷新界面
    connect(&manager, &ModuleManager::value_changed,
        this, &ModuleValuesDialog::on_value_changed);
    connect(&manager, &ModuleManager::period_changed,
        this, &ModuleValuesDialog::on_period_changed);

    LOG_MODULE("ModuleValuesDialog", "ModuleValuesDialog", LOG_DEBUG,
        "模块数值对话框构建完成");
}

// ============================================
// private slots 实现
// ============================================

void ModuleValuesDialog::on_value_changed(const QString& module_name, const QString& value_id,
    int new_value) {
    if (module_name != QString::fromStdString(module_name_)) {
        return;
    }
    // 查找对应的数值框并更新数值标签
    for (size_t i = 0; i < value_ids_.size(); ++i) {
        if (value_ids_[i] == value_id.toStdString()) {
            value_boxes_[i].value_label->setText(QString::number(new_value));
            return;
        }
    }
}

void ModuleValuesDialog::on_period_combo_changed(int box_index) {
    if (syncing_combos_ || box_index < 0
        || static_cast<size_t>(box_index) >= value_ids_.size()) {
        return;
    }
    // 由下拉框当前显示文本解析查询周期
    QueryPeriod period = query_period_from_text(
        value_boxes_[box_index].period_combo->currentText().toStdString());
    ModuleManager::instance().set_value_period(module_name_, value_ids_[box_index], period);
    LOG_MODULE("ModuleValuesDialog", "on_period_combo_changed", LOG_DEBUG,
        "数值 " << value_ids_[box_index] << " 周期设置为: " << query_period_to_text(period));
}

void ModuleValuesDialog::on_period_changed() {
    // 外部周期变化（如统一设置）后同步下拉框显示
    syncing_combos_ = true;
    auto& manager = ModuleManager::instance();
    for (size_t i = 0; i < value_ids_.size(); ++i) {
        const ModuleValue* value = manager.get_value(module_name_, value_ids_[i]);
        if (value) {
            QString text = QString::fromUtf8(query_period_to_text(value->get_query_period()));
            int idx = value_boxes_[i].period_combo->findText(text);
            if (idx >= 0) {
                value_boxes_[i].period_combo->setCurrentIndex(idx);
            }
        }
    }
    syncing_combos_ = false;
}

// ============================================
// 私有辅助函数实现（private）
// ============================================

void ModuleValuesDialog::setup_ui(const Module& module) {
    QVBoxLayout* main_layout = new QVBoxLayout(this);
    main_layout->setSpacing(10);
    main_layout->setContentsMargins(15, 15, 15, 15);

    // 提示标签
    QLabel* tip_label = new QLabel("点击下方下拉框可单独设置每个数值的查询周期", this);
    tip_label->setProperty("type", "module_tip");
    tip_label->setWordWrap(true);
    main_layout->addWidget(tip_label);

    // 滚动区域承载数值网格，自适应内容高度
    QScrollArea* scroll_area = new QScrollArea(this);
    scroll_area->setWidgetResizable(true);
    scroll_area->setFrameShape(QFrame::NoFrame);
    QWidget* container = new QWidget(scroll_area);
    QGridLayout* grid_layout = new QGridLayout(container);
    grid_layout->setSpacing(12);
    grid_layout->setContentsMargins(6, 6, 6, 6);

    const auto& values = module.get_values();
    value_ids_.reserve(values.size());
    value_boxes_.reserve(values.size());
    int row = 0;
    int col = 0;
    for (const auto& value : values) {
        create_value_box(value, grid_layout, row, col);
        value_ids_.push_back(value.get_id());
        ++col;
        if (col >= 2) {
            col = 0;
            ++row;
        }
    }

    container->setLayout(grid_layout);
    scroll_area->setWidget(container);
    main_layout->addWidget(scroll_area, 1);

    // 关闭按钮
    QPushButton* close_btn = new QPushButton("关闭", this);
    connect(close_btn, &QPushButton::clicked, this, &QDialog::accept);
    main_layout->addWidget(close_btn);

    resize(560, 60 + static_cast<int>((values.size() + 1) / 2) * 110);
}

void ModuleValuesDialog::create_value_box(const ModuleValue& value, QGridLayout* layout,
    int row, int col) {
    // 数值框容器（圆角卡片）
    QWidget* box = new QWidget(this);
    box->setProperty("type", "module_value_box");
    box->setMinimumWidth(230);
    QVBoxLayout* box_layout = new QVBoxLayout(box);
    box_layout->setSpacing(4);
    box_layout->setContentsMargins(10, 8, 10, 8);

    // 第一行：名称（左）+ 当前值（右）
    QHBoxLayout* name_row = new QHBoxLayout();
    name_row->setSpacing(8);
    QLabel* name_label = new QLabel(QString::fromStdString(value.get_name()), box);
    name_label->setProperty("type", "module_value_name");
    QLabel* value_label = new QLabel(
        value.get_has_value() ? QString::number(value.get_last_value()) : QString("--"), box);
    value_label->setProperty("type", "module_value_current");
    value_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    name_row->addWidget(name_label, 1);
    name_row->addWidget(value_label);
    box_layout->addLayout(name_row);

    // 第二行：底层字段名小字（如 m_iHealth）
    QLabel* field_label = new QLabel(QString::fromStdString(value.get_field()), box);
    field_label->setProperty("type", "module_value_field");
    box_layout->addWidget(field_label);

    // 第三行：查询周期下拉框
    QHBoxLayout* period_row = new QHBoxLayout();
    period_row->setSpacing(6);
    QLabel* period_label = new QLabel("周期:", box);
    period_label->setProperty("type", "module_value_field");
    QComboBox* period_combo = new QComboBox(box);
    period_combo->addItem(QString::fromUtf8(query_period_to_text(QueryPeriod::SECOND)));
    period_combo->addItem(QString::fromUtf8(query_period_to_text(QueryPeriod::TWO_SECONDS)));
    period_combo->addItem(QString::fromUtf8(query_period_to_text(QueryPeriod::FOUR_SECONDS)));
    period_combo->addItem(QString::fromUtf8(query_period_to_text(QueryPeriod::HALF_SECOND)));
    period_combo->addItem(QString::fromUtf8(query_period_to_text(QueryPeriod::QUARTER_SECOND)));
    QString current_period = QString::fromUtf8(query_period_to_text(value.get_query_period()));
    int current_index = period_combo->findText(current_period);
    if (current_index >= 0) {
        period_combo->setCurrentIndex(current_index);
    }
    period_combo->setProperty("type", "module_period_combo");
    period_row->addWidget(period_label);
    period_row->addWidget(period_combo, 1);
    box_layout->addLayout(period_row);

    ValueBox vb;
    vb.name_label = name_label;
    vb.value_label = value_label;
    vb.field_label = field_label;
    vb.period_combo = period_combo;
    value_boxes_.push_back(vb);

    // 下拉框变化时应用新的查询周期（捕获数值框索引，避免多个下拉框共用参数歧义）
    const int box_index = static_cast<int>(value_boxes_.size()) - 1;
    connect(period_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, [this, box_index](int) {
            on_period_combo_changed(box_index);
        });

    layout->addWidget(box, row, col);
}

void ModuleValuesDialog::refresh_all_boxes() {
    on_period_changed();
    auto& manager = ModuleManager::instance();
    for (size_t i = 0; i < value_ids_.size(); ++i) {
        const ModuleValue* value = manager.get_value(module_name_, value_ids_[i]);
        if (value) {
            value_boxes_[i].value_label->setText(
                value->get_has_value() ? QString::number(value->get_last_value()) : QString("--"));
        }
    }
}