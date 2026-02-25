#include "DGLABClient.h"
#include "DebugLog.h"

#include <iostream>
#include <QPixmap>
#include <QFile>

DGLABClient::DGLABClient(QWidget* parent)
    : QWidget(parent) {
    ui.setupUi(this);

    // 加载首页图片
    QString image_path = ":/image/assets/normal_image/main_image.png";
    bool main_image_exists = QFile::exists(image_path);
    if (main_image_exists) {
        ui.main_image_label->setScaledContents(true);
        ui.main_image_label->setStyleSheet("QLabel{border-image: url(" + image_path + ") 0 0 0 0 stretch stretch;}");
        ui.main_image_label->setText("");
        LOG_MODULE("DGLABClient", "DGLABClient", LOG_DEBUG, "首页图片加载成功");
    }
    else {
        ui.main_image_label->setText("加载失败！");
        LOG_MODULE("DGLABClient", "DGLABClient", LOG_ERROR, "首页图片资源不存在！");
    }

    // 设置元素属性
    ui.all->setProperty("type", "main_page");
    ui.all->setProperty("mode", "light");
    QList<QPushButton*> btns = ui.left_btns_bar->findChildren<QPushButton*>();
    for (QPushButton* btn : btns) {
        btn->setProperty("type", "main_page_btns");
        btn->setProperty("mode", "light");
    }
    LOG_MODULE("DGLABClient", "DGLABClient", LOG_DEBUG, "共加载 " << btns.size() << " 个按键");
    ui.main_image_label->setProperty("type", "main_image_label");
    ui.main_image_label->setProperty("mode", "light");
    LOG_MODULE("DGLABClient", "DGLABClient", LOG_DEBUG, "设置元素属性完成！当前全局 mode 为：light");

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
