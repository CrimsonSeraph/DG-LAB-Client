#pragma once

#include <QtWidgets/QWidget>
#include "ui_DGLABClient.h"

class DGLABClient : public QWidget
{
    Q_OBJECT

public:
    DGLABClient(QWidget* parent = nullptr);
    ~DGLABClient();

    void on_main_first_btn_clicked();
    void on_main_config_btn_clicked();
    void on_main_setting_btn_clicked();
    void on_main_about_btn_clicked();

private:
    Ui::DGLABClientClass ui;
};

