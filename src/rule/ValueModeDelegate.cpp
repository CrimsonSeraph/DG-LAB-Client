/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "ValueModeDelegate.h"
#include "DebugLog.h"
#include "FormulaBuilderDialog.h"
#include "RuleManager.h"

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

            // 传入当前行的规则序号，使引用列表排除自身
            QString rule_name = model->index(index.row(), 1).data(Qt::DisplayRole).toString();
            int rule_index = RuleManager::instance().get_rule_index(rule_name.toStdString());
            // 用原始值模式作为初始公式（显示文本中的 {   } 占位还原为 {}）
            QString initial = display;
            initial.replace("{   }", "{}");
            FormulaBuilderDialog dlg(initial, qobject_cast<QWidget*>(parent()), rule_index);

            if (dlg.exec() == QDialog::Accepted) {
                QString newRaw = dlg.get_formula();
                LOG_MODULE("ValueModeDelegate", "editorEvent", LOG_INFO,
                    QString("表达式修改: %1 -> %2").arg(display, newRaw).toUtf8().constData());
                // 写回原始表达式（表格刷新时会重新格式化显示，itemChanged 同步到规则管理器）
                model->setData(index, newRaw, Qt::EditRole);
                return true;
            }
            else {
                LOG_MODULE("ValueModeDelegate", "editorEvent", LOG_DEBUG, "用户取消编辑");
            }
        }
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}
