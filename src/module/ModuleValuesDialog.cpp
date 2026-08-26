/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "ModuleValuesDialog.h"

#include "DebugLog.h"
#include "Module.h"
#include "ModuleManager.h"
#include "StyledComboBox.h"

#include <QComboBox>
#include <QFont>
#include <QFontMetrics>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QVBoxLayout>

// ============================================
// 文件级辅助（数值范围显示与省略号文本）
// ============================================
namespace {
    // 当前值标签省略号宽度（容器左半部分）
    constexpr int VALUE_LABEL_ELIDE_WIDTH = 90;
    // 最值标签省略号宽度（容器右半部分）
    constexpr int RANGE_LABEL_ELIDE_WIDTH = 150;

    /// @brief 格式化最值显示：无范围显示 NULL；单侧缺失对应侧显示 NULL（如 "NULL/255"）
    /// @param min_value 最小值（空表示无下限）
    /// @param max_value 最大值（空表示无上限）
    /// @return 显示文本（如 "0/100"、"NULL/255"、"NULL"）
    QString format_range(std::optional<int> min_value, std::optional<int> max_value) {
        if (!min_value && !max_value) {
            return QStringLiteral("NULL");
        }
        const QString min_str = min_value ? QString::number(*min_value) : QStringLiteral("NULL");
        const QString max_str = max_value ? QString::number(*max_value) : QStringLiteral("NULL");
        return min_str + "/" + max_str;
    }

    /// @brief 设置带省略号的标签文本（超出最大宽度时右侧省略），完整文本放入悬浮提示
    /// @param label 目标标签
    /// @param text 完整文本
    /// @param max_width 省略宽度（像素）
    void set_elided_label_text(QLabel* label, const QString& text, int max_width) {
        const QFontMetrics metrics(label->font());
        label->setText(metrics.elidedText(text, Qt::ElideRight, max_width));
        label->setToolTip(text);
    }
} // namespace

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

    // 立即查询一次所有数值，用于首次展示当前值（带省略号处理）
    for (size_t i = 0; i < value_ids_.size(); ++i) {
        int value = manager.query_value(module_name_, value_ids_[i]);
        set_elided_label_text(value_boxes_[i].value_label, QString::number(value),
            VALUE_LABEL_ELIDE_WIDTH);
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
    // 查找对应的数值框并更新数值标签（带省略号处理）
    for (size_t i = 0; i < value_ids_.size(); ++i) {
        if (value_ids_[i] == value_id.toStdString()) {
            set_elided_label_text(value_boxes_[i].value_label, QString::number(new_value),
                VALUE_LABEL_ELIDE_WIDTH);
            return;
        }
    }
}

void ModuleValuesDialog::on_period_combo_changed(int box_index) {
    if (syncing_combos_ || box_index < 0 || static_cast<size_t>(box_index) >= value_ids_.size()) {
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
        create_value_box(value, grid_layout, row, col, value.get_min(), value.get_max());
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

    // 用户可调整弹窗区域大小
    setSizeGripEnabled(true);

    // 获取主屏幕可用区域
    QScreen* screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect screenRect = screen->availableGeometry();
        int maxHeight = static_cast<int>(screenRect.height() * 0.8);
        // 宽度留白
        int width = qMin(560, screenRect.width() - 40);
        // 设置窗口大小（高度取最大高度，宽度固定）
        resize(width, maxHeight);
    }
    else {
        // 保底方案（无屏幕信息时）
        resize(560, 400);
    }
}

void ModuleValuesDialog::create_value_box(const ModuleValue& value, QGridLayout* layout,
    int row, int col, std::optional<int> min_value, std::optional<int> max_value) {
    // 数值框容器（圆角卡片）
    QWidget* box = new QWidget(this);
    box->setProperty("type", "module_value_box");
    box->setMinimumWidth(230);
    QVBoxLayout* box_layout = new QVBoxLayout(box);
    box_layout->setSpacing(4);
    box_layout->setContentsMargins(10, 8, 10, 8);

    // 第一行：名称（左）+ 值范围容器（右：当前值 | 最小值/最大值）
    QHBoxLayout* name_row = new QHBoxLayout();
    name_row->setSpacing(8);
    QLabel* name_label = new QLabel(QString::fromStdString(value.get_name()), box);
    name_label->setProperty("type", "module_value_name");
    name_label->setWordWrap(true);
    name_row->addWidget(name_label, 1);

    // 容器：当前值（左）| 最值（右），"|" 左右成比例（当前值 1 : 最值 2），超出省略号
    QWidget* value_container = new QWidget(box);
    value_container->setProperty("type", "module_value_range_container");
    QHBoxLayout* container_layout = new QHBoxLayout(value_container);
    container_layout->setContentsMargins(0, 0, 0, 0);
    container_layout->setSpacing(4);
    QLabel* value_label = new QLabel(
        value.get_has_value() ? QString::number(value.get_last_value()) : QString("--"),
        value_container);
    value_label->setProperty("type", "module_value_current");
    value_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    value_label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    QLabel* separator_label = new QLabel(QStringLiteral("|"), value_container);
    separator_label->setAlignment(Qt::AlignCenter);
    QLabel* range_label = new QLabel(format_range(min_value, max_value), value_container);
    range_label->setProperty("type", "module_value_range");
    range_label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    range_label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    container_layout->addWidget(value_label, 1);
    container_layout->addWidget(separator_label, 0);
    container_layout->addWidget(range_label, 2);
    // 最值标签初始即做省略号处理（无范围时显示 NULL）
    set_elided_label_text(range_label, format_range(min_value, max_value),
        RANGE_LABEL_ELIDE_WIDTH);
    set_elided_label_text(value_label,
        value.get_has_value() ? QString::number(value.get_last_value()) : QString("--"),
        VALUE_LABEL_ELIDE_WIDTH);

    // 名称（左）与容器（右）成比例（名称 1 : 容器 2）
    name_row->addWidget(value_container, 2);
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
    QComboBox* period_combo = new StyledComboBox(box);
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
    vb.range_label = range_label;
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
            set_elided_label_text(value_boxes_[i].value_label,
                value->get_has_value() ? QString::number(value->get_last_value()) : QString("--"),
                VALUE_LABEL_ELIDE_WIDTH);
        }
    }
}
