/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "StyledComboBox.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QPainterPath>
#include <QRegion>

// ============================================
// 构造/析构（public）
// ============================================

StyledComboBox::StyledComboBox(QWidget* parent)
    : QComboBox(parent) {
    // 参照规则表格下拉框委托的修复方式：清空内联样式、标记样式化背景、
    // 使用应用级样式绘制，规避弹出列表边缘黑色（系统阴影）问题
    setStyleSheet("");
    setAttribute(Qt::WA_StyledBackground, true);
    setStyle(qApp->style());
}

// ============================================
// 重写事件（protected）
// ============================================

void StyledComboBox::showPopup() {
    QComboBox::showPopup();
    // 弹出菜单圆角裁剪：容器为 Qt::Popup 顶层窗口，设置圆角区域保证视觉圆角
    if (QWidget* container = view()->window()) {
        const QRect rect = container->rect();
        if (rect.width() > 12 && rect.height() > 12) {
            QPainterPath path;
            const int radius = 8;
            path.addRoundedRect(QRectF(rect).adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
            container->setMask(QRegion(path.toFillPolygon().toPolygon()));
        }
    }
}
