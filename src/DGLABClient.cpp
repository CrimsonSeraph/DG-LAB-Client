/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "DGLABClient.h"

#include "AppConfig.h"
#include "ComboBoxDelegate.h"
#include "DGLABClient_utils.hpp"
#include "DebugLog.h"
#include "FormulaBuilderDialog.h"
#include "PythonSubprocessManager.h"
#include "RuleManager.h"
#include "ValueModeDelegate.h"

#include <QAbstractSocket>
#include <QAction>
#include <QApplication>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QDebug>
#include <QDialog>
#include <QFile>
#include <QFont>
#include <QHash>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHostAddress>
#include <QIcon>
#include <QInputDialog>
#include <QIntValidator>
#include <QLabel>
#include <QLatin1String>
#include <QLayout>
#include <QLineEdit>
#include <QList>
#include <QLocale>
#include <QMessageBox>
#include <QNetworkInterface>
#include <QPalette>
#include <QPixmap>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QVBoxLayout>

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <utility>

 // ============================================
 // 构造/析构（public）
 // ============================================

DGLABClient::DGLABClient(QWidget* parent)
    : QWidget(parent) {
    LOG_MODULE("DGLABClient", "DGLABClient", LOG_DEBUG, "开始初始化窗口");
    ui_.setupUi(this);

    change_ui_log_level();
    setup_debug_log();
    register_log_sink();
    create_log_highlighter();
    load_main_image();
    create_tray_icon();
    setup_log_widget_style();
    setup_connections();
    setup_port_input_validation();
    setup_default_page();
    setup_rules_ui();
    init_python_manager();
    apply_widget_properties();
    load_stylesheet();
    init_port_input_placeholder();
    init_channel_info();
    setup_channel_value_input_validation();

    LOG_MODULE("DGLABClient", "DGLABClient", LOG_INFO, "窗口初始化完成");
}

DGLABClient::~DGLABClient() {
    delete_old_qr_file();
    DebugLog::instance().unregister_log_sink("qt_ui");
}

// ============================================
// 公共接口（public）
// ============================================

void DGLABClient::on_main_first_btn_clicked() {
    LOG_MODULE("DGLABClient", "on_main_first_btn_clicked", LOG_DEBUG, "main_first_btn 按键触发，跳转 first_page");
    ui_.stackedWidget->setCurrentWidget(ui_.first_page);
}

void DGLABClient::on_main_config_btn_clicked() {
    LOG_MODULE("DGLABClient", "on_main_config_btn_clicked", LOG_DEBUG, "main_config_btn 按键触发，跳转 config_page");
    ui_.stackedWidget->setCurrentWidget(ui_.config_page);
}

void DGLABClient::on_main_setting_btn_clicked() {
    LOG_MODULE("DGLABClient", "on_main_setting_btn_clicked", LOG_DEBUG, "main_setting_btn 按键触发，跳转 setting_page");
    ui_.stackedWidget->setCurrentWidget(ui_.setting_page);
}

void DGLABClient::on_main_about_btn_clicked() {
    LOG_MODULE("DGLABClient", "on_main_about_btn_clicked", LOG_DEBUG, "main_about_btn 按键触发，跳转 about_page");
    ui_.stackedWidget->setCurrentWidget(ui_.about_page);
}

void DGLABClient::on_start_connect_btn_clicked() {
    LOG_MODULE("DGLABClient", "on_start_connect_btn_clicked", LOG_DEBUG, "start_connect_btn 按键触发");
    if (start_connect_btn_loading_) {
        LOG_MODULE("DGLABClient", "on_start_connect_btn_clicked", LOG_DEBUG, "正在连接中，忽略重复点击");
        return;
    }
    if (!is_connected_) {
        LOG_MODULE("DGLABClient", "on_start_connect_btn_clicked", LOG_INFO, "开始连接");
        start_connect_btn_loading_ = true;
        ui_.start_connect_btn->setEnabled(false);
        start_async_connect();
    }
    else {
        LOG_MODULE("DGLABClient", "on_start_connect_btn_clicked", LOG_INFO, "已连接");
    }
}

void DGLABClient::on_close_connect_btn_clicked() {
    LOG_MODULE("DGLABClient", "on_close_connect_btn_clicked", LOG_DEBUG, "close_connect_btn 按键触发");
    if (close_connect_btn_loading_) {
        LOG_MODULE("DGLABClient", "on_close_connect_btn_clicked", LOG_DEBUG, "正在断开中，忽略重复点击");
        return;
    }
    if (is_connected_) {
        LOG_MODULE("DGLABClient", "on_close_connect_btn_clicked", LOG_INFO, "开始断开连接");
        close_connect_btn_loading_ = true;
        ui_.close_connect_btn->setEnabled(false);
        close_async_connect();
    }
    else {
        LOG_MODULE("DGLABClient", "on_close_connect_btn_clicked", LOG_INFO, "没有连接");
    }
}

void DGLABClient::on_start_btn_clicked() {
    QJsonObject test_cmd;
    test_cmd["cmd"] = "send_strength";
    test_cmd["channel"] = 1;
    test_cmd["mode"] = 1;
    test_cmd["value"] = 10;
    async_call(test_cmd, 5000, [this](bool ok, QString msg) {
        if (ok) {
            LOG_MODULE("DGLABClient", "on_start_btn_clicked", LOG_INFO, "测试命令发送成功: " << msg.toStdString());
        }
        else {
            LOG_MODULE("DGLABClient", "on_start_btn_clicked", LOG_ERROR, "测试命令发送失败: " << msg.toStdString());
        }
        });
}

void DGLABClient::on_close_btn_clicked() {
    // 预留实现
}

void DGLABClient::on_show_qr_btn_clicked() {
    show_qr_dialog();
}

void DGLABClient::change_ui_log_level() {
    auto& config = AppConfig::instance();
    int new_level = config.get_value<int>("app.log.ui_log_level", 0);
    LOG_MODULE("DGLABClient", "change_ui_log_level", LOG_INFO, "修改 UI 日志级别: 旧=" << ui_log_level_ << " 新=" << new_level);
    ui_log_level_ = DebugLog::int_to_log_level(new_level);
    DebugLog::instance().set_log_sink_level("qt_ui", ui_log_level_);
}

void DGLABClient::set_port() {
    QString input = ui_.port_input->text().trimmed();
    if (input.isEmpty()) {
        QMessageBox::warning(this, "端口设置失败！", "端口号不能为空！");
        LOG_MODULE("DGLABClient", "set_port", LOG_WARN, "设置端口失败！端口号为空");
    }
    else {
        bool ok;
        int port = input.toInt(&ok);
        if (ok && port >= 0 && port <= 65535) {
            LOG_MODULE("DGLABClient", "set_port", LOG_DEBUG, "开始设置端口");
            auto& config = AppConfig::instance();
            config.set_value_with_name<int>("app.websocket.port", port, "system");
            QMessageBox::information(this, "端口更新完成！", "设置端口完成：" + input);
            LOG_MODULE("DGLABClient", "set_port", LOG_INFO, "设置端口完成：" << input.toStdString());
        }
        else {
            QMessageBox::warning(this, "端口设置失败！", "设置端口失败！非合法端口：" + input);
            LOG_MODULE("DGLABClient", "set_port", LOG_WARN, "设置端口失败！非合法端口：" << input.toStdString());
        }
    }
    ui_.port_input->setText("");
    auto& config = AppConfig::instance();
    int old_port = config.get_value<int>("app.websocket.port", 9999);
    ui_.port_input->setPlaceholderText("请输入 WebSocket 端口号，当前端口号：" + QString::number(old_port));
}

// ============================================
// 重写虚函数（protected）
// ============================================

void DGLABClient::closeEvent(QCloseEvent* event) {
    if (tray_icon_->isVisible()) {
        tray_icon_->showMessage("提示", "程序已最小化到系统托盘",
            QSystemTrayIcon::Information, 2000);
        this->hide();
        event->ignore();
    }
    else {
        event->accept();
    }
}

// ============================================
// 私有辅助函数（private）
// ============================================

// ----- 初始化相关 -----
void DGLABClient::setup_debug_log() {
    ui_.debug_log->setReadOnly(true);
    ui_.debug_log->setStyleSheet("");
    ui_.debug_log->document()->setDefaultStyleSheet("");
    ui_.debug_log->document()->setDefaultFont(QFont("Consolas", 8));
    ui_.debug_log->setAcceptRichText(true);
}

void DGLABClient::register_log_sink() {
    LOG_MODULE("DGLABClient", "register_log_sink", LOG_DEBUG, "开始注册 Qt Sink");

    const int TAG1_WIDTH = 24;
    const int TAG2_WIDTH = 32;
    const int TAG3_WIDTH = 10;
    auto pad_right = [](const QString& s, int width) -> QString {
        if (s.length() >= width) return s.left(width);
        return s + QString(width - s.length(), ' ');
        };

    qt_sink_.callback = [qptr = QPointer<DGLABClient>(this), this,
        TAG1_WIDTH, TAG2_WIDTH, TAG3_WIDTH, pad_right](
            const std::string& module,
            const std::string& method,
            LogLevel level,
            const std::string& message) {
                if (!qptr) return;
                QString tag1 = "[" + QString::fromStdString(module) + "]";
                QString tag2 = "<" + QString::fromStdString(method) + ">";
                QString tag3 = "(" + QString::fromStdString(DebugLog::instance().level_to_string(level)) + ")";
                QString msg = QString::fromStdString(message);

                QString display;
                if (use_fixed_width_log_) {
                    display = pad_right(tag1, TAG1_WIDTH) + " "
                        + pad_right(tag2, TAG2_WIDTH) + " "
                        + pad_right(tag3, TAG3_WIDTH) + ": "
                        + msg;
                }
                else {
                    display = tag1 + " " + tag2 + " " + tag3 + ": " + msg;
                }

                QMetaObject::invokeMethod(qptr.data(), [qptr, display, level]() {
                    if (!qptr) return;
                    qptr->append_log_message(display, level);
                    }, Qt::AutoConnection);
        };
    qt_sink_.min_level = ui_log_level_;
    DebugLog::instance().unregister_log_sink("qt_ui");
    DebugLog::instance().register_log_sink("qt_ui", qt_sink_);
    LOG_MODULE("DGLABClient", "register_log_sink", LOG_DEBUG, "注册 Qt Sink 完成");
}

void DGLABClient::create_log_highlighter() {
    LOG_MODULE("DGLABClient", "create_log_highlighter", LOG_DEBUG, "创建简单的高亮器");
    class LogHighlighter : public QSyntaxHighlighter {
    public:
        LogHighlighter(QTextDocument* doc) : QSyntaxHighlighter(doc) {}
    protected:
        void highlightBlock(const QString& text) override {
            QTextCharFormat f;
            if (text.contains("(ERROR)")) {
                f.setForeground(Qt::red);
            }
            else if (text.contains("(WARN)")) {
                f.setForeground(Qt::yellow);
            }
            else if (text.contains("(INFO)")) {
                f.setForeground(Qt::green);
            }
            else if (text.contains("(DEBUG)")) {
                f.setForeground(Qt::gray);
            }
            else {
                return;
            }
            setFormat(0, text.length(), f);
        }
    };
    log_highlighter_ = new LogHighlighter(ui_.debug_log->document());
}

void DGLABClient::load_main_image() {
    LOG_MODULE("DGLABClient", "load_main_image", LOG_DEBUG, "开始加载首页图片");
    QString image_path = ":/image/assets/normal_image/main_image.png";
    bool main_image_exists = QFile::exists(image_path);
    if (main_image_exists) {
        ui_.main_image_label->setScaledContents(true);
        ui_.main_image_label->setStyleSheet("QLabel{border-image: url(" + image_path + ") 0 0 0 0 stretch stretch;}");
        ui_.main_image_label->setText("");
        LOG_MODULE("DGLABClient", "load_main_image", LOG_DEBUG, "首页图片加载成功");
    }
    else {
        ui_.main_image_label->setText("加载失败！");
        LOG_MODULE("DGLABClient", "load_main_image", LOG_ERROR, "首页图片资源不存在！");
    }
}

void DGLABClient::create_tray_icon() {
    LOG_MODULE("DGLABClient", "create_tray_icon", LOG_DEBUG, "开始创建托盘图标");
    QString tray_icon_path = ":/image/assets/normal_image/main_image.png";
    bool tray_icon_exists = QFile::exists(tray_icon_path);
    if (tray_icon_exists) {
        tray_icon_ = new QSystemTrayIcon(this);
        tray_icon_->setIcon(QIcon(tray_icon_path));
        auto& config = AppConfig::instance();
        std::string app_name = config.get_value<std::string>("app.name", "DG-LAB-Client");
        tray_icon_->setToolTip(QString::fromStdString(app_name));

        tray_menu_ = new QMenu(this);
        QAction* show_action = new QAction("显示", this);
        QAction* quit_action = new QAction("退出", this);

        connect(show_action, &QAction::triggered, this, [this]() {
            this->showNormal();
            this->activateWindow();
            });
        connect(quit_action, &QAction::triggered, qApp, &QApplication::quit);
        tray_menu_->addAction(show_action);
        tray_menu_->addSeparator();
        tray_menu_->addAction(quit_action);
        tray_icon_->setContextMenu(tray_menu_);
        connect(tray_icon_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::DoubleClick) {
                this->showNormal();
                this->activateWindow();
            }
            });
        tray_icon_->show();
        LOG_MODULE("DGLABClient", "create_tray_icon", LOG_DEBUG, "托盘图标加载完成");
    }
    else {
        ui_.main_image_label->setText("加载失败！");
        LOG_MODULE("DGLABClient", "create_tray_icon", LOG_ERROR, "托盘图标不存在！");
    }
}

void DGLABClient::load_stylesheet() {
    LOG_MODULE("DGLABClient", "load_stylesheet", LOG_DEBUG, "开始加载样式表");
    ui_.current_theme->setText("加载中...");
    auto& config = AppConfig::instance();
    theme_ = mode_string_to_theme(config.get_value<std::string>("app.ui.theme", "light"));
    setup_widget_properties("theme", theme_to_mode_string(theme_).toStdString());
    LOG_MODULE("DGLABClient", "load_stylesheet", LOG_INFO, "当前样式：" + theme_to_mode_string(theme_).toStdString());

    QString qss_path = QStringLiteral(":/style/qcss/") + theme_to_mode_string(theme_) + QStringLiteral(".qcss");
    if (!QFile::exists(qss_path)) {
        qss_path = QStringLiteral(":/style/qcss/light.qcss");
    }

    QFile file(qss_path);
    if (file.open(QFile::ReadOnly)) {
        QString style = QString::fromUtf8(file.readAll());
        this->setProperty("theme", theme_to_mode_string(theme_));
        this->setStyleSheet(style);
        file.close();
        ui_.current_theme->setText(theme_to_mode_string_cn(theme_));
    }
    else {
        LOG_MODULE("DGLABClient", "load_stylesheet", LOG_WARN,
            "无法加载样式表文件：" << qss_path.toStdString());
    }
    apply_inline_styles();
}

void DGLABClient::change_theme(const std::string& theme_str) {
    Theme new_theme = mode_string_to_theme(theme_str);
    if (new_theme == theme_) {
        LOG_MODULE("DGLABClient", "change_theme", LOG_DEBUG,
            "主题未改变，当前已是：" << theme_str);
        return;
    }

    theme_ = new_theme;
    LOG_MODULE("DGLABClient", "change_theme", LOG_INFO,
        "切换主题为：" << theme_str << " (枚举值: " << static_cast<int>(theme_) << ")");

    auto& config = AppConfig::instance();
    config.set_value<std::string>("app.ui.theme", theme_to_mode_string(theme_).toStdString());

    load_stylesheet();

    ui_.current_theme->setText(theme_to_mode_string_cn(theme_));
}

void DGLABClient::change_theme(const QString& theme_str) {
    change_theme(theme_str.toStdString());
}

void DGLABClient::setup_log_widget_style() {
    LOG_MODULE("DGLABClient", "setup_log_widget_style", LOG_DEBUG, "设置硬编码样式");
    ui_.debug_log->setStyleSheet("QTextEdit#debug_log { color: black; }");
    QPalette pal = ui_.debug_log->palette();
    pal.setColor(QPalette::Text, Qt::black);
    pal.setColor(QPalette::WindowText, Qt::black);
    ui_.debug_log->setPalette(pal);
}

void DGLABClient::setup_connections() {
    LOG_MODULE("DGLABClient", "setup_connections", LOG_DEBUG, "开始绑定信号与槽");
    connect(ui_.main_first_btn, &QPushButton::clicked, this, &DGLABClient::on_main_first_btn_clicked);
    connect(ui_.main_config_btn, &QPushButton::clicked, this, &DGLABClient::on_main_config_btn_clicked);
    connect(ui_.main_setting_btn, &QPushButton::clicked, this, &DGLABClient::on_main_setting_btn_clicked);
    connect(ui_.main_about_btn, &QPushButton::clicked, this, &DGLABClient::on_main_about_btn_clicked);

    connect(ui_.start_connect_btn, &QPushButton::clicked, this, &DGLABClient::on_start_connect_btn_clicked);
    connect(ui_.close_connect_btn, &QPushButton::clicked, this, &DGLABClient::on_close_connect_btn_clicked);
    connect(ui_.start_btn, &QPushButton::clicked, this, &DGLABClient::on_start_btn_clicked);
    connect(ui_.close_btn, &QPushButton::clicked, this, &DGLABClient::on_close_btn_clicked);

    connect(ui_.port_confirm_btn, &QPushButton::clicked, this, &DGLABClient::set_port);
    connect(ui_.show_qr_btn, &QPushButton::clicked, this, &DGLABClient::on_show_qr_btn_clicked);

    connect(ui_.change_theme_btn, &QPushButton::clicked, this, &DGLABClient::show_theme_selector);

    connect(this, &DGLABClient::connect_finished, this, &DGLABClient::on_connect_finished);
    connect(this, &DGLABClient::close_finished, this, &DGLABClient::on_close_finished);

    connect(ui_.A_channel_value, &QLineEdit::editingFinished, this, &DGLABClient::apply_A_strength_from_input);
    connect(ui_.B_channel_value, &QLineEdit::editingFinished, this, &DGLABClient::apply_B_strength_from_input);
    connect(ui_.A_lock_btn, &QPushButton::clicked, this, &DGLABClient::change_A_lock);
    connect(ui_.B_lock_btn, &QPushButton::clicked, this, &DGLABClient::change_B_lock);
    connect(ui_.confirm_A_strength, &QPushButton::clicked, this, &DGLABClient::apply_A_strength_from_input);
    connect(ui_.confirm_B_strength, &QPushButton::clicked, this, &DGLABClient::apply_B_strength_from_input);
}

void DGLABClient::setup_port_input_validation() {
    QIntValidator* validator = new QIntValidator(0, 65535, this);
    QLocale locale = QLocale::c();
    validator->setLocale(locale);
    ui_.port_input->setValidator(validator);
    connect(ui_.port_input, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (text.length() > 5) {
            ui_.port_input->setText(text.left(5));
        }
        });
}

void DGLABClient::setup_default_page() {
    ui_.stackedWidget->setCurrentWidget(ui_.first_page);
}

void DGLABClient::init_python_manager() {
    LOG_MODULE("DGLABClient", "init_python_manager", LOG_INFO, "启动并连接 Python 服务");
    py_manager_ = new PythonSubprocessManager(this);

    connect(py_manager_, &PythonSubprocessManager::started,
        this, [this](bool success, const QString& error) {
            if (!success) {
                emit connect_finished(false, "Python 进程启动错误: " + error);
                return;
            }
        });
    connect(py_manager_, &PythonSubprocessManager::finished,
        this, [this]() {
            emit close_finished(true, "Python 进程关闭");
        });

    connect(py_manager_, &PythonSubprocessManager::active_message_received,
        this, &DGLABClient::on_active_message_received);

    auto& config = AppConfig::instance();
    QString pythonPath = QString::fromStdString(config.get_value<std::string>("python.path", "python"));
    std::string bridge_module = config.get_value<std::string>("python.bridge_path", "./python/Bridge.py");
    LOG_MODULE("DGLABClient", "init_python_manager", LOG_INFO, "启动 Python 进程 -> [Python 解释器]路径："
        << pythonPath.toStdString() << "（注：若解释器路径直接为<Python>则使用系统默认 Python 路径）");
    LOG_MODULE("DGLABClient", "init_python_manager", LOG_INFO, "启动 Python 进程 -> [Python 服务模块]路径：" << bridge_module);
    if (bridge_module.starts_with(".")) bridge_module = bridge_module.substr(1);
    QString script_path = QCoreApplication::applicationDirPath() + QString::fromStdString(bridge_module);
    py_manager_->start_process(pythonPath, script_path);
}

void DGLABClient::reset_py_log_level() {
    auto& config = AppConfig::instance();
    QString level = QString::fromStdString(config.get_value<std::string>("app.log_level", "DEBUG"));
    QJsonObject cmd;
    cmd["cmd"] = "set_log_level";
    cmd["level"] = level;
    async_call(cmd, 2000, [this](bool ok, QString msg) {
        if (ok) {
            LOG_MODULE("DGLABClient", "reset_py_log_level", LOG_INFO, "已将 Python 端日志级别设置为: " << msg.toStdString());
        }
        else {
            LOG_MODULE("DGLABClient", "reset_py_log_level", LOG_ERROR, "设置 Python 端日志级别失败: " << msg.toStdString());
        }
        });
}

void DGLABClient::init_port_input_placeholder() {
    auto& config = AppConfig::instance();
    int old_port = config.get_value<int>("app.websocket.port", 9999);
    ui_.port_input->setPlaceholderText("请输入 WebSocket 端口号，当前端口号：" + QString::number(old_port));
}

void DGLABClient::init_channel_info() {
    is_A_info_locked_ = false;
    is_B_info_locked_ = false;
    ui_.A_channel_value->setReadOnly(false);
    ui_.B_channel_value->setReadOnly(false);
    ui_.A_lock_btn->setText("禁用\n更改");
    ui_.B_lock_btn->setText("禁用\n更改");
    refresh_A_display();
    refresh_B_display();
}

// ----- 二维码相关 -----
void DGLABClient::fetch_qr_path() {
    LOG_MODULE("DGLABClient", "fetch_qr_path", LOG_INFO, "开始获取二维码路径");
    delete_old_qr_file();

    QJsonObject cmd;
    cmd["cmd"] = "get_qr_path";
    async_call(cmd, 5000, [this](bool ok, QString msg) {
        if (ok && !msg.isEmpty()) {
            current_qr_path_ = msg;
            LOG_MODULE("DGLABClient", "fetch_qr_path", LOG_INFO,
                "成功获取二维码路径: " << current_qr_path_.toStdString());
            show_qr_dialog();
        }
        else {
            LOG_MODULE("DGLABClient", "fetch_qr_path", LOG_ERROR,
                "获取二维码路径失败: " << msg.toStdString());
            QMessageBox::warning(this, "获取二维码失败",
                "无法生成二维码，请检查连接状态。");
        }
        });
}

void DGLABClient::show_qr_dialog() {
    LOG_MODULE("DGLABClient", "show_qr_dialog", LOG_INFO, "准备显示二维码对话框");

    if (current_qr_path_.isEmpty() || !QFile::exists(current_qr_path_)) {
        LOG_MODULE("DGLABClient", "show_qr_dialog", LOG_WARN,
            "二维码路径无效或文件不存在: " << current_qr_path_.toStdString());
        QDialog* dialog = new QDialog(this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowTitle("DGLab 扫码连接");
        QVBoxLayout* layout = new QVBoxLayout(dialog);
        QLabel* tipLabel = new QLabel("二维码无法显示，请检查连接状态。");
        tipLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(tipLabel);
        QPushButton* closeBtn = new QPushButton("关闭");
        layout->addWidget(closeBtn);
        connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::accept);
        dialog->exec();
        return;
    }

    QPixmap pixmap(current_qr_path_);
    if (pixmap.isNull()) {
        LOG_MODULE("DGLABClient", "show_qr_dialog", LOG_ERROR,
            "无法加载二维码图片: " << current_qr_path_.toStdString());
        QMessageBox::warning(this, "错误", "二维码图片文件损坏或无法读取");
        return;
    }

    LOG_MODULE("DGLABClient", "show_qr_dialog", LOG_INFO,
        "成功加载二维码图片: " << current_qr_path_.toStdString());

    QDialog* dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle("DGLab 扫码连接");
    QVBoxLayout* layout = new QVBoxLayout(dialog);

    QLabel* imageLabel = new QLabel();
    imageLabel->setPixmap(pixmap.scaled(400, 400, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    imageLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(imageLabel);

    QLabel* tipLabel = new QLabel("请使用 DGLab App 扫描上方二维码\n关闭后仍可以在\"配置\"界面点击\"显示二维码\"重新显示");
    tipLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(tipLabel);

    QPushButton* closeBtn = new QPushButton("关闭");
    layout->addWidget(closeBtn);
    connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::accept);

    dialog->exec();
}

void DGLABClient::delete_old_qr_file() {
    if (!current_qr_path_.isEmpty() && QFile::exists(current_qr_path_)) {
        if (QFile::remove(current_qr_path_)) {
            LOG_MODULE("DGLABClient", "delete_old_qr_file", LOG_DEBUG,
                "已删除旧二维码文件: " << current_qr_path_.toStdString());
        }
        else {
            LOG_MODULE("DGLABClient", "delete_old_qr_file", LOG_WARN,
                "删除旧二维码文件失败: " << current_qr_path_.toStdString());
        }
        current_qr_path_.clear();
    }
}

// ----- 网络相关 -----
QString DGLABClient::get_local_lan_ip() {
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    QString fallbackIp;

    for (const QNetworkInterface& iface : interfaces) {
        if (!(iface.flags() & QNetworkInterface::IsUp) ||
            (iface.flags() & QNetworkInterface::IsLoopBack)) {
            continue;
        }

        QString ifaceName = iface.name();
        QString ifaceHuman = iface.humanReadableName();

        if (DGLABClientUtil::contains_any_keyword(ifaceName, default_blacklist_)
            || DGLABClientUtil::contains_any_keyword(ifaceHuman, default_blacklist_)) {
            qDebug() << "黑名单过滤网卡:" << ifaceHuman;
            continue;
        }

        QString ipv4;
        for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
            QHostAddress ip = entry.ip();
            if (ip.protocol() == QAbstractSocket::IPv4Protocol && ip != QHostAddress::LocalHost) {
                ipv4 = ip.toString();
                break;
            }
        }
        if (ipv4.isEmpty()) continue;

        if (fallbackIp.isEmpty()) {
            fallbackIp = ipv4;
        }

        if (DGLABClientUtil::contains_any_keyword(ifaceName, default_whitelist_)
            || DGLABClientUtil::contains_any_keyword(ifaceHuman, default_whitelist_)) {
            qDebug() << "白名单匹配网卡:" << ifaceHuman << " IP:" << ipv4;
            return ipv4;
        }
    }

    if (!fallbackIp.isEmpty()) {
        return fallbackIp;
    }
    return "127.0.0.1";
}

// ----- 规则 UI 相关 -----
void DGLABClient::setup_rules_ui() {
    QLayout* oldLayout = ui_.config_widgrt->layout();
    if (oldLayout) delete oldLayout;
    QVBoxLayout* layout = new QVBoxLayout(ui_.config_widgrt);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    // 文件选择区域
    QHBoxLayout* fileLayout = new QHBoxLayout();
    fileLayout->addWidget(new QLabel("规则文件:"));
    rule_file_combo_ = new QComboBox();
    connect(rule_file_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DGLABClient::on_rule_file_changed);
    fileLayout->addWidget(rule_file_combo_);
    create_file_btn_ = new QPushButton("新建");
    delete_file_btn_ = new QPushButton("删除");
    save_file_btn_ = new QPushButton("保存");
    fileLayout->addWidget(create_file_btn_);
    fileLayout->addWidget(delete_file_btn_);
    fileLayout->addWidget(save_file_btn_);
    layout->addLayout(fileLayout);

    // 规则表格
    rule_table_ = new QTableWidget();
    rule_table_->setColumnCount(4);
    rule_table_->setHorizontalHeaderLabels({ "规则名称", "通道", "模式", "值模式" });
    rule_table_->horizontalHeader()->setStretchLastSection(true);

    QStringList channelOptions = { "A", "B", "无" };
    QStringList modeOptions = { "递减", "递增", "设为", "连减", "连增" };

    rule_table_->setItemDelegateForColumn(1, new ComboBoxDelegate(channelOptions, rule_table_));
    rule_table_->setItemDelegateForColumn(2, new ComboBoxDelegate(modeOptions, rule_table_));
    rule_table_->setItemDelegateForColumn(3, new ValueModeDelegate(rule_table_));
    rule_table_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::AnyKeyPressed);
    rule_table_->viewport()->update();

    layout->addWidget(rule_table_);

    // 操作按钮
    QHBoxLayout* btnLayout = new QHBoxLayout();
    add_rule_btn_ = new QPushButton("添加规则");
    edit_rule_btn_ = new QPushButton("编辑规则");
    delete_rule_btn_ = new QPushButton("删除规则");
    btnLayout->addWidget(add_rule_btn_);
    btnLayout->addWidget(edit_rule_btn_);
    btnLayout->addWidget(delete_rule_btn_);
    layout->addLayout(btnLayout);

    // 连接信号
    connect(create_file_btn_, &QPushButton::clicked, this, &DGLABClient::on_create_rule_file);
    connect(delete_file_btn_, &QPushButton::clicked, this, &DGLABClient::on_delete_rule_file);
    connect(save_file_btn_, &QPushButton::clicked, this, &DGLABClient::on_save_rule_file);
    connect(add_rule_btn_, &QPushButton::clicked, this, &DGLABClient::on_add_rule);
    connect(edit_rule_btn_, &QPushButton::clicked, this, &DGLABClient::on_edit_rule);
    connect(delete_rule_btn_, &QPushButton::clicked, this, &DGLABClient::on_delete_rule);

    refresh_rule_file_list();
    update_rule_table();
    LOG_MODULE("DGLABClient", "setup_rules_ui", LOG_INFO, "规则UI初始化完成");
}

void DGLABClient::init_rule_manager() {
    LOG_MODULE("DGLABClient", "init_rule_manager", LOG_DEBUG, "初始化规则类");
}

void DGLABClient::refresh_rule_file_list() {
    auto& rm = RuleManager::instance();
    auto files = rm.get_available_rule_files();
    rule_file_combo_->clear();
    rule_file_combo_->addItem("rules.json");
    for (const auto& file : files) {
        rule_file_combo_->addItem(QString::fromStdString(file));
    }
    QString current = QString::fromStdString(rm.get_current_rule_file());
    int idx = rule_file_combo_->findText(current);
    if (idx >= 0) rule_file_combo_->setCurrentIndex(idx);
}

void DGLABClient::update_rule_table() {
    auto& rm = RuleManager::instance();
    auto names = rm.get_rule_names();
    rule_table_->setRowCount((int)names.size());
    for (size_t i = 0; i < names.size(); ++i) {
        const auto& name = names[i];
        rule_table_->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(name)));

        std::string channel = rm.get_rule_channel(name);
        rule_table_->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(channel.empty() ? "无" : channel)));

        int mode = rm.get_rule_mode(name);
        QString modeStr;
        switch (mode) {
        case 0: modeStr = "递减"; break;
        case 1: modeStr = "递增"; break;
        case 2: modeStr = "设为"; break;
        case 3: modeStr = "连减"; break;
        case 4: modeStr = "连增"; break;
        default: modeStr = "未知";
        }
        rule_table_->setItem(i, 2, new QTableWidgetItem(modeStr));

        std::string pattern = rm.get_rule_value_pattern(name);
        QString displayPattern = QString::fromStdString(pattern);
        displayPattern.replace("{}", "{   }");
        rule_table_->setItem(i, 3, new QTableWidgetItem(displayPattern));
    }
}

// ----- 样式辅助 -----
void DGLABClient::setup_widget_properties(const std::string& property, const std::string& key) {
    LOG_MODULE("DGLABClient", "setup_widget_properties", LOG_DEBUG, "开始设置元素属性");
    LOG_MODULE("DGLABClient", "setup_widget_properties", LOG_DEBUG, "设置元素统一属性[" << property << "]为：" << key);

    apply_widget_properties();

    // 为所有需要动态属性的控件统一设置
    QList<QWidget*> all_widgets = ui_.all->findChildren<QWidget*>();
    all_widgets.append(ui_.all);
    all_widgets.append(this);
    for (QWidget* w : all_widgets) {
        w->setProperty(property.c_str(), key.c_str());
    }

    // 确保按钮的动态属性已设置
    QList<QPushButton*> btns = ui_.all->findChildren<QPushButton*>();
    LOG_MODULE("DGLABClient", "setup_widget_properties", LOG_DEBUG, "共为" << btns.size() << "个按钮设置属性");

    LOG_MODULE("DGLABClient", "setup_widget_properties", LOG_DEBUG, "设置元素属性完成！");
}

void DGLABClient::apply_widget_properties() {
    // 主页面背景
    ui_.all->setProperty("type", "main_page");
    // 毛玻璃背景控件（glassmorphism）
    ui_.left_btns_bar->setProperty("type", "glassmorphism");
    ui_.port_info->setProperty("type", "glassmorphism");    // 端口面板
    ui_.channel_waves->setProperty("type", "glassmorphism");    // 通道面板
    ui_.main_page_btns_bar->setProperty("type", "glassmorphism");   // 首页按钮栏
    ui_.theme->setProperty("type", "glassmorphism");   // 样式切换面板
    // 导航按钮
    ui_.main_first_btn->setProperty("type", "nav_btn");
    ui_.main_config_btn->setProperty("type", "nav_btn");
    ui_.main_setting_btn->setProperty("type", "nav_btn");
    ui_.main_about_btn->setProperty("type", "nav_btn");
    // 操作按钮
    ui_.start_connect_btn->setProperty("type", "action_btn");
    ui_.close_connect_btn->setProperty("type", "action_btn");
    ui_.start_btn->setProperty("type", "action_btn");
    ui_.close_btn->setProperty("type", "action_btn");
    ui_.port_confirm_btn->setProperty("type", "action_btn");
    ui_.show_qr_btn->setProperty("type", "action_btn");
    ui_.change_theme_btn->setProperty("type", "action_btn");
    // 标题标签
    ui_.main_title->setProperty("type", "title");
    ui_.config_title->setProperty("type", "title");
    ui_.setting_title->setProperty("type", "title");
    ui_.about_title->setProperty("type", "title");
    // 普通标签
    ui_.port_label->setProperty("type", "label");
    ui_.theme_label->setProperty("type", "label");
    ui_.current_theme->setProperty("type", "label");
    // 图片标签
    ui_.main_image_label->setProperty("type", "image");
    // 输入框
    ui_.port_input->setProperty("type", "input");
    ui_.A_channel_value->setProperty("type", "input");
    ui_.B_channel_value->setProperty("type", "input");
    // 调试日志框
    ui_.debug_log->setProperty("type", "debug_log");
    // 波形控件
    ui_.wave_show->setProperty("type", "waveform");
    // 滚动区域
    ui_.config_scroll_area->setProperty("type", "scrollarea");
    ui_.setting_scroll_area->setProperty("type", "scrollarea");
    // 堆叠窗口
    ui_.stackedWidget->setProperty("type", "stacked");
    // 子面板（无毛玻璃效果，仅用于布局）
    ui_.about_left_part->setProperty("type", "sub_panel");
    ui_.about_right_part->setProperty("type", "sub_panel");
    ui_.setting_left_part->setProperty("type", "sub_panel");
    ui_.setting_right_part->setProperty("type", "sub_panel");
    ui_.config_widgrt->setProperty("type", "sub_panel");
    ui_.setting_grid->setProperty("type", "sub_panel");
    // 字体设置
    ui_.A_channel->setProperty("type", "font");
    ui_.B_channel->setProperty("type", "font");
}

void DGLABClient::apply_inline_styles() {
    LOG_MODULE("DGLABClient", "apply_inline_styles", LOG_DEBUG, "开始设置内联样式");
    LOG_MODULE("DGLABClient", "apply_inline_styles", LOG_DEBUG, "设置内联字体样式");
    QString font_color;
    static const QHash<Theme, QString> theme_font_colors = {
        {LIGHT, "rgba(0, 0, 0, 255)"},
        {NIGHT, "rgba(255, 255, 255, 255)"},
        {CHARCOAL_PINK, "rgba(255, 255, 255, 255)"},
        {DEEPSEA_CREAM, "rgba(255, 255, 255, 255)"},
        {VINE_PURPLE_TEA_GREEN, "rgba(0, 0, 0, 255)"},
        {OFFWHITE_CAMELLIA, "rgba(0, 0, 0, 255)"},
        {DARK_BLUE_CLEAR_BLUE, "rgba(255, 255, 255, 255)"},
        {KLEIN_YELLOW, "rgba(255, 255, 255, 255)"},
        {MARS_GREEN_ROSE, "rgba(255, 255, 255, 255)"},
        {HERMES_ORANGE_NAVY, "rgba(255, 255, 255, 255)"},
        {TIFFANY_BLUE_CHEESE, "rgba(0, 0, 0, 255)"},
        {CHINA_RED_YELLOW, "rgba(255, 255, 255, 255)"},
        {VANDYKE_BROWN_KHAKI, "rgba(255, 255, 255, 255)"},
        {PRUSSIAN_BLUE_FOG, "rgba(255, 255, 255, 255)"}
    };

    font_color = theme_font_colors.value(theme_, "");
    if (font_color.isEmpty()) {
        font_color = "rgba(0, 0, 0, 255)";
    }

    QList<QWidget*> all_widgets = ui_.all->findChildren<QWidget*>();
    all_widgets.append(ui_.all);
    for (QWidget* w : all_widgets) {
        if (w->property("type").toString() == "font") {
            w->setStyleSheet(QString("color: %1;").arg(font_color));
        }
    }
    LOG_MODULE("DGLABClient", "apply_inline_styles", LOG_DEBUG, "设置内联样式完成！");
}

QString DGLABClient::theme_to_mode_string(Theme theme) {
    static const QHash<Theme, QString> map = {
        {LIGHT, QStringLiteral("light")},
        {NIGHT, QStringLiteral("night")},
        {CHARCOAL_PINK, QStringLiteral("charcoal_pink")},
        {DEEPSEA_CREAM, QStringLiteral("deepsea_cream")},
        {VINE_PURPLE_TEA_GREEN, QStringLiteral("vine_purple_tea_green")},
        {OFFWHITE_CAMELLIA, QStringLiteral("offwhite_camellia")},
        {DARK_BLUE_CLEAR_BLUE, QStringLiteral("dark_blue_clear_blue")},
        {KLEIN_YELLOW, QStringLiteral("klein_yellow")},
        {MARS_GREEN_ROSE, QStringLiteral("mars_green_rose")},
        {HERMES_ORANGE_NAVY, QStringLiteral("hermes_orange_navy")},
        {TIFFANY_BLUE_CHEESE, QStringLiteral("tiffany_blue_cheese")},
        {CHINA_RED_YELLOW, QStringLiteral("china_red_yellow")},
        {VANDYKE_BROWN_KHAKI, QStringLiteral("vandyke_brown_khaki")},
        {PRUSSIAN_BLUE_FOG, QStringLiteral("prussian_blue_fog")}
    };
    return map.value(theme, QStringLiteral("light"));
}

Theme DGLABClient::mode_string_to_theme(const std::string& theme_str) {
    static const QHash<QLatin1String, Theme> map = {
        {QLatin1String("light"), LIGHT},
        {QLatin1String("night"), NIGHT},
        {QLatin1String("charcoal_pink"), CHARCOAL_PINK},
        {QLatin1String("deepsea_cream"), DEEPSEA_CREAM},
        {QLatin1String("vine_purple_tea_green"), VINE_PURPLE_TEA_GREEN},
        {QLatin1String("offwhite_camellia"), OFFWHITE_CAMELLIA},
        {QLatin1String("dark_blue_clear_blue"), DARK_BLUE_CLEAR_BLUE},
        {QLatin1String("klein_yellow"), KLEIN_YELLOW},
        {QLatin1String("mars_green_rose"), MARS_GREEN_ROSE},
        {QLatin1String("hermes_orange_navy"), HERMES_ORANGE_NAVY},
        {QLatin1String("tiffany_blue_cheese"), TIFFANY_BLUE_CHEESE},
        {QLatin1String("china_red_yellow"), CHINA_RED_YELLOW},
        {QLatin1String("vandyke_brown_khaki"), VANDYKE_BROWN_KHAKI},
        {QLatin1String("prussian_blue_fog"), PRUSSIAN_BLUE_FOG}
    };
    return map.value(QLatin1String(theme_str.c_str()), LIGHT);
}

Theme DGLABClient::mode_string_to_theme(const QString& theme_str) {
    return mode_string_to_theme(theme_str.toStdString());
}

QString DGLABClient::theme_to_mode_string_cn(Theme theme) {
    static const QHash<Theme, QString> map = {
        {LIGHT, QStringLiteral("浅色模式")},
        {NIGHT, QStringLiteral("深色模式")},
        {CHARCOAL_PINK, QStringLiteral("炭黑甜粉")},
        {DEEPSEA_CREAM, QStringLiteral("深海奶白")},
        {VINE_PURPLE_TEA_GREEN, QStringLiteral("藤紫钛绿")},
        {OFFWHITE_CAMELLIA, QStringLiteral("无白茶花")},
        {DARK_BLUE_CLEAR_BLUE, QStringLiteral("捣蓝清水")},
        {KLEIN_YELLOW, QStringLiteral("克莱因黄")},
        {MARS_GREEN_ROSE, QStringLiteral("马尔斯玫瑰")},
        {HERMES_ORANGE_NAVY, QStringLiteral("爱马仕深蓝")},
        {TIFFANY_BLUE_CHEESE, QStringLiteral("蒂芙尼奶酪")},
        {CHINA_RED_YELLOW, QStringLiteral("中国红黄")},
        {VANDYKE_BROWN_KHAKI, QStringLiteral("凡戴克棕卡其")},
        {PRUSSIAN_BLUE_FOG, QStringLiteral("普鲁士雾灰")}
    };
    return map.value(theme, QStringLiteral("浅色模式"));
}

Theme DGLABClient::mode_string_to_theme_cn(const std::string& theme_str) {
    static const QHash<QString, Theme> map = {
        {QStringLiteral("浅色模式"), LIGHT},
        {QStringLiteral("深色模式"), NIGHT},
        {QStringLiteral("炭黑甜粉"), CHARCOAL_PINK},
        {QStringLiteral("深海奶白"), DEEPSEA_CREAM},
        {QStringLiteral("藤紫钛绿"), VINE_PURPLE_TEA_GREEN},
        {QStringLiteral("无白茶花"), OFFWHITE_CAMELLIA},
        {QStringLiteral("捣蓝清水"), DARK_BLUE_CLEAR_BLUE},
        {QStringLiteral("克莱因黄"), KLEIN_YELLOW},
        {QStringLiteral("马尔斯玫瑰"), MARS_GREEN_ROSE},
        {QStringLiteral("爱马仕深蓝"), HERMES_ORANGE_NAVY},
        {QStringLiteral("蒂芙尼奶酪"), TIFFANY_BLUE_CHEESE},
        {QStringLiteral("中国红黄"), CHINA_RED_YELLOW},
        {QStringLiteral("凡戴克棕卡其"), VANDYKE_BROWN_KHAKI},
        {QStringLiteral("普鲁士雾灰"), PRUSSIAN_BLUE_FOG}
    };
    return map.value(QString::fromStdString(theme_str), LIGHT);
}

Theme DGLABClient::mode_string_to_theme_cn(const QString& theme_str) {
    return mode_string_to_theme_cn(theme_str.toStdString());
}

// ----- 日志辅助 -----
void DGLABClient::append_log_message(const QString& message, int level) {
    QString clean = message;
    QRegularExpression ansi("\\x1B\\[[0-9;]*[A-Za-z]");
    clean.remove(ansi);
    clean.replace('\r', "");
    append_colored_text(ui_.debug_log, clean);
}

void DGLABClient::append_colored_text(QTextEdit* edit, const QString& text) {
    edit->moveCursor(QTextCursor::End);
    edit->insertPlainText(text + "\n");
    edit->moveCursor(QTextCursor::End);
    edit->ensureCursorVisible();
    if (log_highlighter_) {
        log_highlighter_->rehighlight();
    }
}

void DGLABClient::refresh_A_display() {
    if (is_A_info_locked_) {
        ui_.A_channel_value->setText(QString::number(A_strength_));
    }
    else {
        ui_.A_channel_value->setPlaceholderText(QString::number(A_strength_));
        if (!is_connected_) {
            A_strength_ = 0;
            ui_.A_channel_value->setText("");
        }
    }
}

void DGLABClient::refresh_B_display() {
    if (is_B_info_locked_) {
        ui_.B_channel_value->setText(QString::number(B_strength_));
    }
    else {
        ui_.B_channel_value->setPlaceholderText(QString::number(B_strength_));
        if (!is_connected_) {
            B_strength_ = 0;
            ui_.B_channel_value->setText("");
        }
    }
}

void DGLABClient::apply_A_strength_from_input() {
    QString text = ui_.A_channel_value->text();
    bool ok;
    int newVal = text.toInt(&ok);
    if (!ok || newVal < 0 || newVal > 200) {
        refresh_A_display();
        return;
    }
    if (newVal == A_strength_) {
        return;
    }
    A_strength_ = newVal;
    refresh_A_display();

    if (is_connected_) {
        QJsonObject cmd;
        cmd["cmd"] = "set_strength";
        cmd["channel"] = "A";
        cmd["mode"] = 2;
        cmd["value"] = A_strength_;
        async_call(cmd, 5000, [this](bool ok, QString msg) {
            if (ok) {
                LOG_MODULE("DGLABClient", "apply_A_strength_from_input", LOG_INFO,
                    "A通道强度设置成功: " << msg.toStdString());
            }
            else {
                LOG_MODULE("DGLABClient", "apply_A_strength_from_input", LOG_ERROR,
                    "A通道强度设置失败: " << msg.toStdString());
                QMessageBox::warning(this, "错误", "设置A通道强度失败: " + msg);
            }
            });
    }
    else {
        LOG_MODULE("DGLABClient", "apply_A_strength_from_input", LOG_WARN, "未连接，无法设置A通道强度");
        A_strength_ = 0;
        refresh_A_display();
    }
}

void DGLABClient::apply_B_strength_from_input() {
    QString text = ui_.B_channel_value->text();
    bool ok;
    int newVal = text.toInt(&ok);
    if (!ok || newVal < 0 || newVal > 200) {
        refresh_B_display();
        return;
    }
    if (newVal == B_strength_) {
        return;
    }
    B_strength_ = newVal;
    refresh_B_display();
    if (is_connected_) {
        QJsonObject cmd;
        cmd["cmd"] = "set_strength";
        cmd["channel"] = "B";
        cmd["mode"] = 2;
        cmd["value"] = B_strength_;
        async_call(cmd, 5000, [this](bool ok, QString msg) {
            if (ok) {
                LOG_MODULE("DGLABClient", "apply_B_strength_from_input", LOG_INFO,
                    "B通道强度设置成功: " << msg.toStdString());
            }
            else {
                LOG_MODULE("DGLABClient", "apply_B_strength_from_input", LOG_ERROR,
                    "B通道强度设置失败: " << msg.toStdString());
                QMessageBox::warning(this, "错误", "设置B通道强度失败: " + msg);
            }
            });
    }
    else {
        LOG_MODULE("DGLABClient", "apply_B_strength_from_input", LOG_WARN, "未连接，无法设置B通道强度");
        B_strength_ = 0;
        refresh_B_display();
    }
}

// ============================================
// private slots 实现
// ============================================

void DGLABClient::on_connect_finished(bool success, const QString& msg) {
    start_connect_btn_loading_ = false;
    ui_.start_connect_btn->setEnabled(true);
    if (success) {
        is_connected_ = true;
        LOG_MODULE("DGLABClient", "handle_connect_finished", LOG_INFO, msg.toStdString());
        reset_py_log_level();
    }
    else {
        QMessageBox::warning(this, "连接错误！", msg);
        LOG_MODULE("DGLABClient", "handle_connect_finished", LOG_ERROR, msg.toStdString());
    }
}

void DGLABClient::on_close_finished(bool success, const QString& msg) {
    close_connect_btn_loading_ = false;
    ui_.close_connect_btn->setEnabled(true);
    if (success) {
        is_connected_ = false;
        LOG_MODULE("DGLABClient", "handle_close_finished", LOG_INFO, msg.toStdString());
    }
    else {
        LOG_MODULE("DGLABClient", "handle_close_finished", LOG_ERROR, msg.toStdString());
    }
}

void DGLABClient::start_async_connect() {
    LOG_MODULE("DGLABClient", "start_async_connect", LOG_INFO, "正在获取本机IP并更新WebSocket地址");
    auto& config = AppConfig::instance();
    int port = config.get_value<int>("app.websocket.port", 9999);
    QString localIp = get_local_lan_ip();
    QString wsUrl = QString("ws://%1:%2").arg(localIp).arg(port);
    LOG_MODULE("DGLABClient", "start_async_connect", LOG_INFO,
        "使用的WebSocket地址: " << wsUrl.toStdString());

    QJsonObject update_url_cmd;
    update_url_cmd["cmd"] = "set_ws_url";
    update_url_cmd["url"] = wsUrl;
    async_call(update_url_cmd, 5000, [this](bool ok, QString msg) {
        if (ok) {
            LOG_MODULE("DGLABClient", "start_async_connect", LOG_INFO, "端口更新成功，继续连接");
            QJsonObject connect_cmd;
            connect_cmd["cmd"] = "connect";
            async_call(connect_cmd, 5000, [this](bool ok, QString msg) {
                emit connect_finished(ok, msg);
                if (ok) {
                    fetch_qr_path();
                }
                });
        }
        else {
            emit connect_finished(false, "端口更新失败: " + msg);
        }
        });
}

void DGLABClient::close_async_connect() {
    LOG_MODULE("DGLABClient", "close_async_connect", LOG_INFO, "正在断开连接");
    QJsonObject close_cmd;
    close_cmd["cmd"] = "close";
    async_call(close_cmd, 5000, [this](bool ok, QString msg) {
        emit close_finished(ok, msg);
        });
}

void DGLABClient::show_theme_selector() {
    LOG_MODULE("DGLABClient", "show_theme_selector", LOG_DEBUG, "打开主题选择对话框");
    ThemeSelectorDialog* dialog = new ThemeSelectorDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &ThemeSelectorDialog::theme_selected,
        this, static_cast<void (DGLABClient::*)(Theme)>(&DGLABClient::change_theme));
    dialog->show();
}

void DGLABClient::change_theme(Theme theme) {
    // 复用已有的字符串版本
    change_theme(theme_to_mode_string(theme).toStdString());
}

void DGLABClient::on_rule_file_changed(int index) {
    if (index < 0) return;
    QString filename = rule_file_combo_->itemText(index);
    try {
        RuleManager::instance().load_rule_file(filename.toStdString());
        update_rule_table();
    }
    catch (const std::exception& e) {
        QMessageBox::warning(this, "错误", QString("加载规则文件失败: ") + e.what());
    }
}

void DGLABClient::on_create_rule_file() {
    bool ok;
    auto& config = AppConfig::instance();
    QString keyword = QString::fromStdString(config.get_value<std::string>("rule.key", "rule"));
    QString name = QInputDialog::getText(this, "新建规则文件",
        "请输入文件名（不含.json，但需要包含关键字：" + keyword + "，否则会自动添加）:",
        QLineEdit::Normal, "", &ok);
    if (!ok || name.isEmpty()) return;
    if (name.endsWith(".json", Qt::CaseInsensitive)) {
        name = name.left(name.length() - 5);
    }
    if (!name.contains(keyword, Qt::CaseSensitive)) {
        name = name + "_" + keyword;
    }
    if (!name.endsWith(".json", Qt::CaseInsensitive)) {
        name += ".json";
    }
    auto& rm = RuleManager::instance();
    auto existing = rm.get_available_rule_files();
    if (std::find(existing.begin(), existing.end(), name.toStdString()) != existing.end()) {
        QMessageBox::warning(this, "错误", "文件已存在");
        return;
    }
    nlohmann::json emptyRules;
    if (rm.create_rule_file(name.toStdString(), emptyRules)) {
        refresh_rule_file_list();
        int idx = rule_file_combo_->findText(name);
        if (idx >= 0) rule_file_combo_->setCurrentIndex(idx);
        QMessageBox::information(this, "提示", "文件创建成功: " + name);
    }
    else {
        QMessageBox::warning(this, "错误", "创建文件失败");
    }
}

void DGLABClient::on_delete_rule_file() {
    QString filename = rule_file_combo_->currentText();
    if (filename == "rules.json") {
        QMessageBox::warning(this, "错误", "不能删除默认规则文件");
        return;
    }
    int ret = QMessageBox::question(this, "确认", "确定要删除规则文件 " + filename + " 吗？");
    if (ret == QMessageBox::Yes) {
        auto& rm = RuleManager::instance();
        if (rm.delete_rule_file(filename.toStdString())) {
            refresh_rule_file_list();
            update_rule_table();
        }
        else {
            QMessageBox::warning(this, "错误", "删除文件失败");
        }
    }
}

void DGLABClient::on_save_rule_file() {
    auto& rm = RuleManager::instance();
    if (rm.save_current_rule_file()) {
        QMessageBox::information(this, "提示", "保存成功");
    }
    else {
        QMessageBox::warning(this, "错误", "保存失败");
    }
}

void DGLABClient::on_add_rule() {
    bool ok;
    QString name = QInputDialog::getText(this, "添加规则", "规则名称:", QLineEdit::Normal, "", &ok);
    if (!ok || name.isEmpty()) return;

    QStringList channels = { "无", "A", "B" };
    QString channel = QInputDialog::getItem(this, "选择通道", "通道:", channels, 0, false, &ok);
    if (!ok) return;
    QString channelStr = (channel == "无") ? "" : channel;

    QStringList modes = { "递减", "递增", "设为", "连减", "连增" };
    QString modeStr = QInputDialog::getItem(this, "选择模式", "模式:", modes, 0, false, &ok);
    if (!ok) return;
    int mode = modes.indexOf(modeStr);

    FormulaBuilderDialog dlg("", this);
    if (dlg.exec() != QDialog::Accepted) return;
    QString valuePattern = dlg.get_formula();
    if (valuePattern.isEmpty()) return;

    auto& rm = RuleManager::instance();
    auto currentFile = rm.get_current_rule_file();
    try {
        nlohmann::json j = rm.load_json_file(currentFile);
        if (!j.contains("rules")) j["rules"] = nlohmann::json::object();
        j["rules"][name.toStdString()] = {
            {"channel", channelStr.toStdString()},
            {"mode", mode},
            {"valuePattern", valuePattern.toStdString()}
        };
        if (rm.modify_rule_file(currentFile, j["rules"])) {
            rm.load_rule_file(currentFile);
            update_rule_table();
            LOG_MODULE("DGLABClient", "on_add_rule", LOG_INFO, "添加规则成功: " << name.toStdString());
        }
        else {
            QMessageBox::warning(this, "错误", "添加规则失败");
        }
    }
    catch (const std::exception& e) {
        QMessageBox::warning(this, "错误", e.what());
    }
}

void DGLABClient::on_edit_rule() {
    int row = rule_table_->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, "提示", "请先选择要编辑的规则");
        return;
    }
    QString name = rule_table_->item(row, 0)->text();
    auto& rm = RuleManager::instance();

    QString oldChannel = QString::fromStdString(rm.get_rule_channel(name.toStdString()));
    if (oldChannel.isEmpty()) oldChannel = "无";
    int oldMode = rm.get_rule_mode(name.toStdString());
    QString oldPattern = QString::fromStdString(rm.get_rule_value_pattern(name.toStdString()));

    bool ok;
    QStringList channels = { "无", "A", "B" };
    QString channel = QInputDialog::getItem(this, "编辑规则", "通道:", channels,
        channels.indexOf(oldChannel), false, &ok);
    if (!ok) return;
    QString channelStr = (channel == "无") ? "" : channel;

    QStringList modes = { "递减", "递增", "设为", "连减", "连增" };
    QString modeStr = QInputDialog::getItem(this, "编辑规则", "模式:", modes, oldMode, false, &ok);
    if (!ok) return;
    int mode = modes.indexOf(modeStr);

    FormulaBuilderDialog dlg(oldPattern, this);
    if (dlg.exec() != QDialog::Accepted) return;
    QString newPattern = dlg.get_formula();
    if (newPattern.isEmpty()) return;

    std::string currentFile = rm.get_current_rule_file();
    try {
        nlohmann::json j = rm.load_json_file(currentFile);
        if (!j.contains("rules")) j["rules"] = nlohmann::json::object();
        j["rules"][name.toStdString()] = {
            {"channel", channelStr.toStdString()},
            {"mode", mode},
            {"valuePattern", newPattern.toStdString()}
        };
        if (rm.modify_rule_file(currentFile, j["rules"])) {
            rm.load_rule_file(currentFile);
            update_rule_table();
            LOG_MODULE("DGLABClient", "on_edit_rule", LOG_INFO, "编辑规则成功: " << name.toStdString());
        }
        else {
            QMessageBox::warning(this, "错误", "编辑规则失败");
        }
    }
    catch (const std::exception& e) {
        QMessageBox::warning(this, "错误", e.what());
    }
}

void DGLABClient::on_delete_rule() {
    int row = rule_table_->currentRow();
    if (row < 0) return;
    QString name = rule_table_->item(row, 0)->text();
    int ret = QMessageBox::question(this, "确认", "确定要删除规则 " + name + " 吗？");
    if (ret == QMessageBox::Yes) {
        auto& rm = RuleManager::instance();
        auto currentFile = rm.get_current_rule_file();
        try {
            nlohmann::json j = rm.load_json_file(currentFile);
            if (j.contains("rules") && j["rules"].contains(name.toStdString())) {
                j["rules"].erase(name.toStdString());
                if (rm.modify_rule_file(currentFile, j["rules"])) {
                    rm.load_rule_file(currentFile);
                    update_rule_table();
                    LOG_MODULE("DGLABClient", "on_delete_rule", LOG_INFO, "删除规则成功: " << name.toStdString());
                }
                else {
                    QMessageBox::warning(this, "错误", "删除规则失败");
                    LOG_MODULE("DGLABClient", "on_delete_rule", LOG_ERROR, "删除规则失败: " << name.toStdString());
                }
            }
        }
        catch (const std::exception& e) {
            QMessageBox::warning(this, "错误", e.what());
        }
    }
}

void DGLABClient::on_active_message_received(const QJsonObject& message) {
    // 消息格式：{ "type": "active_message", "data": {...} }
    QJsonObject data = message.value("data").toObject();
    QString msgType = data.value("type").toString();

    if (msgType == "msg") {
        QString msgContent = data.value("message").toString();
        if (msgContent.startsWith("strength-")) {
            // 格式: strength-A+B+A_limit+B_limit
            QStringList parts = msgContent.mid(9).split('+');
            if (parts.size() >= 4) {
                bool ok1, ok2, ok3, ok4;
                int aStr = parts[0].toInt(&ok1);
                int bStr = parts[1].toInt(&ok2);
                int aLim = parts[2].toInt(&ok3);
                int bLim = parts[3].toInt(&ok4);
                if (ok1 && ok2 && ok3 && ok4) {
                    int A_strength_temp_ = qBound(0, aStr, 200);
                    if (A_strength_temp_ != A_strength_) {
                        A_strength_ = A_strength_temp_;
                        refresh_A_display();
                        LOG_MODULE("DGLABClient", "on_active_message_received", LOG_DEBUG,
                            "A通道强度更新: " << A_strength_ << " (原值: " << A_strength_temp_ << ")");
                    }
                    int B_strength_temp_ = qBound(0, bStr, 200);
                    if (B_strength_temp_ != B_strength_) {
                        B_strength_ = B_strength_temp_;
                        refresh_B_display();
                        LOG_MODULE("DGLABClient", "on_active_message_received", LOG_DEBUG,
                            "B通道强度更新: " << B_strength_ << " (原值: " << B_strength_temp_ << ")");
                    }
                    int A_limit_temp_ = qBound(0, aLim, 200);
                    if (A_limit_temp_ != A_limit_) {
                        A_limit_ = A_limit_temp_;
                        LOG_MODULE("DGLABClient", "on_active_message_received", LOG_DEBUG,
                            "A通道强度上限更新: " << A_limit_ << " (原值: " << A_limit_temp_ << ")");
                    }
                    int B_limit_temp_ = qBound(0, bLim, 200);
                    if (B_limit_temp_ != B_limit_) {
                        B_limit_ = B_limit_temp_;
                        LOG_MODULE("DGLABClient", "on_active_message_received", LOG_DEBUG,
                            "B通道强度上限更新: " << B_limit_ << " (原值: " << B_limit_temp_ << ")");
                    }
                }
                else {
                    LOG_MODULE("DGLABClient", "on_active_message_received", LOG_WARN,
                        "解析强度数值失败: " << msgContent.toStdString());
                }
            }
            else {
                LOG_MODULE("DGLABClient", "on_active_message_received", LOG_WARN,
                    "strength 消息格式错误: " << msgContent.toStdString());
            }
        }
        else {
            LOG_MODULE("DGLABClient", "on_active_message_received", LOG_DEBUG,
                "收到普通 msg: " << msgContent.toStdString());
        }
    }
    else if (msgType == "break") {
        LOG_MODULE("DGLABClient", "on_active_message_received", LOG_INFO,
            "收到断开指令，更新连接状态");
        is_connected_ = false;
    }
    else if (msgType == "error") {
        QString errorMsg = data.value("message").toString();
        LOG_MODULE("DGLABClient", "on_active_message_received", LOG_ERROR,
            "收到服务器错误: " << errorMsg.toStdString());
    }
    else if (msgType == "bind") {
    }
}

void DGLABClient::setup_channel_value_input_validation() {
    QIntValidator* validator = new QIntValidator(0, 200, this);
    QLocale locale = QLocale::c();
    validator->setLocale(locale);
    ui_.A_channel_value->setValidator(validator);
    ui_.B_channel_value->setValidator(validator);
    connect(ui_.A_channel_value, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (text.length() > 3) {
            ui_.A_channel_value->setText(text.left(3));
        }
        });
    connect(ui_.B_channel_value, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (text.length() > 3) {
            ui_.B_channel_value->setText(text.left(3));
        }
        });
}

void DGLABClient::change_A_lock() {
    if (!is_A_info_locked_) {
        apply_A_strength_from_input();
    }
    is_A_info_locked_ = !is_A_info_locked_;
    ui_.A_channel_value->setReadOnly(is_A_info_locked_);
    ui_.A_lock_btn->setText(is_A_info_locked_ ? "启用\n更改" : "禁用\n更改");
    refresh_A_display();
}

void DGLABClient::change_B_lock() {
    if (!is_B_info_locked_) {
        apply_B_strength_from_input();
    }
    is_B_info_locked_ = !is_B_info_locked_;
    ui_.B_channel_value->setReadOnly(is_B_info_locked_);
    ui_.B_lock_btn->setText(is_B_info_locked_ ? "启用\n更改" : "禁用\n更改");
    refresh_B_display();
}
