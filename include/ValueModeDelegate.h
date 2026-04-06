#pragma once

#include <QStyledItemDelegate>

class ValueModeDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit ValueModeDelegate(QObject* parent = nullptr);

    // 禁用默认文本编辑器
    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;

protected:
    // 双击时弹出公式构建对话框
    bool editorEvent(QEvent* event, QAbstractItemModel* model,
        const QStyleOptionViewItem& option, const QModelIndex& index) override;
};
