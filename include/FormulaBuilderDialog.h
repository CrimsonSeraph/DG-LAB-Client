#pragma once

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

class FormulaBuilderDialog : public QDialog {
    Q_OBJECT
public:
    explicit FormulaBuilderDialog(const QString& initialFormula = "", QWidget* parent = nullptr);
    QString get_formula() const;

private:
    QLineEdit* expression_edit_ = nullptr;
    QLabel* status_label_ = nullptr;

    bool expression_validity(const QString& expr, QString* error_msg = nullptr) const;

signals:
    void expression_error(const QString& e) const;

private slots:
    void append_token(const QString& token);
    void clear_formula();
    void validate_and_accept();
    void update_status();
    void show_error(const QString& msg);
};
