#pragma once

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

// ============================================
// FormulaBuilderDialog - 表达式构建对话框（用于规则值模式编辑）
// ============================================
class FormulaBuilderDialog : public QDialog {
    Q_OBJECT

public:
    // -------------------- 构造/析构 --------------------
    /// @brief 构造函数
    /// @param initialFormula 初始表达式内容
    /// @param parent 父窗口指针
    explicit FormulaBuilderDialog(const QString& initialFormula = "", QWidget* parent = nullptr);

    // -------------------- 公共接口 --------------------
    /// @brief 获取当前编辑的表达式
    /// @return 表达式字符串
    QString get_formula() const;

private:
    // -------------------- 成员变量 --------------------
    QLineEdit* expression_edit_ = nullptr;   ///< 表达式输入框
    QLabel* status_label_ = nullptr;         ///< 状态提示标签

    // -------------------- 私有辅助函数 --------------------
    /// @brief 验证表达式合法性
    /// @param expr 待验证的表达式
    /// @param error_msg 输出错误信息（可选）
    /// @param suppress_log 是否抑制日志输出（用于高频调用）
    /// @return 合法返回 true
    bool expression_validity(const QString& expr, QString* error_msg = nullptr, bool suppress_log = false) const;

signals:
    /// @brief 表达式错误信号（由验证失败时触发）
    void expression_error(const QString& e) const;

private slots:
    /// @brief 向表达式末尾追加一个 token（按钮点击）
    void append_token(const QString& token);
    /// @brief 清空表达式
    void clear_formula();
    /// @brief 验证表达式并接受对话框（如果合法）
    void validate_and_accept();
    /// @brief 实时更新状态提示（合法/非法）
    void update_status();
    /// @brief 显示错误弹窗
    void show_error(const QString& msg);
};
