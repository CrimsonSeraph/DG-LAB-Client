#include "FormulaBuilderDialog.h"

#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QMessageBox>
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

    connect(expression_edit_, &QLineEdit::textChanged, this, &FormulaBuilderDialog::update_status);
    update_status();
}

void FormulaBuilderDialog::append_token(const QString& token) {
    int pos = expression_edit_->cursorPosition();
    QString text = expression_edit_->text();
    expression_edit_->setText(text.left(pos) + token + text.mid(pos));
    expression_edit_->setCursorPosition(pos + token.length());
}

void FormulaBuilderDialog::clear_formula() {
    expression_edit_->clear();
}

void FormulaBuilderDialog::validate_and_accept() {
    if (parentheses_balanced(expression_edit_->text())) {
        accept();
    }
    else {
        QMessageBox::warning(this, "括号错误", "括号不匹配或提前闭合，请检查！");
    }
}

void FormulaBuilderDialog::update_status() {
    if (parentheses_balanced(expression_edit_->text())) {
        status_label_->setText("<font color='green'>✓ 括号匹配合法</font>");
    }
    else {
        status_label_->setText("<font color='red'>✗ 括号不匹配</font>");
    }
}

bool FormulaBuilderDialog::parentheses_balanced(const QString& expr) const {
    int count = 0;
    for (const QChar& ch : expr) {
        if (ch == '(') ++count;
        else if (ch == ')') --count;
        if (count < 0) return false;
    }
    return count == 0;
}

QString FormulaBuilderDialog::get_formula() const {
    return expression_edit_->text();
}
