#include "DGLABClient.h"
#include "DebugLog.h"

#include <iostream>
#include <QPixmap>
#include <QFile>

DGLABClient::DGLABClient(QWidget* parent)
    : QWidget(parent) {
    ui.setupUi(this);

    // 加载首页图片
    bool main_image_exists = QFile::exists(":/image/assets/normal_image/main_image.png");
    if (main_image_exists) {
        LOG_MODULE("DGLABClient", "DGLABClient", LOG_DEBUG, "首页图片资源存在");
        QPixmap main_pixmap(":/image/assets/normal_image/main_image.png");
        ui.main_image_label->setScaledContents(true);

        if (!main_pixmap.isNull()) {
            ui.main_image_label->setPixmap(main_pixmap);
            LOG_MODULE("DGLABClient", "DGLABClient", LOG_DEBUG, "首页图片加载成功");
        }
        else {
            ui.main_image_label->setText("加载失败！");
            LOG_MODULE("DGLABClient", "DGLABClient", LOG_ERROR, "首页图片加载失败！");
        }
    }
    else {
        ui.main_image_label->setText("加载失败！");
        LOG_MODULE("DGLABClient", "DGLABClient", LOG_ERROR, "首页图片资源不存在！");
    }

    // 设置元素属性

    // 加载样式表
    bool stylesheet_exists = QFile::exists(":/qcss/qcss/style.qcss");
    if (stylesheet_exists) {
        LOG_MODULE("DGLABClient", "DGLABClient", LOG_DEBUG, "样式表存在");
        QFile stylesheet_file(":/qcss/qcss/style.qcss");
        if (stylesheet_file.open(QFile::ReadOnly)) {
            QString styleSheet = QLatin1String(stylesheet_file.readAll());
            qApp->setStyleSheet(styleSheet);
            stylesheet_file.close();
            LOG_MODULE("DGLABClient", "DGLABClient", LOG_DEBUG, "样式表加载成功");
        }
        else {
            LOG_MODULE("DGLABClient", "DGLABClient", LOG_ERROR, "样式表打开失败！");
        }
    }
    else {
        LOG_MODULE("DGLABClient", "DGLABClient", LOG_ERROR, "样式表不存在！");
    }

    // 绑定信号与槽
    connect(ui.main_first_btn, &QPushButton::clicked, this, &DGLABClient::on_main_first_btn_clicked);
    connect(ui.main_config_btn, &QPushButton::clicked, this, &DGLABClient::on_main_config_btn_clicked);
    connect(ui.main_setting_btn, &QPushButton::clicked, this, &DGLABClient::on_main_setting_btn_clicked);
    connect(ui.main_about_btn, &QPushButton::clicked, this, &DGLABClient::on_main_about_btn_clicked);
}

DGLABClient::~DGLABClient() {
}

void DGLABClient::on_main_first_btn_clicked() {
    ui.stackedWidget->setCurrentWidget(ui.first_page);
}

void DGLABClient::on_main_config_btn_clicked() {
    ui.stackedWidget->setCurrentWidget(ui.config_page);
}

void DGLABClient::on_main_setting_btn_clicked() {
    ui.stackedWidget->setCurrentWidget(ui.setting_page);
}

void DGLABClient::on_main_about_btn_clicked() {
    ui.stackedWidget->setCurrentWidget(ui.about_page);
}
