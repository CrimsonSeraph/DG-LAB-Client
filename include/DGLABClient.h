#pragma once

#include <QtWidgets/QWidget>
#include "ui_DGLABClient.h"

class DGLABClient : public QWidget
{
    Q_OBJECT

public:
    DGLABClient(QWidget* parent = nullptr);
    ~DGLABClient();

private:
    Ui::DGLABClientClass ui;
};

