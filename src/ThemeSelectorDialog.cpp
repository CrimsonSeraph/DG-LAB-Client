/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "ThemeSelectorDialog.h"
#include "DebugLog.h"

#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QFrame>
#include <QMouseEvent>

 // ============================================
 // ClickableCard - 可点击的卡片容器（局部辅助类）
 // ============================================
class ClickableCard : public QFrame {
    Q_OBJECT
public:
    inline explicit ClickableCard(QWidget* parent = nullptr) : QFrame(parent) {
        setFrameShape(QFrame::Box);
        setFrameShadow(QFrame::Raised);
        setLineWidth(1);
        setCursor(Qt::PointingHandCursor);
    }

signals:
    void clicked();

protected:
    inline void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            emit clicked();
        }
        QFrame::mouseReleaseEvent(event);
    }
};

// ============================================
// 构造/析构（public）
// ============================================

ThemeSelectorDialog::ThemeSelectorDialog(QWidget* parent)
    : QDialog(parent) {
    LOG_MODULE("ThemeSelectorDialog", "ThemeSelectorDialog", LOG_DEBUG,
        "开始初始化主题选择对话框");

    // 初始化主题中文名映射
    chinese_names_ = {
        {LIGHT, "浅色模式"},
        {NIGHT, "深色模式"},
        {CHARCOAL_PINK, "炭黑甜粉"},
        {DEEPSEA_CREAM, "深海奶白"},
        {VINE_PURPLE_TEA_GREEN, "藤紫钛绿"},
        {OFFWHITE_CAMELLIA, "无白茶花"},
        {DARK_BLUE_CLEAR_BLUE, "捣蓝清水"},
        {KLEIN_YELLOW, "克莱因黄"},
        {MARS_GREEN_ROSE, "马尔斯玫瑰"},
        {HERMES_ORANGE_NAVY, "爱马仕深蓝"},
        {TIFFANY_BLUE_CHEESE, "蒂芙尼奶酪"},
        {CHINA_RED_YELLOW, "中国红黄"},
        {VANDYKE_BROWN_KHAKI, "凡戴克棕卡其"},
        {PRUSSIAN_BLUE_FOG, "普鲁士雾灰"}
    };

    // 初始化主题英文模式名映射
    mode_names_ = {
        {LIGHT, "light"},
        {NIGHT, "night"},
        {CHARCOAL_PINK, "charcoal_pink"},
        {DEEPSEA_CREAM, "deepsea_cream"},
        {VINE_PURPLE_TEA_GREEN, "vine_purple_tea_green"},
        {OFFWHITE_CAMELLIA, "offwhite_camellia"},
        {DARK_BLUE_CLEAR_BLUE, "dark_blue_clear_blue"},
        {KLEIN_YELLOW, "klein_yellow"},
        {MARS_GREEN_ROSE, "mars_green_rose"},
        {HERMES_ORANGE_NAVY, "hermes_orange_navy"},
        {TIFFANY_BLUE_CHEESE, "tiffany_blue_cheese"},
        {CHINA_RED_YELLOW, "china_red_yellow"},
        {VANDYKE_BROWN_KHAKI, "vandyke_brown_khaki"},
        {PRUSSIAN_BLUE_FOG, "prussian_blue_fog"}
    };

    // 初始化主题主色映射
    primary_colors_ = {
        {LIGHT, QColor(0xE8, 0xF0, 0xFE)},  // Ice Blue
        {NIGHT, QColor(0x2C, 0x3E, 0x50)},  // Deep Slate
        {CHARCOAL_PINK, QColor(0x1A, 0x1A, 0x1D)},  // Charcoal
        {DEEPSEA_CREAM, QColor(0x12, 0x2E, 0x8A)},  // Deepsea Blue
        {VINE_PURPLE_TEA_GREEN, QColor(0x91, 0xC5, 0x3A)},  // Vine Purple
        {OFFWHITE_CAMELLIA, QColor(0xF1, 0xDD, 0xDF)},  // Offwhite
        {DARK_BLUE_CLEAR_BLUE, QColor(0x11, 0x30, 0x56)},   // Dark Blue
        {KLEIN_YELLOW, QColor(0x00, 0x2E, 0xA6)},   // Klein Blue
        {MARS_GREEN_ROSE, QColor(0x01, 0x84, 0x7F)},    // Mars Green
        {HERMES_ORANGE_NAVY, QColor(0xFF, 0x77, 0x0F)}, // Hermes Orange
        {TIFFANY_BLUE_CHEESE, QColor(0x81, 0xD8, 0xCF)},    // Tiffany Blue
        {CHINA_RED_YELLOW, QColor(0xFF, 0x00, 0x00)},   // China Red
        {VANDYKE_BROWN_KHAKI, QColor(0x49, 0x2D, 0x22)},    // Vandyke Brown
        {PRUSSIAN_BLUE_FOG, QColor(0x00, 0x31, 0x53)}   // Prussian Blue
    };

    // 初始化主题副色映射
    secondary_colors_ = {
        {LIGHT, QColor(0xD4, 0xE6, 0xF1)},  // Misty Blue
        {NIGHT, QColor(0x1A, 0x25, 0x2F)},  // Dark Navy
        {CHARCOAL_PINK, QColor(0xE6, 0x39, 0x7C)},  // Sweet Pink
        {DEEPSEA_CREAM, QColor(0xF5, 0xEF, 0xEA)},  // Soft Milk White
        {VINE_PURPLE_TEA_GREEN, QColor(0x5E, 0x55, 0xA2)},  // Titanium Green
        {OFFWHITE_CAMELLIA, QColor(0xE7, 0x2D, 0x48)},  // Camellia Red
        {DARK_BLUE_CLEAR_BLUE, QColor(0x91, 0xD5, 0xD3)},   // Clear Blue
        {KLEIN_YELLOW, QColor(0xFF, 0xE7, 0x6F)},   // Pine Flower Yellow
        {MARS_GREEN_ROSE, QColor(0xF9, 0xD2, 0xE4)},    // Rose Pink
        {HERMES_ORANGE_NAVY, QColor(0x00, 0x00, 0x26)}, // Navy Blue
        {TIFFANY_BLUE_CHEESE, QColor(0xF8, 0xF5, 0xD6)},    // Cheese Yellow
        {CHINA_RED_YELLOW, QColor(0xFA, 0xEA, 0xD3)},   // Light Yellow
        {VANDYKE_BROWN_KHAKI, QColor(0xD8, 0xC7, 0xB5)},  // Light Khaki
        {PRUSSIAN_BLUE_FOG, QColor(0xE5, 0xDD, 0xD7)}     // Fog Gray
    };

    setup_ui();
    setWindowTitle("选择主题");
    resize(700, 500);

    LOG_MODULE("ThemeSelectorDialog", "ThemeSelectorDialog", LOG_DEBUG,
        "主题选择对话框初始化完成");
}

// ============================================
// 私有辅助函数（private）
// ============================================

void ThemeSelectorDialog::setup_ui() {
    LOG_MODULE("ThemeSelectorDialog", "setup_ui", LOG_DEBUG, "开始构建界面");

    QVBoxLayout* main_layout = new QVBoxLayout(this);
    main_layout->setSpacing(10);

    // 标题标签
    QLabel* title_label = new QLabel("请选择一个主题", this);
    title_label->setAlignment(Qt::AlignCenter);
    QFont title_font = title_label->font();
    title_font.setPointSize(14);
    title_font.setBold(true);
    title_label->setFont(title_font);
    main_layout->addWidget(title_label);

    // 滚动区域
    QScrollArea* scroll_area = new QScrollArea(this);
    scroll_area->setWidgetResizable(true);
    QWidget* container = new QWidget(scroll_area);
    QGridLayout* grid_layout = new QGridLayout(container);
    grid_layout->setSpacing(15);
    grid_layout->setContentsMargins(15, 15, 15, 15);

    int row = 0, col = 0;
    const int max_cols = 2;

    for (int i = LIGHT; i <= PRUSSIAN_BLUE_FOG; ++i) {
        Theme theme = static_cast<Theme>(i);
        QString ch_name = chinese_names_.value(theme, "未知");
        QString mode_name = mode_names_.value(theme, "unknown");
        QColor primary_color = primary_colors_.value(theme, Qt::gray);
        QColor secondary_color = secondary_colors_.value(theme, Qt::lightGray);
        create_theme_card(theme, ch_name, mode_name, primary_color, secondary_color, grid_layout, row, col);

        col++;
        if (col >= max_cols) {
            col = 0;
            row++;
        }
    }

    container->setLayout(grid_layout);
    scroll_area->setWidget(container);
    main_layout->addWidget(scroll_area);

    // 取消按钮
    QPushButton* cancel_btn = new QPushButton("取消", this);
    connect(cancel_btn, &QPushButton::clicked, this, &QDialog::reject);
    main_layout->addWidget(cancel_btn);

    LOG_MODULE("ThemeSelectorDialog", "setup_ui", LOG_DEBUG, "界面构建完成");
}

void ThemeSelectorDialog::create_theme_card(Theme theme,
    const QString& chinese_name,
    const QString& mode_name,
    const QColor& primary_color,
    const QColor& secondary_color,
    QGridLayout* layout,
    int row, int col) {
    ClickableCard* card = new ClickableCard(nullptr);
    card->setMinimumSize(280, 100);

    QVBoxLayout* card_layout = new QVBoxLayout(card);

    // 中文名称
    QLabel* name_label = new QLabel(chinese_name, card);
    QFont name_font = name_label->font();
    name_font.setPointSize(12);
    name_font.setBold(true);
    name_label->setFont(name_font);
    card_layout->addWidget(name_label);

    // 英文模式名
    QLabel* mode_label = new QLabel(mode_name, card);
    mode_label->setStyleSheet("color: gray;");
    card_layout->addWidget(mode_label);

    // 主色和副色块水平布局
    QHBoxLayout* color_layout = new QHBoxLayout();

    // 主色块
    QLabel* primary_block = new QLabel(card);
    primary_block->setFixedSize(40, 20);
    primary_block->setStyleSheet(QString("background-color: %1; border: 1px solid gray;")
        .arg(primary_color.name()));
    color_layout->addWidget(primary_block);

    // 副色块
    QLabel* secondary_block = new QLabel(card);
    secondary_block->setFixedSize(40, 20);
    secondary_block->setStyleSheet(QString("background-color: %1; border: 1px solid gray;")
        .arg(secondary_color.name()));
    color_layout->addWidget(secondary_block);

    
    color_layout->addStretch(); // 右侧弹簧
    card_layout->addLayout(color_layout);
    card_layout->addStretch();  // 底部弹簧

    connect(card, &ClickableCard::clicked, this, [this, theme, mode_name]() {
        LOG_MODULE("ThemeSelectorDialog", "create_theme_card", LOG_DEBUG,
            "用户选择主题: " + mode_name.toStdString());
        emit theme_selected(theme);
        accept();
        });

    layout->addWidget(card, row, col);

    LOG_MODULE("ThemeSelectorDialog", "create_theme_card", LOG_DEBUG,
        QString("创建主题卡片: %1 (%2)").arg(chinese_name, mode_name).toUtf8().constData());
}

#include "ThemeSelectorDialog.moc"
