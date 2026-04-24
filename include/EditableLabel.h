#pragma once

#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QString>
#include <QValidator>

class EditableLabel : public QLabel
{
    Q_OBJECT

public:
    /// @brief 构造函数
    /// @param parent 父窗口
    explicit EditableLabel(QWidget* parent = nullptr);

    /// @brief 析构函数（默认）
    ~EditableLabel() = default;

    /// @brief 设置输入验证器
    void set_validator(const QValidator* validator);

    /// @brief 是否运行编辑
    void set_editable(bool editable);

signals:
    /// @brief 文本被编辑完成时发出的信号
    /// @param new_text 编辑后的新文本
    void text_edited(const QString& new_text);

    /// @brief 双击发出信号
    void double_clicked();

protected:
    /// @brief 重写鼠标双击事件，进入编辑模式
    /// @param event 鼠标事件对象
    void mouseDoubleClickEvent(QMouseEvent* event) override;

    /// @brief 重写大小改变事件，调整内嵌 QLineEdit 的位置和大小
    /// @param event 大小事件对象
    void resizeEvent(QResizeEvent* event) override;

private slots:
    /// @brief 编辑完成（失去焦点或按下回车）时的槽函数
    void on_editing_finished();

private:
    bool editable_; ///< 是否允许编辑
    QLineEdit* line_edit_;  ///< 内嵌的文本编辑器
    bool editing_;  ///< 是否正处于编辑模式

    /// @brief 进入编辑模式: 显示 QLineEdit 并隐藏标签自身内容
    void enter_edit_mode();

    /// @brief 退出编辑模式: 将编辑器的文本同步回标签，隐藏编辑器
    void exit_edit_mode();

    /// @brief 调整编辑器的几何尺寸，使其与标签可视区域一致
    void update_editor_geometry();
};
