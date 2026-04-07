#include "FormulaBuilderDialog.h"

#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QStack>

#include <QVBoxLayout>

FormulaBuilderDialog::FormulaBuilderDialog(const QString& initialFormula, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("编辑值模式 - 计算表达式");
    resize(620, 420);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);

    // 表达式输入框（支持手动编辑）
    expression_edit_ = new QLineEdit(initialFormula, this);
    expression_edit_->setFont(QFont("Consolas", 14));
    expression_edit_->setMinimumHeight(50);
    mainLayout->addWidget(new QLabel("计算式（{} 为占位符）："));
    mainLayout->addWidget(expression_edit_);

    // 状态提示
    status_label_ = new QLabel(this);
    status_label_->setWordWrap(true);
    mainLayout->addWidget(status_label_);

    // 操作按钮网格
    QGridLayout* grid = new QGridLayout();
    grid->setSpacing(6);
    QStringList tokens = { "{}", "+", "-", "*", "/", "(", ")" };
    int r = 0, c = 0;
    for (const QString& t : tokens) {
        QPushButton* btn = new QPushButton(t, this);
        btn->setMinimumSize(70, 50);
        btn->setFont(QFont("Consolas", 16, QFont::Bold));
        connect(btn, &QPushButton::clicked, this, [this, t]() { append_token(t); });
        grid->addWidget(btn, r, c++);
        if (c > 3) { c = 0; ++r; }
    }
    mainLayout->addLayout(grid);

    // 清除按钮
    QPushButton* clearBtn = new QPushButton("清除全部", this);
    connect(clearBtn, &QPushButton::clicked, this, &FormulaBuilderDialog::clear_formula);
    mainLayout->addWidget(clearBtn);

    // 对话框按钮
    QHBoxLayout* btnLayout = new QHBoxLayout();
    QPushButton* okBtn = new QPushButton("确定", this);
    QPushButton* cancelBtn = new QPushButton("取消", this);
    okBtn->setDefault(true);
    connect(okBtn, &QPushButton::clicked, this, &FormulaBuilderDialog::validate_and_accept);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(okBtn);
    btnLayout->addWidget(cancelBtn);
    mainLayout->addLayout(btnLayout);

    // 连接信号槽：表达式错误时显示警告
    connect(this, &FormulaBuilderDialog::expression_error, this, &FormulaBuilderDialog::show_error);

    connect(expression_edit_, &QLineEdit::textChanged, this, &FormulaBuilderDialog::update_status);
    update_status();
}

QString FormulaBuilderDialog::get_formula() const {
    return expression_edit_->text();
}

bool FormulaBuilderDialog::expression_validity(const QString& expr, QString* error_msg) const {
    auto setError = [error_msg](const QString& err) {
        if (error_msg) *error_msg = err;
        return false;
        };

    if (expr.isEmpty())
        return setError("表达式不能为空");

    int i = 0;
    const int len = expr.length();
    bool expectOperand = true;
    QStack<char> parenStack;

    while (i < len) {
        QChar ch = expr[i];

        // 处理占位符 {} 或 {变量名}
        if (ch == '{') {
            int j = expr.indexOf('}', i + 1);
            if (j == -1)
                return setError("缺少闭合的 '}'");

            // {} 或 {xxx} 整体视为一个操作数（里面可以是任意字符或空）
            if (!expectOperand)
                return setError("不允许连续的操作数");

            expectOperand = false;
            i = j + 1;
            continue;
        }

        if (ch == '(') {
            if (!expectOperand) return setError("左括号位置不正确");
            parenStack.push('(');
            expectOperand = true;
            ++i;
        }
        else if (ch == ')') {
            if (parenStack.isEmpty()) return setError("多余的右括号");
            if (expectOperand) return setError("右括号前必须有操作数");
            parenStack.pop();
            expectOperand = false;
            ++i;
        }
        else if (ch == '+' || ch == '-' || ch == '*' || ch == '/') {
            if (expectOperand) return setError("运算符不能出现在开头或连续出现");
            expectOperand = true;
            ++i;
        }
        else if (ch.isSpace()) {
            return setError("表达式中不允许空格");
        }
        else {
            return setError(QString("非法字符 '%1'，只允许 + - * / ( ) { }").arg(ch));
        }
    }

    if (!parenStack.isEmpty())
        return setError("括号不匹配：缺少右括号");
    if (expectOperand)
        return setError("表达式不能以运算符结尾");

    return true;
}

void FormulaBuilderDialog::clear_formula() {
    expression_edit_->clear();
}

void FormulaBuilderDialog::append_token(const QString& token) {
    int pos = expression_edit_->cursorPosition();
    QString text = expression_edit_->text();
    expression_edit_->setText(text.left(pos) + token + text.mid(pos));
    expression_edit_->setCursorPosition(pos + token.length());
}

void FormulaBuilderDialog::validate_and_accept() {
    QString error_msg;
    if (expression_validity(expression_edit_->text(), &error_msg)) {
        accept();
    }
    else {
        emit expression_error(error_msg);
    }
}

void FormulaBuilderDialog::update_status() {
    QString error;
    if (expression_validity(expression_edit_->text(), &error)) {
        status_label_->setText("<font color='green'>✓ 表达式合法</font>");
    }
    else {
        status_label_->setText("<font color='red'>✗ " + error + "</font>");
    }
}

void FormulaBuilderDialog::show_error(const QString& msg) {
    QMessageBox::warning(this, "表达式错误", msg);
}
