#include "FileComboBox.h"

#include <QAbstractItemView>
#include <QStylePainter>
#include <QStyleOptionComboBox>

FileComboBox::FileComboBox(QWidget* parent) : QComboBox(parent) {
    setEditable(false);
    setStyleSheet(
        "QComboBox {"
        "    background: rgba(255,255,255,0.9);"
        "    border: 1px solid rgba(141,165,187,1.0);"
        "    border-radius: 9px;"
        "    padding: 4px 8px;"
        "    min-height: 28px;"
        "}"
        "QComboBox::drop-down {"
        "    width: 0px;"
        "    border: none;"
        "}"
        "QComboBox::down-arrow {"
        "    image: none;"
        "    width: 0;"
        "    height: 0;"
        "}"
    );
    setAttribute(Qt::WA_StyledBackground, true);
}

void FileComboBox::showEvent(QShowEvent* event) {
    if (view()) {
        view()->setStyleSheet(
            "QAbstractItemView {"
            "    background-color: rgba(255,255,255,0.98);"
            "    border: 1px solid rgba(141,165,187,1.0);"
            "    border-radius: 6px;"
            "    selection-background-color: rgba(212,230,241,1.0);"
            "    selection-color: rgba(26,43,60,1.0);"
            "    outline: 0;"
            "    margin: 0;"
            "    padding: 0;"
            "}"
            "QAbstractItemView::item {"
            "    padding: 4px 8px;"
            "    min-height: 22px;"
            "}"
        );
    }
    QComboBox::showEvent(event);
}

void FileComboBox::paintEvent(QPaintEvent* event) {
    QStylePainter painter(this);
    QStyleOptionComboBox opt;
    initStyleOption(&opt);
    painter.drawComplexControl(QStyle::CC_ComboBox, opt);
    if (!opt.currentText.isEmpty()) {
        painter.drawControl(QStyle::CE_ComboBoxLabel, opt);
    }
    QRect arrowRect = style()->subControlRect(QStyle::CC_ComboBox, &opt, QStyle::SC_ComboBoxArrow, this);
    if (arrowRect.isValid()) {
        QPoint center = arrowRect.center();
        QPolygon triangle;
        triangle << QPoint(center.x() - 4, center.y() - 2)
            << QPoint(center.x() + 4, center.y() - 2)
            << QPoint(center.x(), center.y() + 3);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(44, 62, 80));
        painter.drawPolygon(triangle);
    }
}
