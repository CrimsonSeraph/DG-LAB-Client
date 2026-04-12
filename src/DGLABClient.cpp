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
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHostAddress>
#include <QIcon>
#include <QInputDialog>
#include <QIntValidator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
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
#include <QThreadPool>
#include <QVBoxLayout>

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <utility>

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
    load_stylesheet();
    setup_log_widget_style();
    setup_connections();
    setup_port_input_validation();
    setup_default_page();
    setup_rules_ui();
    init_python_manager();

    LOG_MODULE("DGLABClient", "DGLABClient", LOG_INFO, "窗口初始化完成");
}

DGLABClient::~DGLABClient() {
    delete_old_qr_file();
    DebugLog::instance().unregister_log_sink("qt_ui");
}

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

template<typename Callback>
void DGLABClient::async_call(const QJsonObject& cmd, int timeout, Callback&& callback) {
    LOG_MODULE("DGLABClient", "async_call", LOG_DEBUG, "在后台线程执行异步调用");
    QThreadPool::globalInstance()->start([this, cmd, timeout, callback = std::forward<Callback>(callback)]() mutable {
        try {
            LOG_MODULE("DGLABClient", "async_call", LOG_INFO, "正在发送命令: " << DebugLogUtil::remove_newline(QJsonDocument(cmd).toJson().toStdString()));
            py_manager_->call(cmd, [callback = std::move(callback)](const QJsonObject& resp) mutable {
                bool ok = resp["status"].toString() == "ok";
                QString msg = resp["message"].toString();
                callback(ok, msg);
                }, timeout);
        }
        catch (const std::runtime_error& e) {
            emit close_finished(false, QString("运行时错误: ") + e.what());
        }
        catch (const std::exception& e) {
            emit close_finished(false, QString("异常: ") + e.what());
        }
        catch (...) {
            emit close_finished(false, "未知异常");
        }
        });
}

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

void DGLABClient::setup_widget_properties(const std::string& property, const std::string& key) {
    LOG_MODULE("DGLABClient", "setup_widget_properties", LOG_DEBUG, "开始设置元素属性");
    LOG_MODULE("DGLABClient", "setup_widget_properties", LOG_INFO, "设置元素统一属性[" << property << "]为：" << key);

    ui_.all->setProperty("type", "main_page");
    ui_.all->setProperty(property.c_str(), key.c_str());

    ui_.left_btns_bar->setProperty("type", "glassmorphism");
    ui_.left_btns_bar->setProperty(property.c_str(), key.c_str());

    QList<QPushButton*> btns = ui_.all->findChildren<QPushButton*>();
    for (QPushButton* btn : btns) {
        btn->setProperty("type", "btns");
        btn->setProperty(property.c_str(), key.c_str());
    }
    LOG_MODULE("DGLABClient", "setup_widget_properties", LOG_DEBUG, "共加载" << btns.size() << "个按键");

    ui_.main_image_label->setProperty("type", "main_image_label");
    ui_.main_image_label->setProperty(property.c_str(), key.c_str());

    ui_.main_page_btns_bar->setProperty("type", "glassmorphism");
    ui_.main_page_btns_bar->setProperty(property.c_str(), key.c_str());

    ui_.debug_log->setProperty("type", "debug_log");
    ui_.debug_log->setProperty(property.c_str(), key.c_str());

    ui_.port_info->setProperty("type", "glassmorphism");
    ui_.port_info->setProperty(property.c_str(), key.c_str());

    auto& config = AppConfig::instance();
    int old_port = config.get_value<int>("app.websocket.port", 9999);
    ui_.port_input->setPlaceholderText("请输入 WebSocket 端口号，当前端口号：" + QString::number(old_port));
    ui_.port_input->setProperty("type", "input");
    ui_.port_input->setProperty(property.c_str(), key.c_str());
    ui_.port_label->setProperty("type", "label");
    ui_.port_label->setProperty(property.c_str(), key.c_str());

    ui_.config_scroll_area->setProperty("type", "none");
    ui_.config_scrollAreaWidgetContents->setProperty("type", "none");
    ui_.config_widgrt->setProperty("type", "glassmorphism");
    ui_.config_widgrt->setProperty(property.c_str(), key.c_str());

    ui_.setting_scroll_area->setProperty("type", "none");
    ui_.setting_scrollAreaWidgetContents->setProperty("type", "none");
    ui_.setting_grid->setProperty("type", "glassmorphism");
    ui_.setting_grid->setProperty(property.c_str(), key.c_str());
    ui_.style_mode->setProperty("type", "glassmorphism");
    ui_.style_mode->setProperty(property.c_str(), key.c_str());
    ui_.current_style_mode->setProperty("type", "label");
    ui_.current_style_mode->setProperty(property.c_str(), key.c_str());
    ui_.style_mode_label->setProperty("type", "label");
    ui_.style_mode_label->setProperty(property.c_str(), key.c_str());

    ui_.about_widget->setProperty("type", "glassmorphism");
    ui_.about_widget->setProperty(property.c_str(), key.c_str());

    LOG_MODULE("DGLABClient", "setup_widget_properties", LOG_DEBUG, "设置元素属性完成！");
}

void DGLABClient::load_stylesheet() {
    LOG_MODULE("DGLABClient", "load_stylesheet", LOG_INFO, "开始加载样式表");
    auto& config = AppConfig::instance();
    is_light_mode_ = config.get_value<bool>("app.ui.is_light_mode", true);
    setup_widget_properties("mode", (is_light_mode_ ? "light" : "night"));
    LOG_MODULE("DGLABClient", "load_stylesheet", LOG_INFO, "当前样式：" << is_light_mode_ ? "Light" : "Night");
    if (is_light_mode_) {
        load_light_stylesheet();
        ui_.current_style_mode->setText("当前模式：Light");
    }
    else {
        load_night_stylesheet();
        ui_.current_style_mode->setText("当前模式：Night");
    }
}

void DGLABClient::load_light_stylesheet() {
    LOG_MODULE("DGLABClient", "load_light_stylesheet", LOG_DEBUG, "开始加载 Light 样式表");
    QString light_stylesheet_path = ":/style/qcss/style_light.qcss";
    bool light_stylesheet_exists = QFile::exists(light_stylesheet_path);
    if (light_stylesheet_exists) {
        LOG_MODULE("DGLABClient", "load_light_stylesheet", LOG_DEBUG, "Light 样式表存在");
        QFile light_stylesheet_file(light_stylesheet_path);
        if (light_stylesheet_file.open(QFile::ReadOnly)) {
            QString light_style_sheet = QLatin1String(light_stylesheet_file.readAll());
            qApp->setStyleSheet(light_style_sheet);
            light_stylesheet_file.close();
            LOG_MODULE("DGLABClient", "load_light_stylesheet", LOG_DEBUG, "Light 样式表加载成功");
        }
        else {
            LOG_MODULE("DGLABClient", "load_light_stylesheet", LOG_ERROR, "Light 样式表打开失败！");
        }
    }
    else {
        LOG_MODULE("DGLABClient", "load_light_stylesheet", LOG_ERROR, "Light 样式表不存在！");
    }
}

void DGLABClient::load_night_stylesheet() {
    LOG_MODULE("DGLABClient", "load_night_stylesheet", LOG_DEBUG, "开始加载 Night 样式表");
    QString night_stylesheet_path = ":/style/qcss/style_night.qcss";
    bool night_stylesheet_exists = QFile::exists(night_stylesheet_path);
    if (night_stylesheet_exists) {
        LOG_MODULE("DGLABClient", "load_night_stylesheet", LOG_DEBUG, "Night 样式表存在");
        QFile night_stylesheet_file(night_stylesheet_path);
        if (night_stylesheet_file.open(QFile::ReadOnly)) {
            QString night_style_sheet = QLatin1String(night_stylesheet_file.readAll());
            qApp->setStyleSheet(night_style_sheet);
            night_stylesheet_file.close();
            LOG_MODULE("DGLABClient", "load_night_stylesheet", LOG_DEBUG, "Night 样式表加载成功");
        }
        else {
            LOG_MODULE("DGLABClient", "load_night_stylesheet", LOG_ERROR, "Night 样式表打开失败！");
        }
    }
    else {
        LOG_MODULE("DGLABClient", "load_night_stylesheet", LOG_ERROR, "Night 样式表不存在！");
    }
}

void DGLABClient::change_theme() {
    LOG_MODULE("DGLABClient", "change_theme", LOG_INFO, "切换主题为：" << (is_light_mode_ ? "Night" : "Light"));
    is_light_mode_ = !is_light_mode_;
    auto& config = AppConfig::instance();
    config.set_value<bool>("app.ui.is_light_mode", is_light_mode_);
    load_stylesheet();
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

    connect(ui_.change_style_mode_btn, &QPushButton::clicked, this, &DGLABClient::change_theme);

    connect(this, &DGLABClient::connect_finished, this, &DGLABClient::handle_connect_finished);
    connect(this, &DGLABClient::close_finished, this, &DGLABClient::handle_close_finished);
}

void DGLABClient::setup_port_input_validation() {
    QIntValidator* validator = new QIntValidator(0, 65535, this);
    QLocale locale = QLocale::c();
    validator->setLocale(locale);
    ui_.port_input->setValidator(validator);
    connect(ui_.port_input, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (text.length() > 100) {
            ui_.port_input->setText(text.left(100));
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

    auto& config = AppConfig::instance();
    QString pythonPath = QString::fromStdString(config.get_value<std::string>("python.path", "python"));
    std::string bridge_module = config.get_value<std::string>("python.bridge_path", "./python/Bridge.py");
    QString script_path = QCoreApplication::applicationDirPath() + QString::fromStdString(bridge_module);
    LOG_MODULE("DGLABClient", "init_python_manager", LOG_INFO, "启动 Python 进程 -> [Python 解释器]路径："
        << pythonPath.toStdString() << "（注：若解释器路径直接为<Python>则使用系统默认 Python 路径）");
    LOG_MODULE("DGLABClient", "init_python_manager", LOG_INFO, "启动 Python 进程 -> [Python 服务模块]路径：" << bridge_module);
    if (bridge_module.starts_with(".")) bridge_module = bridge_module.substr(1);
    py_manager_->start_process(pythonPath, script_path);
}

void DGLABClient::reset_py_log_level() {
    auto& config = AppConfig::instance();
    //QString level = QString::fromStdString(config.get_value<std::string>("app.log_level", "DEBUG"));
    QString level = "DEBUG";
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

    // 检查路径是否有效且文件存在
    if (current_qr_path_.isEmpty() || !QFile::exists(current_qr_path_)) {
        LOG_MODULE("DGLABClient", "show_qr_dialog", LOG_WARN,
            "二维码路径无效或文件不存在: " << current_qr_path_.toStdString());
        // 显示文本提示
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

    // 加载图片
    QPixmap pixmap(current_qr_path_);
    if (pixmap.isNull()) {
        LOG_MODULE("DGLABClient", "show_qr_dialog", LOG_ERROR,
            "无法加载二维码图片: " << current_qr_path_.toStdString());
        QMessageBox::warning(this, "错误", "二维码图片文件损坏或无法读取");
        return;
    }

    LOG_MODULE("DGLABClient", "show_qr_dialog", LOG_INFO,
        "成功加载二维码图片: " << current_qr_path_.toStdString());

    // 创建并显示对话框
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

QString DGLABClient::get_local_lan_ip() {
    auto& config = AppConfig::instance();

    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    QString fallbackIp;
    QString whitelistIp;

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

    // 第1列、第2列使用普通下拉框
    rule_table_->setItemDelegateForColumn(1, new ComboBoxDelegate(channelOptions, rule_table_));
    rule_table_->setItemDelegateForColumn(2, new ComboBoxDelegate(modeOptions, rule_table_));
    // 第3列使用复杂公式构建器
    rule_table_->setItemDelegateForColumn(3, new ValueModeDelegate(rule_table_));
    // 编辑触发方式
    rule_table_->setEditTriggers(QAbstractItemView::DoubleClicked |
        QAbstractItemView::AnyKeyPressed);
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

        // 获取模式字符串
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

        // 值模式（显示占位符版本）
        std::string pattern = rm.get_rule_value_pattern(name);
        // 将 {} 替换为 {   } 便于阅读
        QString displayPattern = QString::fromStdString(pattern);
        displayPattern.replace("{}", "{   }");
        rule_table_->setItem(i, 3, new QTableWidgetItem(displayPattern));
    }
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

    // 通道选择
    QStringList channels = { "无", "A", "B" };
    QString channel = QInputDialog::getItem(this, "选择通道", "通道:", channels, 0, false, &ok);
    if (!ok) return;
    QString channelStr = (channel == "无") ? "" : channel;

    // 模式选择
    QStringList modes = { "递减", "递增", "设为", "连减", "连增" };
    QString modeStr = QInputDialog::getItem(this, "选择模式", "模式:", modes, 0, false, &ok);
    if (!ok) return;
    int mode = modes.indexOf(modeStr);

    // 值模式输入
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
    // 编辑通道
    QStringList channels = { "无", "A", "B" };
    QString channel = QInputDialog::getItem(this, "编辑规则", "通道:", channels,
        channels.indexOf(oldChannel), false, &ok);
    if (!ok) return;
    QString channelStr = (channel == "无") ? "" : channel;

    // 编辑模式
    QStringList modes = { "递减", "递增", "设为", "连减", "连增" };
    QString modeStr = QInputDialog::getItem(this, "编辑规则", "模式:", modes, oldMode, false, &ok);
    if (!ok) return;
    int mode = modes.indexOf(modeStr);

    // 编辑值模式
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

void DGLABClient::on_close_btn_clicked() {}

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
            config.set_value_with_name<int>("app.websocket.port", port, "system"); // 直接使用 port
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

void DGLABClient::handle_connect_finished(bool success, const QString& msg) {
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

void DGLABClient::handle_close_finished(bool success, const QString& msg) {
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
