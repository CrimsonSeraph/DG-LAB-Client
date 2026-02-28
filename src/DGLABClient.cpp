#include "DGLABClient.h"
#include "DebugLog.h"
#include "AppConfig.h"
#include "PyExecutorManager.h"

#include <iostream>
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
#include <QtConcurrent/QtConcurrent>

DGLABClient::DGLABClient(QWidget* parent)
    : QWidget(parent) {
    LOG_MODULE("DGLABClient", "DGLABClient", LOG_DEBUG, "开始初始化窗口");
    ui.setupUi(this);

    // 首页相关设置
    ui.debug_log->setReadOnly(true);
    ui.debug_log->setStyleSheet("");
    ui.debug_log->document()->setDefaultStyleSheet("");
    ui.debug_log->document()->setDefaultFont(QFont("Consolas", 8));
    ui.debug_log->setAcceptRichText(true);

    // 注册 Qt Sink
    LOG_MODULE("DGLABClient", "DGLABClient", LOG_DEBUG, "开始注册 Qt Sink");
    ui_log_level = ui_log_level;
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

            // 在目标对象所属线程执行；在执行时再次检查对象是否仍然存在
            QMetaObject::invokeMethod(qptr.data(), [qptr, display, level]() {
                if (!qptr) {
                    return;
                }
                qptr->append_log_message(display, level);
                }, Qt::AutoConnection);
        };
    int uiLogLevel = ui_log_level;
    qtSink.min_level = static_cast<LogLevel>(uiLogLevel);
    DebugLog::Instance().unregister_log_sink("qt_ui");
    DebugLog::Instance().register_log_sink("qt_ui", qtSink);
    LOG_MODULE("DGLABClient", "DGLABClient", LOG_DEBUG, "注册 Qt Sink 完成");

    // 创建简单的高亮器，根据行内的等级关键字为整行上色
    LOG_MODULE("DGLABClient", "DGLABClient", LOG_DEBUG, "创建简单的高亮器");
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
    logHighlighter = new LogHighlighter(ui.debug_log->document());

    // 加载首页图片
    LOG_MODULE("DGLABClient", "DGLABClient", LOG_DEBUG, "开始加载首页图片");
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
    LOG_MODULE("DGLABClient", "DGLABClient", LOG_DEBUG, "开始设置元素属性");
    ui.all->setProperty("type", "main_page");
    ui.all->setProperty("mode", "light");

    QList<QPushButton*> btns = ui.all->findChildren<QPushButton*>();
    for (QPushButton* btn : btns) {
        btn->setProperty("type", "btns");
        btn->setProperty("mode", "light");
    }
    LOG_MODULE("DGLABClient", "DGLABClient", LOG_DEBUG, "共加载" << btns.size() << "个按键");
    ui.main_image_label->setProperty("type", "main_image_label");
    ui.main_image_label->setProperty("mode", "light");
    LOG_MODULE("DGLABClient", "DGLABClient", LOG_DEBUG, "设置元素属性完成！当前全局 mode 为：light");

    // 加载样式表
    LOG_MODULE("DGLABClient", "DGLABClient", LOG_DEBUG, "开始加载样式表");
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
    // 为日志控件设置局部样式与调色板，避免全局样式覆盖字符颜色
    ui.debug_log->setStyleSheet("QTextEdit#debug_log { color: black; }");
    QPalette pal = ui.debug_log->palette();
    pal.setColor(QPalette::Text, Qt::black);
    pal.setColor(QPalette::WindowText, Qt::black);
    ui.debug_log->setPalette(pal);

    // 绑定信号与槽
    LOG_MODULE("DGLABClient", "DGLABClient", LOG_DEBUG, "开始绑定信号与槽");
    connect(ui.main_first_btn, &QPushButton::clicked, this, &DGLABClient::on_main_first_btn_clicked);
    connect(ui.main_config_btn, &QPushButton::clicked, this, &DGLABClient::on_main_config_btn_clicked);
    connect(ui.main_setting_btn, &QPushButton::clicked, this, &DGLABClient::on_main_setting_btn_clicked);
    connect(ui.main_about_btn, &QPushButton::clicked, this, &DGLABClient::on_main_about_btn_clicked);

    connect(ui.start_connect_btn, &QPushButton::clicked, this, &DGLABClient::on_start_connect_btn_clicked);
    connect(ui.close_connect_btn, &QPushButton::clicked, this, &DGLABClient::on_close_connect_btn_clicked);
    connect(ui.start_btn, &QPushButton::clicked, this, &DGLABClient::on_start_btn_clicked);
    connect(ui.close_btn, &QPushButton::clicked, this, &DGLABClient::on_close_btn_clicked);

    connect(this, &DGLABClient::connect_finished, this, &DGLABClient::handle_connect_finished);
    connect(this, &DGLABClient::code_content_ready, this, &DGLABClient::handle_code_content_ready);
    connect(this, &DGLABClient::close_finished, this, &DGLABClient::handle_close_finished);

    // 设置默认页面
    ui.stackedWidget->setCurrentWidget(ui.first_page);
    LOG_MODULE("DGLABClient", "DGLABClient", LOG_DEBUG, "窗口初始化完成");
}

DGLABClient::~DGLABClient() {
    DebugLog::Instance().unregister_log_sink("qt_ui");
}

void DGLABClient::on_main_first_btn_clicked() {
    LOG_MODULE("DGLABClient", "on_main_first_btn_clicked", LOG_INFO, "main_first_btn 按键触发，跳转 first_page");
    ui.stackedWidget->setCurrentWidget(ui.first_page);
}

void DGLABClient::on_main_config_btn_clicked() {
    LOG_MODULE("DGLABClient", "on_main_config_btn_clicked", LOG_INFO, "main_config_btn 按键触发，跳转 config_page");
    ui.stackedWidget->setCurrentWidget(ui.config_page);
}

void DGLABClient::on_main_setting_btn_clicked() {
    LOG_MODULE("DGLABClient", "on_main_setting_btn_clicked", LOG_INFO, "main_setting_btn 按键触发，跳转 setting_page");
    ui.stackedWidget->setCurrentWidget(ui.setting_page);
}

void DGLABClient::on_main_about_btn_clicked() {
    LOG_MODULE("DGLABClient", "on_main_about_btn_clicked", LOG_INFO, "main_about_btn 按键触发，跳转 about_page");
    ui.stackedWidget->setCurrentWidget(ui.about_page);
}

void DGLABClient::on_start_connect_btn_clicked() {
    LOG_MODULE("DGLABClient", "on_start_connect_btn_clicked", LOG_INFO, "start_connect_btn 按键触发");
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
}

void DGLABClient::on_close_connect_btn_clicked() {
    LOG_MODULE("DGLABClient", "on_close_connect_btn_clicked", LOG_INFO, "close_connect_btn 按键触发");
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

void DGLABClient::change_ui_log_level(LogLevel new_level) {
    LOG_MODULE("DGLABClient", "change_ui_log_level", LOG_DEBUG, "修改 UI 日志级别: 旧=" << ui_log_level << " 新=" << new_level);
    ui_log_level = new_level;
    DebugLog::Instance().set_log_sink_level("qt_ui", new_level);
}

void DGLABClient::append_log_message(const QString& message, int level) {
    QString clean = message;
    QRegularExpression ansi("\\x1B\\[[0-9;]*[A-Za-z]");
    clean.remove(ansi);

    clean.replace('\r', "");

    append_colored_text(ui.debug_log, clean);
}

void DGLABClient::append_colored_text(QTextEdit* edit, const QString& text) {
    // 插入纯文本，让高亮器负责为整行着色（避免局部格式被全局 QSS 覆盖）
    edit->moveCursor(QTextCursor::End);
    edit->insertPlainText(text + "\n");
    edit->moveCursor(QTextCursor::End);
    edit->ensureCursorVisible();

    // 如果存在高亮器，通知它重绘以应用行级颜色
    if (logHighlighter) {
        logHighlighter->rehighlight();
    }
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
    // 在后台线程执行连接操作，避免阻塞 UI
    LOG_MODULE("DGLABClient", "start_async_connect", LOG_DEBUG, "在后台线程执行连接操作");
    QtConcurrent::run([this]() {
        auto& manager = PyExecutorManager::instance();
        bool success = false;
        QString errorMsg;
        try {
            if (!manager.has_executor("WebSocketCore", "DGLabClient")) {
                // 注册执行器
                LOG_MODULE("DGLABClient", "start_async_connect", LOG_DEBUG, "开始注册执行器");
                if (!manager.register_executor("WebSocketCore", "DGLabClient", true)) {
                    emit connect_finished(false, "执行器注册失败");
                    LOG_MODULE("DGLABClient", "start_async_connect", LOG_ERROR, "注册执行器失败");
                    return;
                }
            }

            // 连接（同步调用）
            LOG_MODULE("DGLABClient", "start_async_connect", LOG_INFO, "正在连接");
            bool is_connect = manager.call_sync<bool>("WebSocketCore", "DGLabClient", "connect");
            if (!is_connect) {
                emit connect_finished(false, "连接失败");
                return;
            }

            // 连接完成，获取二维码内容（同步调用）
            LOG_MODULE("DGLABClient", "start_async_connect", LOG_INFO, "开始获取二维码内容");
            std::string qr = manager.call_sync<std::string>("WebSocketCore", "DGLabClient", "generate_qr_content");
            emit code_content_ready(QString::fromStdString(qr));

            // 发送测试指令（不关心结果）
            manager.call_sync<void>("WebSocketCore", "DGLabClient", "sync_send_strength_operation", 1, 2, 10);

            emit connect_finished(true, "连接成功");
        }
        catch (const std::exception& e) {
            emit connect_finished(false, QString("异常: ") + e.what());
        }
        catch (...) {
            emit connect_finished(false, "未知异常");
        }
        });
}

void DGLABClient::close_async_connect() {
    // 在后台线程执行连接操作，避免阻塞 UI
    LOG_MODULE("DGLABClient", "close_async_connect", LOG_DEBUG, "在后台线程执行断开操作");
    QtConcurrent::run([this]() {
        auto& manager = PyExecutorManager::instance();
        bool success = false;
        QString errorMsg;
        try {
            // 断开（同步调用）
            LOG_MODULE("DGLABClient", "close_async_connect", LOG_INFO, "正在断开连接");
            bool is_close = manager.call_sync<bool>("WebSocketCore", "DGLabClient", "sync_close");
            if (!is_close) {
                emit close_finished(false, "断开失败");
                return;
            }
            emit close_finished(true, "断开成功");
        }
        catch (const std::exception& e) {
            emit close_finished(false, QString("异常: ") + e.what());
        }
        catch (...) {
            emit close_finished(false, "未知异常");
        }
        });
}
