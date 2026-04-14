#pragma once

#include <QComboBox>
#include <QStyledItemDelegate>

// ============================================
// ComboBoxDelegate - 为表格/列表提供下拉框编辑器的委托类
// ============================================
class ComboBoxDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    // -------------------- 构造/析构 --------------------
    /// @brief 构造函数
    /// @param items 下拉框选项列表
    /// @param parent 父对象指针
    explicit ComboBoxDelegate(const QStringList& items, QObject* parent = nullptr);

    // -------------------- 重写 QStyledItemDelegate 虚函数 --------------------
    /// @brief 创建编辑器控件（QComboBox）
    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;

    /// @brief 将模型数据设置到编辑器
    void setEditorData(QWidget* editor, const QModelIndex& index) const override;

    /// @brief 将编辑器数据写回模型
    void setModelData(QWidget* editor, QAbstractItemModel* model,
        const QModelIndex& index) const override;

    /// @brief 更新编辑器的几何位置
    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;

private:
    // -------------------- 成员变量 --------------------
    QStringList items_;   ///< 下拉选项列表
};
