#include "ComboBoxDelegate.h"
#include "DebugLog.h"

#include <QTimer>

// ============================================
// 构造/析构（public）
// ============================================

ComboBoxDelegate::ComboBoxDelegate(const QStringList& items, QObject* parent)
    : QStyledItemDelegate(parent), items_(items) {
    LOG_MODULE("ComboBoxDelegate", "ComboBoxDelegate", LOG_DEBUG,
        QString("构造下拉框委托，选项数量: %1").arg(items.size()).toUtf8().constData());
}

// ============================================
// 重写 QStyledItemDelegate 虚函数（public）
// ============================================

QWidget* ComboBoxDelegate::createEditor(QWidget* parent,
    const QStyleOptionViewItem& option,
    const QModelIndex& index) const {
    LOG_MODULE("ComboBoxDelegate", "createEditor", LOG_DEBUG,
        QString("创建编辑器，索引: (%1,%2)").arg(index.row()).arg(index.column()).toUtf8().constData());

    QComboBox* combo = new QComboBox(parent);
    combo->addItems(items_);
    combo->setEditable(false);
    combo->setAutoFillBackground(true);
    combo->setGeometry(option.rect);
    combo->showPopup();
    return combo;
}

void ComboBoxDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const {
    LOG_MODULE("ComboBoxDelegate", "setEditorData", LOG_DEBUG,
        QString("设置编辑器数据，当前显示值: %1").arg(index.data(Qt::DisplayRole).toString()).toUtf8().constData());

    QComboBox* combo = qobject_cast<QComboBox*>(editor);
    if (!combo) return;

    QString currentText = index.data(Qt::DisplayRole).toString();
    int idx = combo->findText(currentText);
    if (idx >= 0)
        combo->setCurrentIndex(idx);
    else
        combo->setCurrentText(currentText);
}

void ComboBoxDelegate::setModelData(QWidget* editor, QAbstractItemModel* model,
    const QModelIndex& index) const {
    QComboBox* combo = qobject_cast<QComboBox*>(editor);
    if (!combo) return;

    QString newValue = combo->currentText();
    LOG_MODULE("ComboBoxDelegate", "setModelData", LOG_DEBUG,   // 原 LOG_INFO 降级为 LOG_DEBUG
        QString("将编辑器值写入模型: %1 -> %2")
        .arg(index.data(Qt::DisplayRole).toString(), newValue).toUtf8().constData());

    model->setData(index, newValue, Qt::EditRole);
}

void ComboBoxDelegate::updateEditorGeometry(QWidget* editor,
    const QStyleOptionViewItem& option,
    const QModelIndex& index) const {
    if (!editor) return;

    QRect rect = option.rect;
    if (rect.width() < 20 || rect.height() < 10) {
        rect = option.rect;
    }

    LOG_MODULE("ComboBoxDelegate", "updateEditorGeometry", LOG_DEBUG,
        QString("更新编辑器几何位置: (%1,%2,%3,%4)")
        .arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height()).toUtf8().constData());

    editor->setGeometry(rect);
    editor->updateGeometry();
    editor->update();

    if (QComboBox* combo = qobject_cast<QComboBox*>(editor)) {
        combo->update();
        QTimer::singleShot(0, combo, &QComboBox::showPopup);
    }
}
