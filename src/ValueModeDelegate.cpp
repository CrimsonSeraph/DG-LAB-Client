/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "DebugLog.h"
#include "FormulaBuilderDialog.h"
#include "ValueModeDelegate.h"

#include <QEvent>
#include <QMouseEvent>

// ============================================
// 构造/析构（public）
// ============================================

ValueModeDelegate::ValueModeDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {
    LOG_MODULE("ValueModeDelegate", "ValueModeDelegate", LOG_DEBUG, "构造委托对象");
}

// ============================================
// 重写 QStyledItemDelegate 虚函数（public）
// ============================================

QWidget* ValueModeDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option,
    const QModelIndex& index) const {
    LOG_MODULE("ValueModeDelegate", "createEditor", LOG_DEBUG, "禁用默认文本编辑，返回 nullptr");
    return nullptr;
}

// ============================================
// 重写事件处理（protected）
// ============================================

bool ValueModeDelegate::editorEvent(QEvent* event, QAbstractItemModel* model,
    const QStyleOptionViewItem& option, const QModelIndex& index) {
    if (event->type() == QEvent::MouseButtonDblClick) {
        QMouseEvent* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            QString display = index.data(Qt::DisplayRole).toString();
            LOG_MODULE("ValueModeDelegate", "editorEvent", LOG_DEBUG,
                QString("双击单元格，当前显示文本: %1").arg(display).toUtf8().constData());

            FormulaBuilderDialog dlg(display, qobject_cast<QWidget*>(parent()));

            if (dlg.exec() == QDialog::Accepted) {
                QString newRaw = dlg.get_formula();
                QString newDisplay = newRaw;
                newDisplay.replace("{}", "{   }");
                LOG_MODULE("ValueModeDelegate", "editorEvent", LOG_INFO,
                    QString("表达式修改: %1 -> %2").arg(display, newDisplay).toUtf8().constData());
                model->setData(index, newDisplay, Qt::EditRole);
                return true;
            }
            else {
                LOG_MODULE("ValueModeDelegate", "editorEvent", LOG_DEBUG, "用户取消编辑");
            }
        }
    }
    return QStyledItemDelegate::editorEvent(event, model, QStyleOptionViewItem(), index);
}
