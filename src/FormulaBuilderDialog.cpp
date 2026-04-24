/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "DebugLog.h"
#include "FormulaBuilderDialog.h"

#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QStack>
#include <QVBoxLayout>

// ============================================
// 构造/析构（public）
// ============================================

FormulaBuilderDialog::FormulaBuilderDialog(const QString& initialFormula, QWidget* parent)
    : QDialog(parent) {
    LOG_MODULE("FormulaBuilderDialog", "FormulaBuilderDialog", LOG_DEBUG,
        QString("构造对话框，初始表达式: %1").arg(initialFormula).toUtf8().constData());

    setWindowTitle("编辑值模式 - 计算表达式");
    resize(620, 420);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);

    expression_edit_ = new QLineEdit(initialFormula, this);
    expression_edit_->setFont(QFont("Consolas", 14));
    expression_edit_->setMinimumHeight(50);
    mainLayout->addWidget(new QLabel("计算式（{} 为占位符，仅支持整数）: "));
    mainLayout->addWidget(expression_edit_);

    status_label_ = new QLabel(this);
    status_label_->setWordWrap(true);
    mainLayout->addWidget(status_label_);

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

    QPushButton* clearBtn = new QPushButton("清除全部", this);
    connect(clearBtn, &QPushButton::clicked, this, &FormulaBuilderDialog::clear_formula);
    mainLayout->addWidget(clearBtn);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    QPushButton* okBtn = new QPushButton("确定", this);
    QPushButton* cancelBtn = new QPushButton("取消", this);
    okBtn->setDefault(true);
    connect(okBtn, &QPushButton::clicked, this, &FormulaBuilderDialog::validate_and_accept);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(okBtn);
    btnLayout->addWidget(cancelBtn);
    mainLayout->addLayout(btnLayout);

    connect(this, &FormulaBuilderDialog::expression_error, this, &FormulaBuilderDialog::show_error);
    connect(expression_edit_, &QLineEdit::textChanged, this, &FormulaBuilderDialog::update_status);
    update_status();
}

QString FormulaBuilderDialog::get_formula() const {
    LOG_MODULE("FormulaBuilderDialog", "get_formula", LOG_DEBUG,
        QString("返回表达式: %1").arg(expression_edit_->text()).toUtf8().constData());
    return expression_edit_->text();
}

// ============================================
// 私有辅助函数（private）
// ============================================

bool FormulaBuilderDialog::expression_validity(const QString& expr, QString* error_msg, bool suppress_log) const {
    if (!suppress_log) {
        LOG_MODULE("FormulaBuilderDialog", "expression_validity", LOG_DEBUG,
            QString("开始验证表达式: %1").arg(expr).toUtf8().constData());
    }

    auto set_error = [error_msg, suppress_log](const QString& err) {
        if (error_msg) *error_msg = err;
        if (!suppress_log) {
            LOG_MODULE("FormulaBuilderDialog", "expression_validity", LOG_ERROR, err.toUtf8().constData());
        }
        return false;
        };

    if (expr.isEmpty()) {
        return set_error("表达式不能为空");
    }

    int i = 0;
    const int len = expr.length();
    bool expectOperand = true;
    QStack<char> parenStack;

    while (i < len) {
        QChar ch = expr[i];

        if (ch == '{') {
            int j = expr.indexOf('}', i + 1);
            if (j == -1) {
                return set_error("缺少闭合的 '}'");
            }
            if (!expectOperand) {
                return set_error("不允许连续的操作数");
            }
            expectOperand = false;
            i = j + 1;
            continue;
        }

        if (ch.isDigit()) {
            if (!expectOperand) {
                return set_error("不允许连续的操作数");
            }
            while (i < len && expr[i].isDigit()) {
                ++i;
            }
            expectOperand = false;
            continue;
        }

        if (ch == '(') {
            if (!expectOperand) {
                return set_error("左括号位置不正确");
            }
            parenStack.push('(');
            expectOperand = true;
            ++i;
            continue;
        }
        else if (ch == ')') {
            if (parenStack.isEmpty()) {
                return set_error("多余的右括号");
            }
            if (expectOperand) {
                return set_error("右括号前必须有操作数");
            }
            parenStack.pop();
            expectOperand = false;
            ++i;
            continue;
        }
        else if (ch == '+' || ch == '-' || ch == '*' || ch == '/') {
            if (expectOperand) {
                return set_error("运算符不能出现在开头或连续出现");
            }
            expectOperand = true;
            ++i;
            continue;
        }
        else if (ch.isSpace()) {
            return set_error("表达式中不允许空格");
        }
        else {
            return set_error(QString("非法字符 '%1'，只允许 + - * / ( ) { } 和数字").arg(ch));
        }
    }

    if (!parenStack.isEmpty()) {
        return set_error("括号不匹配: 缺少右括号");
    }
    if (expectOperand) {
        return set_error("表达式不能以运算符结尾");
    }

    if (!suppress_log) {
        LOG_MODULE("FormulaBuilderDialog", "expression_validity", LOG_DEBUG, "表达式验证通过");
    }
    return true;
}

// ============================================
// private slots 实现
// ============================================

void FormulaBuilderDialog::append_token(const QString& token) {
    int pos = expression_edit_->cursorPosition();
    QString text = expression_edit_->text();
    expression_edit_->setText(text.left(pos) + token + text.mid(pos));
    expression_edit_->setCursorPosition(pos + token.length());
}

void FormulaBuilderDialog::clear_formula() {
    LOG_MODULE("FormulaBuilderDialog", "clear_formula", LOG_DEBUG, "清除全部表达式");
    expression_edit_->clear();
}

void FormulaBuilderDialog::validate_and_accept() {
    LOG_MODULE("FormulaBuilderDialog", "validate_and_accept", LOG_DEBUG, "用户点击确定，开始验证");

    QString error_msg;
    if (expression_validity(expression_edit_->text(), &error_msg, false)) {
        LOG_MODULE("FormulaBuilderDialog", "validate_and_accept", LOG_INFO, "表达式合法，接受对话框");
        accept();
    }
    else {
        LOG_MODULE("FormulaBuilderDialog", "validate_and_accept", LOG_WARN,
            QString("表达式非法: %1").arg(error_msg).toUtf8().constData());
        emit expression_error(error_msg);
    }
}

void FormulaBuilderDialog::update_status() {
    QString error;
    if (expression_validity(expression_edit_->text(), &error, true)) {
        status_label_->setText("<font color='green'>✓ 表达式合法</font>");
    }
    else {
        status_label_->setText("<font color='red'>✗ " + error + "</font>");
    }
}

void FormulaBuilderDialog::show_error(const QString& msg) {
    LOG_MODULE("FormulaBuilderDialog", "show_error", LOG_WARN,
        QString("显示错误弹窗: %1").arg(msg).toUtf8().constData());
    QMessageBox::warning(this, "表达式错误", msg);
}
