#pragma once

#include <QStyledItemDelegate>

// ============================================
// ValueModeDelegate - 值模式列的自定义委托（双击弹出公式构建对话框）
// ============================================
class ValueModeDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    // -------------------- 构造/析构 --------------------
    /// @brief 构造函数
    explicit ValueModeDelegate(QObject* parent = nullptr);

    // -------------------- 重写 QStyledItemDelegate 虚函数 --------------------
    /// @brief 禁用默认文本编辑器（返回 nullptr，不创建内联编辑控件）
    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;

protected:
    // -------------------- 重写事件处理 --------------------
    /// @brief 处理双击事件，弹出公式构建对话框进行编辑
    bool editorEvent(QEvent* event, QAbstractItemModel* model,
        const QStyleOptionViewItem& option, const QModelIndex& index) override;
};
