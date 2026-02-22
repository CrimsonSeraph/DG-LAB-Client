#include "DGLABClient.h"

#include <iostream>

DGLABClient::DGLABClient(QWidget* parent)
    : QWidget(parent) {
    ui.setupUi(this);

    connect(ui.main_first_btn, &QPushButton::clicked, this, &DGLABClient::on_main_first_btn_clicked);
    connect(ui.main_config_btn, &QPushButton::clicked, this, &DGLABClient::on_main_config_btn_clicked);
    connect(ui.main_setting_btn, &QPushButton::clicked, this, &DGLABClient::on_main_setting_btn_clicked);
    connect(ui.main_about_btn, &QPushButton::clicked, this, &DGLABClient::on_main_about_btn_clicked);
}

DGLABClient::~DGLABClient() {
}

void DGLABClient::on_main_first_btn_clicked() {
    std::cout << "main_first_btn clicked!" << std::endl;
}

void DGLABClient::on_main_config_btn_clicked() {
    std::cout << "main_config_btn clicked!" << std::endl;
}

void DGLABClient::on_main_setting_btn_clicked() {
    std::cout << "main_setting_btn clicked!" << std::endl;
}

void DGLABClient::on_main_about_btn_clicked() {
    std::cout << "main_about_btn clicked!" << std::endl;
}
