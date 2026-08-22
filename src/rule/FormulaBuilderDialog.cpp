/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "FormulaBuilderDialog.h"

#include "DebugLog.h"
#include "Module.h"
#include "ModuleManager.h"
#include "RuleManager.h"

#include <QAction>
#include <QColor>
#include <QCursor>
#include <QFont>
#include <QGridLayout>
#include <QRegularExpression>
#include <QHBoxLayout>
#include <QMenu>
#include <QMessageBox>
#include <QStack>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QVBoxLayout>

// ============================================
// PlaceholderHighlighter - 表达式注释高亮器（局部辅助类）
// 将 {id:xxx(名称)} 中的 (名称) 部分显示为灰色
// ============================================
class PlaceholderHighlighter : public QSyntaxHighlighter {
public:
    /// @brief 构造函数
    /// @param doc 目标文档
    explicit PlaceholderHighlighter(QTextDocument* doc)
        : QSyntaxHighlighter(doc) {
    }

protected:
    /// @brief 高亮块：将 {id:xxx(名称)} 的括号注释部分设为灰色
    /// @param text 文本块内容
    void highlightBlock(const QString& text) override {
        // 匹配 {id:xxx(注释)} 中的 (注释) 部分
        QRegularExpression pattern("\{id:[^\{\}]*\([^\(\)]*\)\}");
        QRegularExpressionMatchIterator it = pattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            QString whole = match.captured(0);
            int open_paren = whole.indexOf('(');
            int close_paren = whole.lastIndexOf(')');
            if (open_paren >= 0 && close_paren > open_paren) {
                QTextCharFormat gray;
                gray.setForeground(QColor(150, 150, 150));
                int abs_start = match.capturedStart(0) + open_paren;
                int len = close_paren - open_paren + 1;
                setFormat(abs_start, len, gray);
            }
        }
    }
};

// ============================================
// 构造/析构（public）
// ============================================

FormulaBuilderDialog::FormulaBuilderDialog(const QString& initialFormula, QWidget* parent,
    int currentRuleIndex)
    : QDialog(parent)
    , current_rule_index_(currentRuleIndex) {
    LOG_MODULE("FormulaBuilderDialog", "FormulaBuilderDialog", LOG_DEBUG,
        QString("构造对话框，初始表达式: %1，当前规则序号: %2")
            .arg(initialFormula)
            .arg(currentRuleIndex)
            .toUtf8()
            .constData());

    setWindowTitle("编辑值模式 - 计算表达式");
    resize(640, 460);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);

    expression_edit_ = new QTextEdit(initialFormula, this);
    expression_edit_->setFont(QFont("Consolas", 14));
    expression_edit_->setMinimumHeight(70);
    expression_edit_->setAcceptRichText(false);
    mainLayout->addWidget(new QLabel("计算式（{id:xxx(名称)} 引用数值、{rule:xx} 引用规则、{} 为空值占位）: "));
    mainLayout->addWidget(expression_edit_);
    // 括号注释部分显示为灰色
    new PlaceholderHighlighter(expression_edit_->document());

    status_label_ = new QLabel(this);
    status_label_->setWordWrap(true);
    mainLayout->addWidget(status_label_);

    QGridLayout* grid = new QGridLayout();
    grid->setSpacing(6);
    QStringList tokens = {"{}", "+", "-", "*", "/", "(", ")"};
    int r = 0, c = 0;
    for (const QString& t : tokens) {
        QPushButton* btn = new QPushButton(t, this);
        btn->setMinimumSize(70, 50);
        btn->setFont(QFont("Consolas", 16, QFont::Bold));
        connect(btn, &QPushButton::clicked, this, [this, t]() { append_token(t); });
        grid->addWidget(btn, r, c++);
        if (c > 3) {
            c = 0;
            ++r;
        }
    }
    mainLayout->addLayout(grid);

    // 可用数值/规则选择按钮
    QPushButton* valuesBtn = new QPushButton("显示可用数值", this);
    valuesBtn->setToolTip("点击后显示当前可获取的数值与规则，点击插入引用");
    connect(valuesBtn, &QPushButton::clicked, this, &FormulaBuilderDialog::show_available_values);
    mainLayout->addWidget(valuesBtn);

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
    connect(expression_edit_, &QTextEdit::textChanged, this, &FormulaBuilderDialog::update_status);
    update_status();
}

QString FormulaBuilderDialog::get_formula() const {
    LOG_MODULE("FormulaBuilderDialog", "get_formula", LOG_DEBUG,
        QString("返回表达式: %1").arg(expression_edit_->toPlainText()).toUtf8().constData());
    return expression_edit_->toPlainText();
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
    QTextCursor cursor = expression_edit_->textCursor();
    cursor.insertText(token);
    expression_edit_->setTextCursor(cursor);
}

void FormulaBuilderDialog::clear_formula() {
    LOG_MODULE("FormulaBuilderDialog", "clear_formula", LOG_DEBUG, "清除全部表达式");
    expression_edit_->clear();
}

void FormulaBuilderDialog::show_available_values() {
    LOG_MODULE("FormulaBuilderDialog", "show_available_values", LOG_DEBUG, "弹出可用数值/规则选择菜单");

    // 弹出菜单，分组列出可引用的数值与规则
    QMenu menu(this);
    QMenu* values_menu = menu.addMenu("数值（点击插入 {id:xxx(名称)}）");
    QMenu* rules_menu = menu.addMenu("规则（点击插入 {rule:xx}）");

    // 列出模块数值（名称 + ID）
    auto& module_manager = ModuleManager::instance();
    for (const auto& module_name : module_manager.get_module_names()) {
        const Module* module = module_manager.get_module(module_name);
        if (!module) continue;
        for (const auto& value : module->get_values()) {
            QString label = QString("%1 (%2)")
                    .arg(QString::fromStdString(value.get_name()))
                    .arg(QString::fromStdString(value.get_id()));
            QAction* action = values_menu->addAction(label);
            connect(action, &QAction::triggered, this, [this, value]() {
                // 插入 {id:xxx(名称)}，括号内为名称注释（显示为灰色）
                QString token = QString("{id:%1(%2)}")
                        .arg(QString::fromStdString(value.get_id()))
                        .arg(QString::fromStdString(value.get_name()));
                append_token(token);
            });
        }
    }

    // 列出除自身外的所有规则（名称 + 序号），包含父级为通道的规则
    // 规则可以有通道与其他规则的混合父级，通道是否启用由独立的通道启用变量决定
    auto& rule_manager = RuleManager::instance();
    for (const auto& rule_name : rule_manager.get_rule_names()) {
        int index = rule_manager.get_rule_index(rule_name);
        // 排除自身，避免自引用
        if (index == current_rule_index_) {
            continue;
        }
        QString label = QString("%1 [#%2]")
                .arg(QString::fromStdString(rule_name))
                .arg(index);
        QAction* action = rules_menu->addAction(label);
        connect(action, &QAction::triggered, this, [this, index]() {
            append_token(QString("{rule:%1}").arg(index));
        });
    }

    menu.exec(QCursor::pos());
}

void FormulaBuilderDialog::validate_and_accept() {
    LOG_MODULE("FormulaBuilderDialog", "validate_and_accept", LOG_DEBUG, "用户点击确定，开始验证");

    QString error_msg;
    if (expression_validity(expression_edit_->toPlainText(), &error_msg, false)) {
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
    if (expression_validity(expression_edit_->toPlainText(), &error, true)) {
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