#pragma once

#include <QtWidgets/QWidget>
#include "ui_DGLABClinet.h"

class DGLABClinet : public QWidget
{
    Q_OBJECT

public:
    DGLABClinet(QWidget *parent = nullptr);
    ~DGLABClinet();

private:
    Ui::DGLABClinetClass ui;
};

