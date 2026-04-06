#pragma once

#include <QComboBox>
#include <QStyledItemDelegate>

class ComboBoxDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit ComboBoxDelegate(const QStringList& items, QObject* parent = nullptr);

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;

    void setEditorData(QWidget* editor, const QModelIndex& index) const override;
    void setModelData(QWidget* editor, QAbstractItemModel* model,
        const QModelIndex& index) const override;

    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;

private:
    QStringList m_items;    // 下拉选项列表
};
