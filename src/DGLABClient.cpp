#include "DGLABClient.h"
#include "DebugLog.h"
#include "AppConfig.h"
#include "PythonSubprocessManager.h"

#include <iostream>
#include <algorithm>
#include <QPixmap>
#include <QFile>
#include <QList>
#include <QPushButton>
#include <QApplication>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QColor>
#include <QRegularExpression>
#include <QPointer>
#include <QSyntaxHighlighter>
#include <QThreadPool>
#include <QCoreApplication>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDebug>
#include <QLineEdit>
#include <QIntValidator>
#include <QIcon>
#include <QAction>

DGLABClient::DGLABClient(QWidget* parent)
    : QWidget(parent) {
    LOG_MODULE("DGLABClient", "DGLABClient", LOG_DEBUG, "开始初始化窗口");
    ui.setupUi(this);

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
    init_python_manager();

    LOG_MODULE("DGLABClient", "DGLABClient", LOG_INFO, "窗口初始化完成");
}

DGLABClient::~DGLABClient() {
    DebugLog::Instance().unregister_log_sink("qt_ui");
}

void DGLABClient::append_log_message(const QString& message, int level) {
    QString clean = message;
    QRegularExpression ansi("\\x1B\\[[0-9;]*[A-Za-z]");
    clean.remove(ansi);
    clean.replace('\r', "");
    append_colored_text(ui.debug_log, clean);
}

void DGLABClient::append_colored_text(QTextEdit* edit, const QString& text) {
    edit->moveCursor(QTextCursor::End);
    edit->insertPlainText(text + "\n");
    edit->moveCursor(QTextCursor::End);
    edit->ensureCursorVisible();
    if (log_highlighter) {
        log_highlighter->rehighlight();
    }
}

template<typename Callback>
void DGLABClient::async_call(const QJsonObject& cmd, int timeout, Callback&& callback) {
    LOG_MODULE("DGLABClient", "asyncCall", LOG_DEBUG, "在后台线程执行异步调用");
    QThreadPool::globalInstance()->start([this, cmd, timeout, callback = std::forward<Callback>(callback)]() mutable {
        try {
            LOG_MODULE("DGLABClient", "asyncCall", LOG_INFO, "正在发送命令: " << DebugLogUtil::remove_newline(QJsonDocument(cmd).toJson().toStdString()));
            m_pyManager->call(cmd, [callback = std::move(callback)](const QJsonObject& resp) mutable {
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
    ui.debug_log->setReadOnly(true);
    ui.debug_log->setStyleSheet("");
    ui.debug_log->document()->setDefaultStyleSheet("");
    ui.debug_log->document()->setDefaultFont(QFont("Consolas", 8));
    ui.debug_log->setAcceptRichText(true);
}

void DGLABClient::register_log_sink() {
    LOG_MODULE("DGLABClient", "register_log_sink", LOG_DEBUG, "开始注册 Qt Sink");
    qtSink.callback = [qptr = QPointer<DGLABClient>(this)](const std::string& module,
        const std::string& method,
        LogLevel level,
        const std::string& message) {
            if (!qptr)
                return;
            QString display = QString("[%1] <%2> (%3): %4")
                .arg(QString::fromStdString(module))
                .arg(QString::fromStdString(method))
                .arg(QString::fromStdString(DebugLog::Instance().level_to_string(level)))
                .arg(QString::fromStdString(message));

            QMetaObject::invokeMethod(qptr.data(), [qptr, display, level]() {
                if (!qptr) return;
                qptr->append_log_message(display, level);
                }, Qt::AutoConnection);
        };
    qtSink.min_level = ui_log_level;
    DebugLog::Instance().unregister_log_sink("qt_ui");
    DebugLog::Instance().register_log_sink("qt_ui", qtSink);
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
    log_highlighter = new LogHighlighter(ui.debug_log->document());
}

void DGLABClient::load_main_image() {
    LOG_MODULE("DGLABClient", "load_main_image", LOG_DEBUG, "开始加载首页图片");
    QString image_path = ":/image/assets/normal_image/main_image.png";
    bool main_image_exists = QFile::exists(image_path);
    if (main_image_exists) {
        ui.main_image_label->setScaledContents(true);
        ui.main_image_label->setStyleSheet("QLabel{border-image: url(" + image_path + ") 0 0 0 0 stretch stretch;}");
        ui.main_image_label->setText("");
        LOG_MODULE("DGLABClient", "load_main_image", LOG_DEBUG, "首页图片加载成功");
    }
    else {
        ui.main_image_label->setText("加载失败！");
        LOG_MODULE("DGLABClient", "load_main_image", LOG_ERROR, "首页图片资源不存在！");
    }
}

void DGLABClient::create_tray_icon() {
    LOG_MODULE("DGLABClient", "create_tray_icon", LOG_DEBUG, "开始创建托盘图标");
    QString tray_icon_path = ":/image/assets/normal_image/main_image.png";
    bool tray_icon_exists = QFile::exists(tray_icon_path);
    if (tray_icon_exists) {
        tray_icon = new QSystemTrayIcon(this);
        tray_icon->setIcon(QIcon(tray_icon_path));
        AppConfig& config = AppConfig::instance();
        std::string app_name = config.get_value<std::string>("app.name", "DG-LAB-Client");
        tray_icon->setToolTip(QString::fromStdString(app_name));

        tray_menu = new QMenu(this);
        QAction* show_action = new QAction("显示", this);
        QAction* quit_action = new QAction("退出", this);

        connect(show_action, &QAction::triggered, this, [=]() {
            this->showNormal();
            this->activateWindow();
            });
        connect(quit_action, &QAction::triggered, qApp, &QApplication::quit);
        tray_menu->addAction(show_action);
        tray_menu->addSeparator();
        tray_menu->addAction(quit_action);
        tray_icon->setContextMenu(tray_menu);
        connect(tray_icon, &QSystemTrayIcon::activated, this, [=](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::DoubleClick) {
                this->showNormal();
                this->activateWindow();
            }
            });
        tray_icon->show();
        LOG_MODULE("DGLABClient", "create_tray_icon", LOG_DEBUG, "托盘图标加载完成");
    }
    else {
        ui.main_image_label->setText("加载失败！");
        LOG_MODULE("DGLABClient", "create_tray_icon", LOG_ERROR, "托盘图标不存在！");
    }
}

void DGLABClient::setup_widget_properties(const std::string& property, const std::string& key) {
    LOG_MODULE("DGLABClient", "setup_widget_properties", LOG_DEBUG, "开始设置元素属性");
    LOG_MODULE("DGLABClient", "setup_widget_properties", LOG_INFO, "设置元素统一属性[" << property << "]为：" << key);

    ui.all->setProperty("type", "main_page");
    ui.all->setProperty(property.c_str(), key.c_str());

    ui.left_btns_bar->setProperty("type", "glassmorphism");
    ui.left_btns_bar->setProperty(property.c_str(), key.c_str());

    QList<QPushButton*> btns = ui.all->findChildren<QPushButton*>();
    for (QPushButton* btn : btns) {
        btn->setProperty("type", "btns");
        btn->setProperty(property.c_str(), key.c_str());
    }
    LOG_MODULE("DGLABClient", "setup_widget_properties", LOG_DEBUG, "共加载" << btns.size() << "个按键");

    ui.main_image_label->setProperty("type", "main_image_label");
    ui.main_image_label->setProperty(property.c_str(), key.c_str());

    ui.main_page_btns_bar->setProperty("type", "glassmorphism");
    ui.main_page_btns_bar->setProperty(property.c_str(), key.c_str());

    ui.debug_log->setProperty("type", "debug_log");
    ui.debug_log->setProperty(property.c_str(), key.c_str());

    ui.port_info->setProperty("type", "glassmorphism");
    ui.port_info->setProperty(property.c_str(), key.c_str());

    AppConfig& config = AppConfig::instance();
    int old_port = config.get_value<int>("app.websocket.port", 9999);
    ui.port_input->setPlaceholderText("请输入 WebSocket 端口号，当前端口号：" + QString::number(old_port));
    ui.port_input->setProperty("type", "input");
    ui.port_input->setProperty(property.c_str(), key.c_str());
    ui.port_label->setProperty("type", "label");
    ui.port_label->setProperty(property.c_str(), key.c_str());

    ui.config_scroll_area->setProperty("type", "none");
    ui.config_scrollAreaWidgetContents->setProperty("type", "none");
    ui.config_widgrt->setProperty("type", "glassmorphism");
    ui.config_widgrt->setProperty(property.c_str(), key.c_str());

    ui.setting_scroll_area->setProperty("type", "none");
    ui.setting_scrollAreaWidgetContents->setProperty("type", "none");
    ui.setting_grid->setProperty("type", "glassmorphism");
    ui.setting_grid->setProperty(property.c_str(), key.c_str());
    ui.style_mode->setProperty("type", "glassmorphism");
    ui.style_mode->setProperty(property.c_str(), key.c_str());
    ui.current_style_mode->setProperty("type", "label");
    ui.current_style_mode->setProperty(property.c_str(), key.c_str());
    ui.style_mode_label->setProperty("type", "label");
    ui.style_mode_label->setProperty(property.c_str(), key.c_str());

    ui.about_widget->setProperty("type", "glassmorphism");
    ui.about_widget->setProperty(property.c_str(), key.c_str());

    LOG_MODULE("DGLABClient", "setup_widget_properties", LOG_DEBUG, "设置元素属性完成！");
}

void DGLABClient::load_stylesheet() {
    LOG_MODULE("DGLABClient", "load_stylesheet", LOG_INFO, "开始加载样式表");
    AppConfig& config = AppConfig::instance();
    is_light_mode = config.get_value<bool>("app.ui.is_light_mode", true);
    setup_widget_properties("mode", is_light_mode ? "light" : "night");
    LOG_MODULE("DGLABClient", "load_stylesheet", LOG_INFO, "当前样式：" << is_light_mode ? "Light" : "Night");
    if (is_light_mode) {
        load_light_stylesheet();
        ui.current_style_mode->setText("当前模式：Light");
    }
    else {
        load_night_stylesheet();
        ui.current_style_mode->setText("当前模式：Night");
    }
}

void DGLABClient::load_light_stylesheet() {
    LOG_MODULE("DGLABClient", "oad_light_stylesheet", LOG_DEBUG, "开始加载 Light 样式表");
    QString light_stylesheet_path = ":/style/qcss/style_light.qcss";
    bool light_stylesheet_exists = QFile::exists(light_stylesheet_path);
    if (light_stylesheet_exists) {
        LOG_MODULE("DGLABClient", "DGLABClient", LOG_DEBUG, "Light 样式表存在");
        QFile light_stylesheet_file(light_stylesheet_path);
        if (light_stylesheet_file.open(QFile::ReadOnly)) {
            QString light_style_sheet = QLatin1String(light_stylesheet_file.readAll());
            qApp->setStyleSheet(light_style_sheet);
            light_stylesheet_file.close();
            LOG_MODULE("DGLABClient", "oad_light_stylesheet", LOG_DEBUG, "Light 样式表加载成功");
        }
        else {
            LOG_MODULE("DGLABClient", "oad_light_stylesheet", LOG_ERROR, "Ligth 样式表打开失败！");
        }
    }
    else {
        LOG_MODULE("DGLABClient", "oad_light_stylesheet", LOG_ERROR, "Light 样式表不存在！");
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
        LOG_MODULE("DGLABClient", "load_night_stylesheet", LOG_ERROR, "Nigth 样式表不存在！");
    }
}

void DGLABClient::change_theme() {
    LOG_MODULE("DGLABClient", "change_theme", LOG_INFO, "切换主题为：" << is_light_mode ? "Night" : "Light");
    is_light_mode = !is_light_mode;
    AppConfig& config = AppConfig::instance();
    config.set_value<bool>("app.ui.is_light_mode", is_light_mode);
    load_stylesheet();
}

void DGLABClient::setup_log_widget_style() {
    LOG_MODULE("DGLABClient", "setup_log_widget_style", LOG_DEBUG, "设置硬编码样式");
    ui.debug_log->setStyleSheet("QTextEdit#debug_log { color: black; }");
    QPalette pal = ui.debug_log->palette();
    pal.setColor(QPalette::Text, Qt::black);
    pal.setColor(QPalette::WindowText, Qt::black);
    ui.debug_log->setPalette(pal);
}

void DGLABClient::setup_connections() {
    LOG_MODULE("DGLABClient", "setup_connections", LOG_DEBUG, "开始绑定信号与槽");
    connect(ui.main_first_btn, &QPushButton::clicked, this, &DGLABClient::on_main_first_btn_clicked);
    connect(ui.main_config_btn, &QPushButton::clicked, this, &DGLABClient::on_main_config_btn_clicked);
    connect(ui.main_setting_btn, &QPushButton::clicked, this, &DGLABClient::on_main_setting_btn_clicked);
    connect(ui.main_about_btn, &QPushButton::clicked, this, &DGLABClient::on_main_about_btn_clicked);

    connect(ui.start_connect_btn, &QPushButton::clicked, this, &DGLABClient::on_start_connect_btn_clicked);
    connect(ui.close_connect_btn, &QPushButton::clicked, this, &DGLABClient::on_close_connect_btn_clicked);
    connect(ui.start_btn, &QPushButton::clicked, this, &DGLABClient::on_start_btn_clicked);
    connect(ui.close_btn, &QPushButton::clicked, this, &DGLABClient::on_close_btn_clicked);

    connect(ui.port_confirm_btn, &QPushButton::clicked, this, &DGLABClient::set_port);

    connect(ui.change_style_mode_btn, &QPushButton::clicked, this, &DGLABClient::change_theme);

    connect(this, &DGLABClient::connect_finished, this, &DGLABClient::handle_connect_finished);
    connect(this, &DGLABClient::code_content_ready, this, &DGLABClient::handle_code_content_ready);
    connect(this, &DGLABClient::close_finished, this, &DGLABClient::handle_close_finished);
}

void DGLABClient::setup_port_input_validation() {
    QIntValidator* validator = new QIntValidator(0, 65535, this);
    QLocale locale = QLocale::c();
    validator->setLocale(locale);
    ui.port_input->setValidator(validator);
    connect(ui.port_input, &QLineEdit::textChanged, this, [=](const QString& text) {
        if (text.length() > 100) {
            ui.port_input->setText(text.left(100));
        }
        });
}

void DGLABClient::setup_default_page() {
    ui.stackedWidget->setCurrentWidget(ui.first_page);
}

void DGLABClient::init_python_manager() {
    LOG_MODULE("DGLABClient", "init_python_manager", LOG_INFO, "启动并连接 Python 服务");
    m_pyManager = new PythonSubprocessManager(this);

    connect(m_pyManager, &PythonSubprocessManager::started,
        this, [this](bool success, const QString& error) {
            if (!success) {
                emit connect_finished(false, "Python 进程启动错误: " + error);
                return;
            }
        });
    connect(m_pyManager, &PythonSubprocessManager::finished,
        this, [this]() {
            emit close_finished(true, "Python 进程关闭");
        });

    AppConfig& config = AppConfig::instance();
    QString pythonPath = QString::fromStdString(config.get_value<std::string>("python.path", "python"));
    std::string bridge_module = config.get_value<std::string>("python.bridge_path", "/python/Bridge.py");
    QString scriptPath = QCoreApplication::applicationDirPath() + QString::fromStdString(bridge_module);
    LOG_MODULE("DGLABClient", "init_python_manager", LOG_INFO, "启动 Python 进程 -> [Python 解释器]路径："
        << pythonPath.toStdString() << "（注：若解释器路径直接为<Python>则使用系统默认 Python 路径）");
    LOG_MODULE("DGLABClient", "init_python_manager", LOG_INFO, "启动 Python 进程 -> [Python 服务模块]路径：" << bridge_module);
    m_pyManager->start_process(pythonPath, scriptPath);
}

void DGLABClient::on_main_first_btn_clicked() {
    LOG_MODULE("DGLABClient", "on_main_first_btn_clicked", LOG_DEBUG, "main_first_btn 按键触发，跳转 first_page");
    ui.stackedWidget->setCurrentWidget(ui.first_page);
}

void DGLABClient::on_main_config_btn_clicked() {
    LOG_MODULE("DGLABClient", "on_main_config_btn_clicked", LOG_DEBUG, "main_config_btn 按键触发，跳转 config_page");
    ui.stackedWidget->setCurrentWidget(ui.config_page);
}

void DGLABClient::on_main_setting_btn_clicked() {
    LOG_MODULE("DGLABClient", "on_main_setting_btn_clicked", LOG_DEBUG, "main_setting_btn 按键触发，跳转 setting_page");
    ui.stackedWidget->setCurrentWidget(ui.setting_page);
}

void DGLABClient::on_main_about_btn_clicked() {
    LOG_MODULE("DGLABClient", "on_main_about_btn_clicked", LOG_DEBUG, "main_about_btn 按键触发，跳转 about_page");
    ui.stackedWidget->setCurrentWidget(ui.about_page);
}

void DGLABClient::on_start_connect_btn_clicked() {
    LOG_MODULE("DGLABClient", "on_start_connect_btn_clicked", LOG_DEBUG, "start_connect_btn 按键触发");
    if (start_connect_btn_loading) {
        LOG_MODULE("DGLABClient", "on_start_connect_btn_clicked", LOG_DEBUG, "正在连接中，忽略重复点击");
        return;
    }
    if (!is_connected) {
        LOG_MODULE("DGLABClient", "on_start_connect_btn_clicked", LOG_INFO, "开始连接");
        start_connect_btn_loading = true;
        ui.start_connect_btn->setEnabled(false);
        start_async_connect();
    }
    else {
        LOG_MODULE("DGLABClient", "on_start_connect_btn_clicked", LOG_INFO, "已连接");
    }
}

void DGLABClient::on_close_connect_btn_clicked() {
    LOG_MODULE("DGLABClient", "on_close_connect_btn_clicked", LOG_DEBUG, "close_connect_btn 按键触发");
    if (close_connect_btn_loading) {
        LOG_MODULE("DGLABClient", "on_close_connect_btn_clicked", LOG_DEBUG, "正在断开中，忽略重复点击");
        return;
    }
    if (is_connected) {
        LOG_MODULE("DGLABClient", "on_close_connect_btn_clicked", LOG_INFO, "开始断开连接");
        close_connect_btn_loading = true;
        ui.close_connect_btn->setEnabled(false);
        close_async_connect();
    }
    else {
        LOG_MODULE("DGLABClient", "on_close_connect_btn_clicked", LOG_INFO, "没有连接");
    }
}

void DGLABClient::on_start_btn_clicked() {
}

void DGLABClient::on_close_btn_clicked() {
}

void DGLABClient::change_ui_log_level() {
    AppConfig& config = AppConfig::instance();
    int new_level = config.get_value<int>("app.log.ui_log_level", 0);
    LOG_MODULE("DGLABClient", "change_ui_log_level", LOG_INFO, "修改 UI 日志级别: 旧=" << ui_log_level << " 新=" << new_level);
    ui_log_level = DebugLog::int_to_log_level(new_level);
    DebugLog::Instance().set_log_sink_level("qt_ui", ui_log_level);
}

void DGLABClient::set_port() {
    QString input = ui.port_input->text().trimmed();
    bool ok;
    int port = input.toInt(&ok);
    if (ok && port >= 0 && port <= 65535) {
        LOG_MODULE("DGLABClient", "set_port", LOG_DEBUG, "开始设置端口");
        AppConfig& config = AppConfig::instance();
        config.set_value_with_name<int>("app.websocket.port", input.toInt(), "system");
        LOG_MODULE("DGLABClient", "set_port", LOG_INFO, "设置端口完成：" << input.toStdString());
    }
    else {
        LOG_MODULE("DGLABClient", "set_port", LOG_WARN, "设置端口失败！非合法端口：" << input.toStdString());
    }
    ui.port_input->setText("");
    AppConfig& config = AppConfig::instance();
    int old_port = config.get_value<int>("app.websocket.port", 9999);
    ui.port_input->setPlaceholderText("请输入 WebSocket 端口号，当前端口号：" + QString::number(old_port));
}

void DGLABClient::handle_connect_finished(bool success, const QString& msg) {
    start_connect_btn_loading = false;
    ui.start_connect_btn->setEnabled(true);
    if (success) {
        is_connected = true;
        LOG_MODULE("DGLABClient", "handle_connect_finished", LOG_INFO, msg.toStdString());
    }
    else {
        LOG_MODULE("DGLABClient", "handle_connect_finished", LOG_ERROR, msg.toStdString());
    }
}

void DGLABClient::handle_code_content_ready(const QString& content) {
}

void DGLABClient::handle_close_finished(bool success, const QString& msg) {
    close_connect_btn_loading = false;
    ui.close_connect_btn->setEnabled(true);
    if (success) {
        is_connected = false;
        LOG_MODULE("DGLABClient", "handle_close_finished", LOG_INFO, msg.toStdString());
    }
    else {
        LOG_MODULE("DGLABClient", "handle_close_finished", LOG_ERROR, msg.toStdString());
    }
}

void DGLABClient::start_async_connect() {
    LOG_MODULE("DGLABClient", "start_async_connect", LOG_INFO, "正在更新端口");
    AppConfig& config = AppConfig::instance();
    int port = config.get_value<int>("app.websocket.port", 9999);
    QJsonObject update_port_cmd;
    update_port_cmd["cmd"] = "set_ws_url";
    update_port_cmd["port"] = port;
    async_call(update_port_cmd, 5000, [this](bool ok, QString msg) {
        if (ok) {
            LOG_MODULE("DGLABClient", "start_async_connect", LOG_INFO, "端口更新成功，继续连接");
            QJsonObject connect_cmd;
            connect_cmd["cmd"] = "connect";
            async_call(connect_cmd, 5000, [this](bool ok, QString msg) {
                emit connect_finished(ok, msg);
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
    if (tray_icon->isVisible()) {
        tray_icon->showMessage("提示", "程序已最小化到系统托盘",
            QSystemTrayIcon::Information, 2000);
        this->hide();
        event->ignore();
    }
    else {
        event->accept();
    }
}
