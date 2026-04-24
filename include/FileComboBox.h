#pragma once
#include <QComboBox>

class FileComboBox : public QComboBox {
    Q_OBJECT
public:
    explicit FileComboBox(QWidget* parent = nullptr);
protected:
    void showEvent(QShowEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
};
