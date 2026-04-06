#include "ComboBoxDelegate.h"

#include <QTimer>

ComboBoxDelegate::ComboBoxDelegate(const QStringList& items, QObject* parent)
    : QStyledItemDelegate(parent), items_(items) {}

QWidget* ComboBoxDelegate::createEditor(QWidget* parent,
    const QStyleOptionViewItem& option,
    const QModelIndex& index) const {
    QComboBox* combo = new QComboBox(parent);
    combo->addItems(items_);
    combo->setEditable(false);  // 不允许手动输入
    combo->setAutoFillBackground(true);
    combo->setGeometry(option.rect);
    combo->showPopup();  // 点击后立即展开下拉
    return combo;
}

void ComboBoxDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const {
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

    model->setData(index, combo->currentText(), Qt::EditRole);
}

void ComboBoxDelegate::updateEditorGeometry(QWidget* editor,
    const QStyleOptionViewItem& option,
    const QModelIndex& index) const {
    if (!editor) return;
    QRect rect = option.rect;
    if (rect.width() < 20 || rect.height() < 10) {
        rect = option.rect;
    }

    editor->setGeometry(rect);
    editor->updateGeometry();
    editor->update();

    if (QComboBox* combo = qobject_cast<QComboBox*>(editor)) {
        combo->update();
        QTimer::singleShot(0, combo, &QComboBox::showPopup);
    }
}
