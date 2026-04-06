#include "ComboBoxDelegate.h"

ComboBoxDelegate::ComboBoxDelegate(const QStringList& items, QObject* parent)
    : QStyledItemDelegate(parent), m_items(items) {}

QWidget* ComboBoxDelegate::createEditor(QWidget* parent,
    const QStyleOptionViewItem&,
    const QModelIndex&) const {
    QComboBox* combo = new QComboBox(parent);
    combo->addItems(m_items);
    combo->setEditable(false);  // 不允许手动输入
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
    const QModelIndex&) const {
    editor->setGeometry(option.rect);
}
