#include "EditableLabel.h"

#include <QLabel>
#include <QLineEdit>
#include <QString>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QValidator>

EditableLabel::EditableLabel(QWidget* parent)
    : QLabel(parent)
    , line_edit_(new QLineEdit(this))
    , editing_(false)
    , editable_(true) {
    line_edit_->setVisible(false);
    line_edit_->setFrame(false);
    line_edit_->setText(this->text());

    // 连接编辑完成信号
    connect(line_edit_, &QLineEdit::editingFinished,
        this, &EditableLabel::on_editing_finished);
    connect(line_edit_, &QLineEdit::returnPressed,
        this, &EditableLabel::on_editing_finished);
}

void EditableLabel::set_validator(const QValidator* validator) {
    line_edit_->setValidator(validator);
}

void EditableLabel::set_editable(bool editable) {
    editable_ = editable;
}

void EditableLabel::mouseDoubleClickEvent(QMouseEvent* event) {
    emit double_clicked();
    if (event->button() == Qt::LeftButton && !editing_ && editable_) {
        enter_edit_mode();
    }
    QLabel::mouseDoubleClickEvent(event);
}

void EditableLabel::resizeEvent(QResizeEvent* event) {
    QLabel::resizeEvent(event);
    if (editing_) {
        update_editor_geometry();
    }
}

void EditableLabel::on_editing_finished() {
    if (editing_) {
        exit_edit_mode();
    }
}

void EditableLabel::enter_edit_mode() {
    if (editing_)
        return;

    editing_ = true;
    line_edit_->setText(this->text());
    update_editor_geometry();
    line_edit_->setVisible(true);
    line_edit_->setFocus();
    line_edit_->selectAll();
}

void EditableLabel::exit_edit_mode() {
    if (!editing_)
        return;

    QString new_text = line_edit_->text();
    if (new_text != this->text()) {
        this->setText(new_text);
        emit text_edited(new_text);
    }

    line_edit_->setVisible(false);
    editing_ = false;
}

void EditableLabel::update_editor_geometry() {
    QRect rect = contentsRect();
    line_edit_->setGeometry(rect);
}
