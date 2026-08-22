/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "DGLABClient.h"

#include "AppConfig.h"
#include "ComboBoxDelegate.h"
#include "DGLABClient_utils.hpp"
#include "DebugLog.h"
#include "EditableLabel.h"
#include "FormulaBuilderDialog.h"
#include "IpSelector.h"
#include "LogExportSettingsDialog.h"
#include "ModuleManager.h"
#include "ModuleValuesDialog.h"
#include "ParentEditDialog.h"
#include "PythonSubprocessManager.h"
#include "RuleManager.h"
#include "SampledWaveformWidget.h"
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
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHash>
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
#include <QMouseEvent>
#include <QNetworkInterface>
#include <QPalette>
#include <QPixmap>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSyntaxHighlighter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

// ============================================
// ModuleCard - 模块页可点击卡片（局部辅助类）
// 显示模块名称与模块内数值的最小查询周期，点击弹出数值展示窗口
// ============================================
class ModuleCard : public QFrame {
    Q_OBJECT

public:
    /// @brief 构造函数
    /// @param module_name 模块名称
    /// @param parent 父窗口指针
    explicit ModuleCard(const QString& module_name, QWidget* parent = nullptr)
        : QFrame(parent)
        , module_name_(module_name) {
        setProperty("type", "module_card");
        setFrameShape(QFrame::NoFrame);
        setCursor(Qt::PointingHandCursor);
        setMinimumHeight(64);

        QVBoxLayout* card_layout = new QVBoxLayout(this);
        card_layout->setContentsMargins(14, 10, 14, 10);
        card_layout->setSpacing(6);

        name_label_ = new QLabel(module_name, this);
        name_label_->setProperty("type", "module_card_name");
        period_label_ = new QLabel(this);
        period_label_->setProperty("type", "module_card_period");
        card_layout->addWidget(name_label_);
        card_layout->addWidget(period_label_);
    }

    /// @brief 获取卡片对应的模块名称
    /// @return 模块名称
    inline const QString& get_module_name() const { return module_name_; }

    /// @brief 设置最小查询周期显示文本
    /// @param text 周期显示文本
    inline void set_period_text(const QString& text) { period_label_->setText(text); }

signals:
    /// @brief 卡片被点击时发出
    void clicked();

protected:
    /// @brief 重写鼠标释放事件: 左键点击发出 clicked 信号
    /// @param event 鼠标事件
    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && rect().contains(event->pos())) {
            emit clicked();
        }
        QFrame::mouseReleaseEvent(event);
    }

private:
    // -------------------- 成员变量 --------------------
    QString module_name_;  ///< 模块名称
    QLabel* name_label_ = nullptr;    ///< 模块名称标签
    QLabel* period_label_ = nullptr;  ///< 周期显示标签
};

// ============================================
// 构造/析构（public）
// ============================================

DGLABClient::DGLABClient(QWidget* parent)
    : QWidget(parent) {
    LOG_MODULE("DGLABClient", "DGLABClient", LOG_DEBUG, "开始初始化窗口");
    ui_.setupUi(this);

    init_style();
    normal_init();
    init_log();
    init_label();
    init_connect();

    connect(ui_.minimize_btn, &QPushButton::clicked, this, &DGLABClient::close);
    connect(ui_.close_btn, &QPushButton::clicked, qApp, &QApplication::quit);
    setWindowFlags(Qt::Window | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
    setWindowFlag(Qt::WindowCloseButtonHint, false);
    LOG_MODULE("DGLABClient", "DGLABClient", LOG_INFO, "窗口初始化完成");
}

DGLABClient::~DGLABClient() {
    delete_old_qr_file();
    DebugLog::instance().unregister_log_sink("qt_ui");
    // 停止自动日志并清理多余日志（自动目录仅保留最新 N 份，分片日志视为一份）
    log_exporter_.stop_auto_log();
    log_exporter_.cleanup_old_logs();
}

void DGLABClient::normal_init() {
    setup_port_input_validation();
    setup_channel_value_editor_input_validation();
    setup_default_page();
    create_tray_icon();
    set_port_label_mode();
    setup_rules_ui();
    setup_module_ui();
    connect_rule_engine();
    setup_channel_cards();
    init_python_manager();
    // 加载日志设置（user.json 的 app.log）并启动自动日志（配置系统加载完毕后记录）
    log_exporter_.load_settings();
    log_exporter_.start_auto_log();
}

void DGLABClient::init_log() {
    change_ui_log_level();
    setup_debug_log();
    register_log_sink();
    create_log_highlighter();
}

void DGLABClient::init_label() {
    refresh_icon();
    refresh_channel_info();
    refresh_channel_strength();
    refresh_connect_label();
    refresh_ip_port_label();
    refresh_theme_label();
}

void DGLABClient::init_connect() const {
    connect_about_page();
    connect_about_channel_contral();
    connect_about_connect();
    connect_about_theme();
    // 日志导出按钮
    connect(ui_.export_log_btn, &QPushButton::clicked, this, &DGLABClient::on_export_log);
    connect(ui_.more_log_setting_btn, &QPushButton::clicked, this, &DGLABClient::on_more_log_setting);
}

void DGLABClient::init_style() {
    setup_log_widget_style();
    apply_widget_properties();
    load_stylesheet();
    setup_inline_style();
    refresh_style();
}

// ============================================
// 公共接口（public）
// ============================================
void DGLABClient::change_ui_log_level() {
    auto& config = AppConfig::instance();
    int new_level = config.get_value<int>("app.log.ui_log_level", 0);
    LOG_MODULE("DGLABClient", "change_ui_log_level", LOG_INFO, "修改 UI 日志级别: 旧=" << ui_log_level_ << " 新=" << new_level);
    ui_log_level_ = DebugLog::int_to_log_level(new_level);
    DebugLog::instance().set_log_sink_level("qt_ui", ui_log_level_);
}

void DGLABClient::refresh_icon() {
    LOG_MODULE("DGLABClient", "refresh_icon", LOG_DEBUG, "开始刷新图标");
    QString image_path = ":/image/assets/normal_image/main_image.png";
    bool main_image_exists = QFile::exists(image_path);
    if (main_image_exists) {
        ui_.app_icon->setScaledContents(true);
        ui_.app_icon->setStyleSheet("QLabel{border-image: url(" + image_path + ") 0 0 0 0 stretch stretch;}");
        ui_.app_icon->setText("");
        LOG_MODULE("DGLABClient", "refresh_icon", LOG_DEBUG, "图标加载成功");
    }
    else {
        ui_.app_icon->setText("加载失败！");
        LOG_MODULE("DGLABClient", "refresh_icon", LOG_ERROR, "图标资源不存在！");
    }
}

void DGLABClient::change_main_page() {
    LOG_MODULE("DGLABClient", "change_main_page", LOG_DEBUG, "main_first_btn 按键触发，跳转 main_page");
    ui_.center_pages->setCurrentWidget(ui_.main_page);
}

void DGLABClient::change_config_page() {
    LOG_MODULE("DGLABClient", "change_config_page", LOG_DEBUG, "main_config_btn 按键触发，跳转 config_page");
    ui_.center_pages->setCurrentWidget(ui_.config_page);
}

void DGLABClient::change_rule_page() {
    LOG_MODULE("DGLABClient", "change_rule_page", LOG_DEBUG, "main_rule_btn 按键触发，跳转 rule_page");
    ui_.center_pages->setCurrentWidget(ui_.rule_page);
}

void DGLABClient::change_module_page() {
    LOG_MODULE("DGLABClient", "change_module_page", LOG_DEBUG, "main_module_btn 按键触发，跳转 module_page");
    ui_.center_pages->setCurrentWidget(ui_.module_page);
}

void DGLABClient::change_about_page() {
    LOG_MODULE("DGLABClient", "change_about_page", LOG_DEBUG, "main_about_btn 按键触发，跳转 about_page");
    ui_.center_pages->setCurrentWidget(ui_.about_page);
}

void DGLABClient::connect_about_page() const {
    LOG_MODULE("DGLABClient", "connect_about_page", LOG_DEBUG, "连接页面相关槽函数");
    connect(ui_.main_page_btn, &QPushButton::clicked, this, &DGLABClient::change_main_page);
    connect(ui_.config_page_btn, &QPushButton::clicked, this, &DGLABClient::change_config_page);
    connect(ui_.rule_page_btn, &QPushButton::clicked, this, &DGLABClient::change_rule_page);
    connect(ui_.module_page_btn, &QPushButton::clicked, this, &DGLABClient::change_module_page);
    connect(ui_.about_page_btn, &QPushButton::clicked, this, &DGLABClient::change_about_page);
}

void DGLABClient::refresh_channel_info() {
    ui_.A_strength_show_label->setText(QString::number(A_strength_));
    ui_.A_start_btn->setText(is_A_start ? "关闭" : "启动");
    ui_.B_strength_show_label->setText(QString::number(B_strength_));
    ui_.B_start_btn->setText(is_B_start ? "关闭" : "启动");
}

void DGLABClient::enable_A() {
    if (is_A_start) {
        LOG_MODULE("DGLABClient", "enable_A", LOG_INFO, "关闭 A 通道");
        ui_.A_start_btn->setText("启动");
    }
    else {
        LOG_MODULE("DGLABClient", "enable_A", LOG_INFO, "启动 A 通道");
        ui_.A_start_btn->setText("关闭");

        QJsonObject test_cmd;
        test_cmd["cmd"] = "send_strength";
        test_cmd["channel"] = 1;
        test_cmd["mode"] = 1;
        test_cmd["value"] = 10;
        async_call(test_cmd, 5000, [this](bool ok, QString msg) {
            if (ok) {
                LOG_MODULE("DGLABClient", "enable_A", LOG_INFO, "测试命令发送成功: " << msg.toStdString());
            }
            else {
                LOG_MODULE("DGLABClient", "enable_A", LOG_ERROR, "测试命令发送失败: " << msg.toStdString());
            }
        });
    }
    is_A_start = !is_A_start;
    // 通道启用状态同步到规则引擎（启用时触发直连规则计算链）
    RuleManager::instance().set_channel_enabled("A", is_A_start);
}

void DGLABClient::enable_B() {
    if (is_B_start) {
        LOG_MODULE("DGLABClient", "enable_B", LOG_INFO, "关闭 B 通道");
        ui_.B_start_btn->setText("启动");
    }
    else {
        LOG_MODULE("DGLABClient", "enable_B", LOG_INFO, "启动 B 通道");
        ui_.B_start_btn->setText("关闭");
    }
    is_B_start = !is_B_start;
    // 通道启用状态同步到规则引擎（启用时触发直连规则计算链）
    RuleManager::instance().set_channel_enabled("B", is_B_start);
}

void DGLABClient::setup_channel_value_editor_input_validation() {
    QIntValidator* validator = new QIntValidator(0, 200, this);
    QLocale locale = QLocale::c();
    validator->setLocale(locale);
    ui_.A_strength_show_label->set_validator(validator);
    ui_.B_strength_show_label->set_validator(validator);
}

void DGLABClient::connect_about_channel_contral() const {
    LOG_MODULE("DGLABClient", "connect_about_channel_contral", LOG_DEBUG, "连接通道控制相关槽函数");
    connect(ui_.A_start_btn, &QPushButton::clicked, this, &DGLABClient::enable_A);
    connect(ui_.B_start_btn, &QPushButton::clicked, this, &DGLABClient::enable_B);
    connect(ui_.A_strength_show_label, &EditableLabel::text_edited, this, &DGLABClient::apply_A_strength);
    connect(ui_.B_strength_show_label, &EditableLabel::text_edited, this, &DGLABClient::apply_B_strength);
}

void DGLABClient::set_port_label_mode() {
    ui_.IP_label->set_editable(false);
}

void DGLABClient::refresh_connect_label() {
    ui_.port_label->setText(QString::number(port_cache_));
    ui_.connect_btn->setText(is_connected_ ? "断开" : "连接");
}

void DGLABClient::handle_connect() {
    LOG_MODULE("DGLABClient", "connect", LOG_DEBUG, "连接触发");
    if (connect_btn_loading_) {
        LOG_MODULE("DGLABClient", "connect", LOG_DEBUG, "忽略重复点击");
        return;
    }
    if (is_connected_) {
        LOG_MODULE("DGLABClient", "connect", LOG_INFO, "断开连接");
        connect_btn_loading_ = true;
        ui_.connect_btn->setEnabled(false);
        close_async_connect();
    }
    else {
        LOG_MODULE("DGLABClient", "connect", LOG_INFO, "正在连接");
        connect_btn_loading_ = true;
        ui_.connect_btn->setEnabled(false);
        start_async_connect();
    }
}

void DGLABClient::set_ip() {
    auto ip_selector = IpSelector::instance();
    QString selected = ip_selector->show_selection_dialog(this);
    ip_cache_ = selected.isEmpty() ? ip_cache_ : selected;
    auto& config = AppConfig::instance();
    config.set_value_with_name<std::string>("app.websocket.ip", ip_cache_.toStdString(), "system");
    refresh_ip_port_label();
}

void DGLABClient::refresh_ip_port_label() {
    auto& config = AppConfig::instance();
    int old_port = config.get_value<int>("app.websocket.port", 9999);
    port_cache_ = old_port;
    ui_.port_label->setText(QString::number(port_cache_));
    std::string old_ip = config.get_value<std::string>("app.websocket.ip", "127.0.0.1");
    ip_cache_ = QString::fromStdString(old_ip);
    ui_.IP_label->setText(ip_cache_);
}

void DGLABClient::setup_port_input_validation() {
    QIntValidator* validator = new QIntValidator(0, 65535, this);
    QLocale locale = QLocale::c();
    validator->setLocale(locale);
    ui_.port_label->set_validator(validator);
}

void DGLABClient::set_ip_port() {
    LOG_MODULE("DGLABClient", "set_port", LOG_DEBUG, "设置 IP 及端口");
    if (port_cache_ >= 0 && port_cache_ <= 65535) {
        LOG_MODULE("DGLABClient", "set_port", LOG_DEBUG, "开始设置端口");
        auto& config = AppConfig::instance();
        config.set_value_with_name<int>("app.websocket.port", port_cache_, "system");
        config.set_value_with_name<std::string>("app.websocket.ip", ip_cache_.toStdString(), "system");
        QString msg = "设置 IP 及端口: " + ip_cache_ + ":" + QString::number(port_cache_);
        QMessageBox::information(this, "信息更新完成！", msg);
        LOG_MODULE("DGLABClient", "set_port", LOG_INFO, msg.toStdString());
    }
    else {
        QMessageBox::warning(this, "端口设置失败！", "设置端口失败！非合法端口: " + QString::number(port_cache_));
        LOG_MODULE("DGLABClient", "set_port", LOG_WARN, "设置端口失败！非合法端口: " << port_cache_);
    }
    refresh_ip_port_label();
}

void DGLABClient::cache_port(const QString& input) {
    LOG_MODULE("DGLABClient", "cache_port", LOG_DEBUG, "缓存端口设置");
    if (input.isEmpty()) {
        QMessageBox::warning(this, "端口设置失败！", "端口号不能为空！");
        LOG_MODULE("DGLABClient", "cache_port", LOG_WARN, "设置端口失败！端口号为空");
    }
    else {
        bool ok;
        int port = input.toInt(&ok);
        if (ok && port >= 0 && port <= 65535) {
            port_cache_ = port;
        }
        else {
            QMessageBox::warning(this, "端口设置失败！", "设置端口失败！非合法端口: " + input);
            LOG_MODULE("DGLABClient", "cache_port", LOG_WARN, "设置端口失败！非合法端口: " << input.toStdString());
        }
    }
    ui_.port_label->setText(QString::number(port_cache_));
}

void DGLABClient::connect_about_connect() const {
    LOG_MODULE("DGLABClient", "connect_about_connect", LOG_DEBUG, "连接连接相关槽函数");
    connect(this, &DGLABClient::connect_finished, this, &DGLABClient::handle_connect_finished);
    connect(this, &DGLABClient::close_finished, this, &DGLABClient::handle_close_finished);
    connect(ui_.confirm_port_btn, &QPushButton::clicked, this, &DGLABClient::set_ip_port);
    connect(ui_.IP_label, &EditableLabel::double_clicked, this, &DGLABClient::set_ip);
    connect(ui_.port_label, &EditableLabel::text_edited, this, &DGLABClient::cache_port);
    connect(ui_.connect_btn, &QPushButton::clicked, this, &DGLABClient::handle_connect);
    connect(ui_.show_qr_btn, &QPushButton::clicked, this, &DGLABClient::show_qr_dialog);
}

void DGLABClient::refresh_theme_label() {}

void DGLABClient::connect_about_theme() const {
    connect(ui_.theme_list_btn, &QPushButton::clicked, this, &DGLABClient::show_theme_selector);
}

// ============================================
// 重写虚函数（protected）
// ============================================

void DGLABClient::closeEvent(QCloseEvent* event) {
    if (tray_icon_ && tray_icon_->isVisible()) {
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
            display = pad_right(tag1, TAG1_WIDTH) + " " + pad_right(tag2, TAG2_WIDTH) + " " + pad_right(tag3, TAG3_WIDTH) + ": " + msg;
        }
        else {
            display = tag1 + " " + tag2 + " " + tag3 + ": " + msg;
        }

        QMetaObject::invokeMethod(qptr.data(), [qptr, display, level]() {
                    if (!qptr) return;
                    qptr->append_log_message(display, level); }, Qt::AutoConnection);
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
        LogHighlighter(QTextDocument* doc)
            : QSyntaxHighlighter(doc) {}

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
        LOG_MODULE("DGLABClient", "create_tray_icon", LOG_ERROR, "托盘图标不存在！");
    }
}

void DGLABClient::load_stylesheet() {
    LOG_MODULE("DGLABClient", "load_stylesheet", LOG_DEBUG, "开始加载样式表");
    auto& config = AppConfig::instance();
    theme_ = mode_string_to_theme(config.get_value<std::string>("app.ui.theme", "light"));
    setup_widget_properties("theme", theme_to_mode_string(theme_).toStdString());
    LOG_MODULE("DGLABClient", "load_stylesheet", LOG_INFO, "当前样式: " + theme_to_mode_string(theme_).toStdString());

    QString qss_path = QStringLiteral(":/style/qcss/") + theme_to_mode_string(theme_) + QStringLiteral(".qcss");
    if (!QFile::exists(qss_path)) {
        qss_path = QStringLiteral(":/style/qcss/light.qcss");
    }

    QFile file(qss_path);
    if (file.open(QFile::ReadOnly)) {
        QString style = QString::fromUtf8(file.readAll());
        qApp->setStyleSheet(style);
        file.close();
    }
    else {
        LOG_MODULE("DGLABClient", "load_stylesheet", LOG_WARN,
            "无法加载样式表文件: " << qss_path.toStdString());
    }
    apply_inline_styles();

    QList<QWidget*> all_widgets = this->findChildren<QWidget*>();
    for (QWidget* widget : all_widgets) {
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
        widget->update();
    }

    LOG_MODULE("DGLABClient", "load_stylesheet", LOG_DEBUG, "样式表加载完成");
}

void DGLABClient::change_theme(const std::string& theme_str) {
    Theme new_theme = mode_string_to_theme(theme_str);
    if (new_theme == theme_) {
        LOG_MODULE("DGLABClient", "change_theme", LOG_DEBUG,
            "主题未改变，当前已是: " << theme_str);
        return;
    }

    theme_ = new_theme;
    LOG_MODULE("DGLABClient", "change_theme", LOG_INFO,
        "切换主题为: " << theme_str << " (枚举值: " << static_cast<int>(theme_) << ")");

    auto& config = AppConfig::instance();
    config.set_value<std::string>("app.ui.theme", theme_to_mode_string(theme_).toStdString());

    load_stylesheet();
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

void DGLABClient::setup_inline_style() {
}

void DGLABClient::refresh_style() {
    QList<QWidget*> widgets = this->findChildren<QWidget*>();
    for (QWidget* w : widgets) {
        w->style()->unpolish(w);
        w->style()->polish(w);
        w->update();
    }
}

void DGLABClient::setup_default_page() {
    ui_.center_pages->setCurrentWidget(ui_.main_page);
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
    LOG_MODULE("DGLABClient", "init_python_manager", LOG_INFO, "启动 Python 进程 -> [Python 解释器]路径: " << pythonPath.toStdString() << "（注: 若解释器路径直接为<Python>则使用系统默认 Python 路径）");
    LOG_MODULE("DGLABClient", "init_python_manager", LOG_INFO, "启动 Python 进程 -> [Python 服务模块]路径: " << bridge_module);
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

// ----- 模块页相关 -----
void DGLABClient::setup_module_ui() {
    // 初始化数值模块管理器（幂等，注册默认 CS2 GSI 模块并启动调度器）
    ModuleManager::instance().init();

    QVBoxLayout* page_layout = ui_.module_page_layout;
    page_layout->setContentsMargins(20, 20, 20, 20);
    page_layout->setSpacing(16);

    // 页面标题
    QLabel* title_label = new QLabel("数值模块", ui_.module_page);
    title_label->setProperty("type", "title");
    page_layout->addWidget(title_label);

    // 统一设置查询周期入口
    QHBoxLayout* period_row = new QHBoxLayout();
    period_row->setSpacing(10);
    QLabel* period_hint = new QLabel("统一设置查询周期:", ui_.module_page);
    module_period_combo_ = new QComboBox(ui_.module_page);
    module_period_combo_->addItem(QString::fromUtf8(query_period_to_text(QueryPeriod::SECOND)));
    module_period_combo_->addItem(QString::fromUtf8(query_period_to_text(QueryPeriod::TWO_SECONDS)));
    module_period_combo_->addItem(QString::fromUtf8(query_period_to_text(QueryPeriod::FOUR_SECONDS)));
    module_period_combo_->addItem(QString::fromUtf8(query_period_to_text(QueryPeriod::HALF_SECOND)));
    module_period_combo_->addItem(QString::fromUtf8(query_period_to_text(QueryPeriod::QUARTER_SECOND)));
    // 应用下拉框弹出样式处理（规避弹出列表边缘黑色）
    DGLABClientUtil::apply_combo_popup_style(module_period_combo_);
    // 默认选中当前基准周期
    int base_index = module_period_combo_->findText(QString::fromUtf8(
        query_period_to_text(query_period_from_ms(ModuleManager::instance().get_base_period_ms()))));
    if (base_index >= 0) {
        module_period_combo_->setCurrentIndex(base_index);
    }
    module_period_apply_btn_ = new QPushButton("应用", ui_.module_page);
    module_period_apply_btn_->setProperty("button_type", "special");
    period_row->addWidget(period_hint);
    period_row->addWidget(module_period_combo_);
    period_row->addWidget(module_period_apply_btn_);
    period_row->addStretch();
    page_layout->addLayout(period_row);

    // 模块卡片滚动区域（自适应布局，不设固定尺寸）
    QScrollArea* scroll_area = new QScrollArea(ui_.module_page);
    scroll_area->setWidgetResizable(true);
    scroll_area->setFrameShape(QFrame::NoFrame);
    module_cards_widget_ = new QWidget(scroll_area);
    QGridLayout* cards_grid = new QGridLayout(module_cards_widget_);
    cards_grid->setSpacing(15);
    cards_grid->setContentsMargins(6, 6, 6, 6);

    auto module_names = ModuleManager::instance().get_module_names();
    int card_row = 0;
    int card_col = 0;
    for (const auto& module_name : module_names) {
        create_module_card(QString::fromStdString(module_name), cards_grid, card_row, card_col);
        ++card_col;
        if (card_col >= 2) {
            card_col = 0;
            ++card_row;
        }
    }
    // 底部弹簧，卡片靠上排列
    cards_grid->setRowStretch(card_row + 1, 1);
    cards_grid->setColumnStretch(0, 1);
    cards_grid->setColumnStretch(1, 1);
    module_cards_widget_->setLayout(cards_grid);
    scroll_area->setWidget(module_cards_widget_);
    page_layout->addWidget(scroll_area, 1);

    // 连接统一周期应用按钮与周期变化刷新
    connect(module_period_apply_btn_, &QPushButton::clicked,
        this, &DGLABClient::apply_module_period_setting);
    connect(&ModuleManager::instance(), &ModuleManager::period_changed,
        this, &DGLABClient::refresh_module_cards);

    refresh_module_cards();
    LOG_MODULE("DGLABClient", "setup_module_ui", LOG_INFO, "模块页面初始化完成");
}

void DGLABClient::show_module_values(const QString& module_name) {
    // 已打开的弹窗直接置顶，避免重复创建
    if (module_values_dialog_) {
        module_values_dialog_->raise();
        module_values_dialog_->activateWindow();
        return;
    }
    module_values_dialog_ = new ModuleValuesDialog(module_name.toStdString(), this);
    module_values_dialog_->setAttribute(Qt::WA_DeleteOnClose);
    connect(module_values_dialog_, &QDialog::destroyed, this, [this]() {
        module_values_dialog_ = nullptr;
    });
    module_values_dialog_->show();
    LOG_MODULE("DGLABClient", "show_module_values", LOG_DEBUG,
        "弹出模块数值窗口: " << module_name.toStdString());
}

void DGLABClient::apply_module_period_setting() {
    if (!module_period_combo_) {
        return;
    }
    QueryPeriod period = query_period_from_text(
        module_period_combo_->currentText().toStdString());
    ModuleManager::instance().set_all_period(period);
    LOG_MODULE("DGLABClient", "apply_module_period_setting", LOG_INFO,
        "统一设置查询周期: " << query_period_to_text(period));
}

void DGLABClient::refresh_module_cards() {
    if (!module_cards_widget_) {
        return;
    }
    auto& manager = ModuleManager::instance();
    const QList<ModuleCard*> cards = module_cards_widget_->findChildren<ModuleCard*>();
    for (ModuleCard* card : cards) {
        int min_period_ms = manager.get_module_min_period_ms(
            card->get_module_name().toStdString());
        QueryPeriod min_period = query_period_from_ms(min_period_ms);
        card->set_period_text(QString("最小查询周期: %1")
                .arg(QString::fromUtf8(query_period_to_text(min_period))));
    }
}

void DGLABClient::create_module_card(const QString& module_name, QGridLayout* layout,
    int row, int col) {
    ModuleCard* card = new ModuleCard(module_name, module_cards_widget_);
    // 点击卡片弹出该模块的数值展示窗口
    connect(card, &ModuleCard::clicked, this, [this, module_name]() {
        show_module_values(module_name);
    });
    layout->addWidget(card, row, col);
}

// ----- 首页通道卡片相关 -----
void DGLABClient::setup_channel_cards() {
    // 模块/规则/波形卡片宽度 1:1:1 等分（防止长内容压缩波形卡片）
    const int stretch = CHANNEL_CARD_WIDTH_STRETCH;
    ui_.A_normal_cards_layout->setStretch(0, stretch);
    ui_.A_normal_cards_layout->setStretch(1, stretch);
    ui_.A_normal_cards_layout->setStretch(2, stretch);
    ui_.B_normal_cards_layout->setStretch(0, stretch);
    ui_.B_normal_cards_layout->setStretch(1, stretch);
    ui_.B_normal_cards_layout->setStretch(2, stretch);
    // 填充 A/B 通道的模块区域与规则区域
    populate_channel_module_card(ui_.A_module_card_layout, "A");
    populate_channel_rule_card(ui_.A_rule_card_layout, "A");
    populate_channel_module_card(ui_.B_module_card_layout, "B");
    populate_channel_rule_card(ui_.B_rule_card_layout, "B");
    // 规则结果变化时刷新对应通道的规则卡片数值
    connect(&RuleManager::instance(), &RuleManager::rule_result_changed,
        this, &DGLABClient::refresh_channel_rule_cards);
    LOG_MODULE("DGLABClient", "setup_channel_cards", LOG_INFO, "首页通道卡片初始化完成");
}

void DGLABClient::populate_channel_module_card(QVBoxLayout* layout, const std::string& channel) {
    // 卡片内容边距与间距（关键布局参数见头文件常量）
    layout->setContentsMargins(CHANNEL_CARD_CONTENT_MARGIN, CHANNEL_CARD_CONTENT_MARGIN,
        CHANNEL_CARD_CONTENT_MARGIN, CHANNEL_CARD_CONTENT_MARGIN);
    layout->setSpacing(CHANNEL_CARD_CONTENT_SPACING);

    // 标题
    QLabel* title = new QLabel("模块", ui_.A_module_card);
    title->setProperty("type", "channel_card_title");
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    // 挂载在该通道上的模块：名称与最小查询周期分行居中显示
    auto& module_manager = ModuleManager::instance();
    for (const auto& module_name : module_manager.get_modules_for_channel(channel)) {
        int min_period_ms = module_manager.get_module_min_period_ms(module_name);
        // 每条信息外层包含子卡片，凸显内容（类似 A_strength_card 在 A_info 中）
        QWidget* info_card = new QWidget(ui_.A_module_card);
        info_card->setProperty("type", "channel_info_card");
        QVBoxLayout* info_layout = new QVBoxLayout(info_card);
        info_layout->setContentsMargins(CHANNEL_INFO_CARD_MARGIN, CHANNEL_INFO_CARD_MARGIN,
            CHANNEL_INFO_CARD_MARGIN, CHANNEL_INFO_CARD_MARGIN);
        info_layout->setSpacing(CHANNEL_INFO_CARD_SPACING);

        QLabel* name_label = new QLabel(QString::fromStdString(module_name), info_card);
        name_label->setProperty("type", "channel_card_item");
        name_label->setAlignment(Qt::AlignCenter);
        name_label->setWordWrap(true);
        QLabel* period_label = new QLabel(QString("最小周期 %1ms").arg(min_period_ms), info_card);
        period_label->setProperty("type", "channel_card_period");
        period_label->setAlignment(Qt::AlignCenter);
        info_layout->addWidget(name_label);
        info_layout->addWidget(period_label);
        layout->addWidget(info_card);
    }
    // 底部弹簧，内容靠上排列
    layout->addStretch();
}

void DGLABClient::populate_channel_rule_card(QVBoxLayout* layout, const std::string& channel) {
    // 卡片内容边距与间距（关键布局参数见头文件常量）
    layout->setContentsMargins(CHANNEL_CARD_CONTENT_MARGIN, CHANNEL_CARD_CONTENT_MARGIN,
        CHANNEL_CARD_CONTENT_MARGIN, CHANNEL_CARD_CONTENT_MARGIN);
    layout->setSpacing(CHANNEL_CARD_CONTENT_SPACING);

    // 标题
    QLabel* title = new QLabel("规则", ui_.A_rule_card);
    title->setProperty("type", "channel_card_title");
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    // 父级为该通道的规则：名称 + 最近一次计算的数值
    auto& rule_manager = RuleManager::instance();
    for (const auto& rule_name : rule_manager.get_rule_names()) {
        auto parents = rule_manager.get_rule_parents(rule_name);
        bool has_channel = false;
        for (const auto& parent : parents) {
            if (parent.type == ParentType::CHANNEL && parent.channel == channel) {
                has_channel = true;
                break;
            }
        }
        if (!has_channel) {
            continue;
        }
        // 每条规则信息外层包含子卡片，凸显内容
        QWidget* info_card = new QWidget(ui_.A_rule_card);
        info_card->setProperty("type", "channel_info_card");
        QHBoxLayout* info_layout = new QHBoxLayout(info_card);
        info_layout->setContentsMargins(CHANNEL_INFO_CARD_MARGIN, CHANNEL_INFO_CARD_MARGIN,
            CHANNEL_INFO_CARD_MARGIN, CHANNEL_INFO_CARD_MARGIN);
        info_layout->setSpacing(CHANNEL_INFO_CARD_SPACING);
        QLabel* name_label = new QLabel(QString::fromStdString(rule_name), info_card);
        name_label->setProperty("type", "channel_card_item");
        name_label->setWordWrap(true);
        // 忽略宽度提示，避免长名称撑宽卡片（由 1:1:1 布局分配宽度）
        name_label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        auto last_result = rule_manager.get_rule_last_result(rule_name);
        QLabel* value_label = new QLabel(
            last_result.has_value() ? QString::number(last_result.value()) : QString("--"), info_card);
        value_label->setProperty("type", "channel_card_value");
        value_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        info_layout->addWidget(name_label, 1);
        info_layout->addWidget(value_label);
        layout->addWidget(info_card);
        // 记录数值标签，供结果变化时刷新
        rule_value_labels_[channel + ":" + rule_name] = value_label;
    }
    // 底部弹簧，内容靠上排列
    layout->addStretch();
}

void DGLABClient::refresh_channel_rule_cards(const QString& rule_name, const QString& channel,
    int value) {
    // 更新对应通道规则卡片中的最近计算值
    auto it = rule_value_labels_.find(channel.toStdString() + ":" + rule_name.toStdString());
    if (it != rule_value_labels_.end()) {
        it->second->setText(QString::number(value));
    }
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

// ----- 规则 UI 相关 -----
void DGLABClient::setup_rules_ui() {
    QLayout* oldLayout = ui_.rules_list->layout();
    if (oldLayout) delete oldLayout;
    QVBoxLayout* layout = new QVBoxLayout(ui_.rules_list);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    // 文件选择区域
    QHBoxLayout* fileLayout = new QHBoxLayout();
    fileLayout->addWidget(new QLabel("规则文件:"));
    rule_file_btn_ = new QToolButton(ui_.rules_list);
    rule_file_btn_->setObjectName("rule_file_btn");
    rule_file_btn_->setPopupMode(QToolButton::InstantPopup);
    rule_file_btn_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    rule_file_btn_->setAutoRaise(false);
    rule_file_btn_->setFocusPolicy(Qt::NoFocus);
    rule_file_menu_ = new QMenu(rule_file_btn_);
    rule_file_btn_->setMenu(rule_file_menu_);
    connect(rule_file_menu_, &QMenu::triggered, this, &DGLABClient::on_rule_file_selected);
    fileLayout->addWidget(rule_file_btn_);
    create_file_btn_ = new QPushButton("新建");
    delete_file_btn_ = new QPushButton("删除");
    save_file_btn_ = new QPushButton("保存");
    fileLayout->addWidget(create_file_btn_);
    fileLayout->addWidget(delete_file_btn_);
    fileLayout->addWidget(save_file_btn_);
    layout->addLayout(fileLayout);

    // 规则表格（启用、规则名称、父级、模式、值模式）
    rule_table_ = new QTableWidget();
    rule_table_->setColumnCount(5);
    rule_table_->setHorizontalHeaderLabels({"启用", "规则名称", "父级", "模式", "值模式"});
    rule_table_->horizontalHeader()->setStretchLastSection(true);

    QStringList channelOptions = {"A", "B", "无"};
    QStringList modeOptions = {"递减", "递增", "设为", "连减", "连增"};

    rule_table_->setItemDelegateForColumn(2, new ComboBoxDelegate(channelOptions, rule_table_));
    rule_table_->setItemDelegateForColumn(3, new ComboBoxDelegate(modeOptions, rule_table_));
    rule_table_->setItemDelegateForColumn(4, new ValueModeDelegate(rule_table_));
    // 列宽策略：启用/父级/模式按内容（宽度可预测），规则名称按最长名称（有最大宽度），值模式占剩余
    QHeaderView* rule_header = rule_table_->horizontalHeader();
    rule_header->setStretchLastSection(false);
    rule_header->setSectionResizeMode(0, QHeaderView::ResizeToContents); // 启用
    rule_header->setSectionResizeMode(1, QHeaderView::Interactive);      // 规则名称
    rule_header->setSectionResizeMode(2, QHeaderView::ResizeToContents); // 父级
    rule_header->setSectionResizeMode(3, QHeaderView::ResizeToContents); // 模式
    rule_header->setSectionResizeMode(4, QHeaderView::Stretch);          // 值模式（剩余宽度）
    // 增大默认行高，避免编辑时输入框字体显示不全
    rule_table_->verticalHeader()->setDefaultSectionSize(30);
    rule_table_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::AnyKeyPressed);
    rule_table_->viewport()->update();
    // 父级列编辑后同步到规则管理器（含通道唯一性去重）
    connect(rule_table_, &QTableWidget::itemChanged,
        this, &DGLABClient::on_rule_table_item_changed);

    layout->addWidget(rule_table_);

    // 操作按钮
    QHBoxLayout* btnLayout = new QHBoxLayout();
    add_rule_btn_ = new QPushButton("添加规则");
    edit_rule_btn_ = new QPushButton("编辑规则");
    edit_parents_btn_ = new QPushButton("编辑父级");
    delete_rule_btn_ = new QPushButton("删除规则");
    btnLayout->addWidget(add_rule_btn_);
    btnLayout->addWidget(edit_rule_btn_);
    btnLayout->addWidget(edit_parents_btn_);
    btnLayout->addWidget(delete_rule_btn_);
    layout->addLayout(btnLayout);

    // 连接信号
    connect(create_file_btn_, &QPushButton::clicked, this, &DGLABClient::on_create_rule_file);
    connect(delete_file_btn_, &QPushButton::clicked, this, &DGLABClient::on_delete_rule_file);
    connect(save_file_btn_, &QPushButton::clicked, this, &DGLABClient::on_save_rule_file);
    connect(add_rule_btn_, &QPushButton::clicked, this, &DGLABClient::on_add_rule);
    connect(edit_rule_btn_, &QPushButton::clicked, this, &DGLABClient::on_edit_rule);
    connect(edit_parents_btn_, &QPushButton::clicked, this, &DGLABClient::on_edit_parents);
    connect(delete_rule_btn_, &QPushButton::clicked, this, &DGLABClient::on_delete_rule);

    refresh_rule_file_list();
    update_rule_table();

    add_rule_btn_->setProperty("button_type", "special");
    edit_rule_btn_->setProperty("button_type", "special");
    edit_parents_btn_->setProperty("button_type", "special");
    delete_rule_btn_->setProperty("button_type", "emphasis");
    rule_table_->horizontalHeader()->setProperty("type", "table_header");
    rule_table_->setAttribute(Qt::WA_StyledBackground, true);
    rule_table_->horizontalHeader()->setAttribute(Qt::WA_StyledBackground, true);
    rule_table_->verticalHeader()->setAttribute(Qt::WA_StyledBackground, true);
    LOG_MODULE("DGLABClient", "setup_rules_ui", LOG_INFO, "规则UI初始化完成");
}

void DGLABClient::connect_rule_engine() {
    // 规则计算完成且父级为通道时，将命令发送给 Python 端
    connect(&RuleManager::instance(), &RuleManager::rule_command_ready,
        this, [this](const QJsonObject& cmd) {
            if (!is_connected_) {
                LOG_MODULE("DGLABClient", "connect_rule_engine", LOG_WARN,
                    "未连接 Python 服务，规则命令未发送");
                return;
            }
            async_call(cmd, 5000, [this](bool ok, QString msg) {
                if (!ok) {
                    LOG_MODULE("DGLABClient", "connect_rule_engine", LOG_ERROR,
                        "规则命令发送失败: " << msg.toStdString());
                }
                else {
                    LOG_MODULE("DGLABClient", "connect_rule_engine", LOG_DEBUG,
                        "规则命令已发送: " << msg.toStdString());
                }
            });
        });
    LOG_MODULE("DGLABClient", "connect_rule_engine", LOG_INFO, "规则引擎信号连接完成");
}

void DGLABClient::refresh_rule_file_list() {
    auto& rm = RuleManager::instance();
    auto files = rm.get_available_rule_files();
    rule_file_menu_->clear();
    QAction* defaultAction = rule_file_menu_->addAction("rules.json");
    defaultAction->setData("rules.json");
    for (const auto& file : files) {
        QAction* action = rule_file_menu_->addAction(QString::fromStdString(file));
        action->setData(QString::fromStdString(file));
    }
    QString current = QString::fromStdString(rm.get_current_rule_file());
    rule_file_btn_->setText(current);
}

void DGLABClient::update_rule_table() {
    updating_rule_table_ = true;
    auto& rm = RuleManager::instance();
    auto names = rm.get_rule_names();
    rule_table_->setRowCount((int)names.size());
    bool light_text = theme_text_is_light();
    // 主题文本为黑时：不适用=浅灰、部分不适用=深灰；为白时反之
    QColor not_applicable_color = light_text ? QColor(110, 110, 110) : QColor(165, 165, 165);
    QColor partial_color = light_text ? QColor(165, 165, 165) : QColor(110, 110, 110);
    // 规则名称列宽：依据最长规则名称设置（限制最大宽度防止过宽）
    int max_name_width = 60;
    QFontMetrics name_fm(rule_table_->font());
    for (const auto& rule_name : names) {
        int width = name_fm.horizontalAdvance(QString::fromStdString(rule_name)) + 24;
        max_name_width = std::max(max_name_width, width);
    }
    max_name_width = std::min(max_name_width, 200);
    rule_table_->setColumnWidth(1, max_name_width);

    for (size_t i = 0; i < names.size(); ++i) {
        const auto& name = names[i];
        // 启用列（勾选框）
        QTableWidgetItem* enabled_item = new QTableWidgetItem();
        enabled_item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        enabled_item->setCheckState(rm.get_rule_enabled(name) ? Qt::Checked : Qt::Unchecked);
        enabled_item->setTextAlignment(Qt::AlignCenter);
        rule_table_->setItem(i, 0, enabled_item);

        // 规则名称列
        rule_table_->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(name)));

        // 父级列显示（通道在前，规则在后，无父级显示"无"）
        std::string parents_display = rm.get_rule_parents_display(name);
        rule_table_->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(parents_display)));

        // 模式列（父级不是通道时灰显）
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
        QTableWidgetItem* mode_item = new QTableWidgetItem(modeStr);
        int applicability = rm.get_rule_mode_applicability(name);
        if (applicability == 1) {
            // 父级无通道：文本后加"(不适用)"，整个文本灰色
            mode_item->setText(modeStr + "(不适用)");
            mode_item->setForeground(QBrush(not_applicable_color));
        }
        else if (applicability == 2) {
            // 父级混合：文本后加"(部分不适用)"，整个文本更接近主题文本色的灰色
            mode_item->setText(modeStr + "(部分不适用)");
            mode_item->setForeground(QBrush(partial_color));
        }
        rule_table_->setItem(i, 3, mode_item);

        // 值模式列（规则文件内容不变，仅改变应用中的显示）
        std::string pattern = rm.get_rule_value_pattern(name);
        QString displayPattern = QString::fromStdString(pattern);
        displayPattern.replace("{}", "{   }");
        rule_table_->setItem(i, 4, new QTableWidgetItem(displayPattern));
    }
    updating_rule_table_ = false;
}

void DGLABClient::on_rule_table_item_changed(QTableWidgetItem* item) {
    if (updating_rule_table_ || !item) {
        return;
    }
    QTableWidgetItem* name_item = rule_table_->item(item->row(), 1);
    if (!name_item || name_item->text().isEmpty()) {
        return;
    }
    QString name = name_item->text();
    if (item->column() == 0) {
        // 启用列：勾选状态同步到规则管理器
        RuleManager::instance().set_rule_enabled(name.toStdString(),
            item->checkState() == Qt::Checked);
        LOG_MODULE("DGLABClient", "on_rule_table_item_changed", LOG_DEBUG,
            "规则 " << name.toStdString() << " 启用状态: "
                    << (item->checkState() == Qt::Checked ? "启用" : "停用"));
    }
    else if (item->column() == 2) {
        // 父级列编辑（通道唯一性：保留用户最后设置的）
        QString parent_text = item->text();
        QString channel = (parent_text == "A" || parent_text == "B") ? parent_text : "";
        RuleManager::instance().set_rule_channel(name.toStdString(), channel.toStdString());
        LOG_MODULE("DGLABClient", "on_rule_table_item_changed", LOG_DEBUG,
            "规则 " << name.toStdString() << " 父级设置为: "
                    << (channel.isEmpty() ? "无" : channel.toStdString()));
        // 通道唯一性可能清除其他行的通道父级，刷新显示
        update_rule_table();
    }
}

// ----- 样式辅助 -----
void DGLABClient::setup_widget_properties(const std::string& property, const std::string& key) {
    LOG_MODULE("DGLABClient", "setup_widget_properties", LOG_DEBUG, "开始设置元素属性");
    LOG_MODULE("DGLABClient", "setup_widget_properties", LOG_DEBUG, "设置元素统一属性[" << property << "]为: " << key);

    // 为所有需要动态属性的控件统一设置
    QList<QWidget*> all_widgets = ui_.all->findChildren<QWidget*>();
    all_widgets.append(ui_.all);
    all_widgets.append(this);
    for (QWidget* w : all_widgets) {
        w->setProperty(property.c_str(), key.c_str());
    }

    apply_widget_properties();

    LOG_MODULE("DGLABClient", "setup_widget_properties", LOG_DEBUG, "设置元素属性完成！");
}

void DGLABClient::apply_widget_properties() {
    ui_.all->setProperty("type", "main_page");

    // ========== 设置半透玻璃面板 ==========
    // 导航栏
    ui_.navigation_bar->setProperty("type", "glass_panel");
    // 通道信息卡片
    ui_.A_info->setProperty("type", "glass_panel");
    ui_.B_info->setProperty("type", "glass_panel");
    // 通道模块/规则/波形卡片
    ui_.A_module_card->setProperty("type", "glass_panel");
    ui_.B_module_card->setProperty("type", "glass_panel");
    ui_.A_rule_card->setProperty("type", "glass_panel");
    ui_.B_rule_card->setProperty("type", "glass_panel");
    ui_.A_wave_card->setProperty("type", "glass_panel");
    ui_.B_wave_card->setProperty("type", "glass_panel");
    // 配置页各个面板
    ui_.port_bar->setProperty("type", "glass_panel");
    ui_.debug_log_card->setProperty("type", "glass_panel");
    ui_.theme_card->setProperty("type", "glass_panel");
    ui_.wave_card->setProperty("type", "glass_panel");
    ui_.config_A_wave_card->setProperty("type", "glass_panel_inner");
    ui_.config_B_wave_card->setProperty("type", "glass_panel_inner");
    ui_.theme_one_card->setProperty("type", "glass_panel_inner");
    ui_.theme_two_card->setProperty("type", "glass_panel_inner");

    // 内层子卡片
    ui_.A_strength_card->setProperty("type", "glass_panel_inner");
    ui_.B_strength_card->setProperty("type", "glass_panel_inner");

    // 页面切换按钮 (type="nav_btn")
    ui_.main_page_btn->setProperty("type", "nav_btn");
    ui_.config_page_btn->setProperty("type", "nav_btn");
    ui_.rule_page_btn->setProperty("type", "nav_btn");
    ui_.module_page_btn->setProperty("type", "nav_btn");
    ui_.about_page_btn->setProperty("type", "nav_btn");

    // 特殊功能按钮 (button_type="special") 金色系
    ui_.minimize_btn->setProperty("button_type", "special");
    ui_.connect_btn->setProperty("button_type", "special");
    ui_.confirm_wave_btn->setProperty("button_type", "special");
    ui_.theme_list_btn->setProperty("button_type", "special");
    ui_.creat_theme_btn->setProperty("button_type", "special");
    ui_.export_log_btn->setProperty("button_type", "special");
    ui_.confirm_port_btn->setProperty("button_type", "special");
    ui_.show_qr_btn->setProperty("button_type", "special");
    ui_.A_start_btn->setProperty("button_type", "special");
    ui_.B_start_btn->setProperty("button_type", "special");
    ui_.creat_wave_btn->setProperty("button_type", "special");
    ui_.more_log_setting_btn->setProperty("button_type", "special");

    // 强调按钮 (button_type="emphasis") 红色系
    ui_.close_btn->setProperty("button_type", "emphasis");

    // 设置字体大小属性
    ui_.app_icon->setProperty("font_size", "L");
    ui_.A_strength_show_label->setProperty("font_size", "M");
    ui_.B_strength_show_label->setProperty("font_size", "M");
    ui_.debug_log->setProperty("font_size", "S");
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
        {PRUSSIAN_BLUE_FOG, "rgba(255, 255, 255, 255)"}};

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

bool DGLABClient::theme_text_is_light() const {
    // 与 apply_inline_styles 中主题字体颜色映射保持一致
    switch (theme_) {
    case NIGHT:
    case CHARCOAL_PINK:
    case DEEPSEA_CREAM:
    case DARK_BLUE_CLEAR_BLUE:
    case KLEIN_YELLOW:
    case MARS_GREEN_ROSE:
    case HERMES_ORANGE_NAVY:
    case CHINA_RED_YELLOW:
    case VANDYKE_BROWN_KHAKI:
    case PRUSSIAN_BLUE_FOG:
        return true;
    default:
        return false;
    }
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
        {PRUSSIAN_BLUE_FOG, QStringLiteral("prussian_blue_fog")}};
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
        {QLatin1String("prussian_blue_fog"), PRUSSIAN_BLUE_FOG}};
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
        {PRUSSIAN_BLUE_FOG, QStringLiteral("普鲁士雾灰")}};
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
        {QStringLiteral("普鲁士雾灰"), PRUSSIAN_BLUE_FOG}};
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

void DGLABClient::refresh_channel_strength() {
    ui_.A_strength_show_label->setText(QString::number(A_strength_));
    ui_.B_strength_show_label->setText(QString::number(B_strength_));
}

void DGLABClient::apply_A_strength(const QString& new_strength) {
    QString text = new_strength;
    bool ok;
    int newVal = text.toInt(&ok);
    if (!ok || newVal < 0 || newVal > 200) {
        refresh_channel_strength();
        return;
    }
    if (newVal == A_strength_) {
        return;
    }
    A_strength_ = newVal;
    refresh_channel_strength();

    if (is_connected_) {
        QJsonObject cmd;
        cmd["cmd"] = "set_strength";
        cmd["channel"] = "A";
        cmd["mode"] = 2;
        cmd["value"] = A_strength_;
        async_call(cmd, 5000, [this](bool ok, QString msg) {
            if (ok) {
                LOG_MODULE("DGLABClient", "apply_A_strength", LOG_INFO,
                    "A通道强度设置成功: " << msg.toStdString());
            }
            else {
                LOG_MODULE("DGLABClient", "apply_A_strength", LOG_ERROR,
                    "A通道强度设置失败: " << msg.toStdString());
                QMessageBox::warning(this, "错误", "设置A通道强度失败: " + msg);
            }
        });
    }
    else {
        LOG_MODULE("DGLABClient", "apply_A_strength", LOG_WARN, "未连接，无法设置A通道强度");
        A_strength_ = 0;
        refresh_channel_strength();
    }
}

void DGLABClient::apply_B_strength(const QString& new_strength) {
    QString text = new_strength;
    bool ok;
    int newVal = text.toInt(&ok);
    if (!ok || newVal < 0 || newVal > 200) {
        refresh_channel_strength();
        return;
    }
    if (newVal == B_strength_) {
        return;
    }
    B_strength_ = newVal;
    refresh_channel_strength();
    if (is_connected_) {
        QJsonObject cmd;
        cmd["cmd"] = "set_strength";
        cmd["channel"] = "B";
        cmd["mode"] = 2;
        cmd["value"] = B_strength_;
        async_call(cmd, 5000, [this](bool ok, QString msg) {
            if (ok) {
                LOG_MODULE("DGLABClient", "apply_B_strength", LOG_INFO,
                    "B通道强度设置成功: " << msg.toStdString());
            }
            else {
                LOG_MODULE("DGLABClient", "apply_B_strength", LOG_ERROR,
                    "B通道强度设置失败: " << msg.toStdString());
                QMessageBox::warning(this, "错误", "设置B通道强度失败: " + msg);
            }
        });
    }
    else {
        LOG_MODULE("DGLABClient", "apply_B_strength", LOG_WARN, "未连接，无法设置B通道强度");
        B_strength_ = 0;
        refresh_channel_strength();
    }
}

// ============================================
// private slots 实现
// ============================================

void DGLABClient::handle_connect_finished(bool success, const QString& msg) {
    connect_btn_loading_ = false;
    ui_.connect_btn->setEnabled(true);
    if (success) {
        is_connected_ = true;
        ui_.connect_btn->setText("断开");
        LOG_MODULE("DGLABClient", "handle_connect_finished", LOG_INFO, msg.toStdString());
        reset_py_log_level();
    }
    else {
        QMessageBox::warning(this, "连接错误！", msg);
        LOG_MODULE("DGLABClient", "handle_connect_finished", LOG_ERROR, msg.toStdString());
    }
}

void DGLABClient::handle_close_finished(bool success, const QString& msg) {
    connect_btn_loading_ = false;
    ui_.connect_btn->setEnabled(true);
    if (success) {
        is_connected_ = false;
        ui_.connect_btn->setText("连接");
        LOG_MODULE("DGLABClient", "handle_close_finished", LOG_INFO, msg.toStdString());
    }
    else {
        QMessageBox::warning(this, "断开连接错误！", msg);
        LOG_MODULE("DGLABClient", "handle_close_finished", LOG_ERROR, msg.toStdString());
    }
    delete_old_qr_file();
}

void DGLABClient::start_async_connect() {
    LOG_MODULE("DGLABClient", "start_async_connect", LOG_INFO, "正在获取本机IP并更新WebSocket地址");
    auto& config = AppConfig::instance();
    int port = config.get_value<int>("app.websocket.port", 9999);
    QString localIp = get_ip_cache_();
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
    change_theme(theme_to_mode_string(theme).toStdString());
}

void DGLABClient::on_export_log() {
    LOG_MODULE("DGLABClient", "on_export_log", LOG_INFO, "开始导出日志");
    // 导出前重新加载设置，确保最新持久化配置即时生效
    log_exporter_.load_settings();
    QString error;
    if (log_exporter_.export_log(ui_.debug_log->toPlainText(), &error)) {
        QMessageBox::information(this, "导出日志",
            "日志已导出到: " + log_exporter_.manual_dir_absolute());
    }
    else {
        QMessageBox::warning(this, "导出日志失败", error.isEmpty() ? "未知错误" : error);
    }
}

void DGLABClient::on_more_log_setting() {
    LOG_MODULE("DGLABClient", "on_more_log_setting", LOG_DEBUG, "打开日志导出设置对话框");
    LogExportSettingsDialog dlg(log_exporter_.auto_settings(), log_exporter_.manual_settings(), this);
    if (dlg.exec() == QDialog::Accepted) {
        log_exporter_.set_auto_settings(dlg.get_auto_settings());
        log_exporter_.set_manual_settings(dlg.get_manual_settings());
        log_exporter_.save_settings();
        // 自动日志设置变化后重启自动日志（使用新设置）
        log_exporter_.stop_auto_log();
        log_exporter_.start_auto_log();
        QMessageBox::information(this, "设置完成", "日志导出设置已保存");
    }
}

void DGLABClient::on_rule_file_selected(QAction* action) {
    if (!action) return;
    QString filename = action->data().toString();
    if (filename.isEmpty())
        filename = action->text();
    rule_file_btn_->setText(filename);
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
        "请输入文件名（不含.json，但需要包含关键字: " + keyword + "，否则会自动添加）:",
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
        QString currentFile = name;
        rule_file_btn_->setText(currentFile);
        RuleManager::instance().load_rule_file(currentFile.toStdString());
        update_rule_table();
        QMessageBox::information(this, "提示", "文件创建成功: " + name);
    }
    else {
        QMessageBox::warning(this, "错误", "创建文件失败");
    }
}

void DGLABClient::on_delete_rule_file() {
    QString filename = rule_file_btn_->text();
    if (filename == "rules.json") {
        QMessageBox::warning(this, "错误", "不能删除默认规则文件");
        return;
    }
    int ret = QMessageBox::question(this, "确认", "确定要删除规则文件 " + filename + " 吗？");
    if (ret == QMessageBox::Yes) {
        auto& rm = RuleManager::instance();
        if (rm.delete_rule_file(filename.toStdString())) {
            refresh_rule_file_list();

            if (rm.get_current_rule_file() != filename.toStdString()) {
                rm.load_rule_file("rules.json");
                rule_file_btn_->setText("rules.json");
            }
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

    QStringList channels = {"无", "A", "B"};
    QString channel = QInputDialog::getItem(this, "选择通道", "通道:", channels, 0, false, &ok);
    if (!ok) return;
    QString channelStr = (channel == "无") ? "" : channel;

    QStringList modes = {"递减", "递增", "设为", "连减", "连增"};
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
        nlohmann::json parents_json = nlohmann::json::array();
        if (!channelStr.isEmpty()) {
            parents_json.push_back(channelStr.toStdString());
        }
        j["rules"][name.toStdString()] = {
            {"enabled", true},
            {"parents", parents_json},
            {"mode", mode},
            {"valuePattern", valuePattern.toStdString()}};
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
    QString name = rule_table_->item(row, 1)->text();
    auto& rm = RuleManager::instance();

    QString oldChannel = QString::fromStdString(rm.get_rule_channel(name.toStdString()));
    if (oldChannel.isEmpty()) oldChannel = "无";
    int oldMode = rm.get_rule_mode(name.toStdString());
    QString oldPattern = QString::fromStdString(rm.get_rule_value_pattern(name.toStdString()));

    bool ok;
    QStringList channels = {"无", "A", "B"};
    QString channel = QInputDialog::getItem(this, "编辑规则", "通道:", channels,
        channels.indexOf(oldChannel), false, &ok);
    if (!ok) return;
    QString channelStr = (channel == "无") ? "" : channel;

    QStringList modes = {"递减", "递增", "设为", "连减", "连增"};
    QString modeStr = QInputDialog::getItem(this, "编辑规则", "模式:", modes, oldMode, false, &ok);
    if (!ok) return;
    int mode = modes.indexOf(modeStr);

    FormulaBuilderDialog dlg(oldPattern, this, rm.get_rule_index(name.toStdString()));
    if (dlg.exec() != QDialog::Accepted) return;
    QString newPattern = dlg.get_formula();
    if (newPattern.isEmpty()) return;

    std::string currentFile = rm.get_current_rule_file();
    try {
        nlohmann::json j = rm.load_json_file(currentFile);
        if (!j.contains("rules")) j["rules"] = nlohmann::json::object();
        nlohmann::json parents_json = nlohmann::json::array();
        if (!channelStr.isEmpty()) {
            parents_json.push_back(channelStr.toStdString());
        }
        j["rules"][name.toStdString()] = {
            {"enabled", rm.get_rule_enabled(name.toStdString())},
            {"parents", parents_json},
            {"mode", mode},
            {"valuePattern", newPattern.toStdString()}};
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

void DGLABClient::on_edit_parents() {
    int row = rule_table_->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, "提示", "请先选择要编辑的规则");
        return;
    }
    QString name = rule_table_->item(row, 1)->text();
    auto& rm = RuleManager::instance();
    if (rm.get_rule_index(name.toStdString()) <= 0) {
        return;
    }

    ParentEditDialog dlg(name.toStdString(), this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    // 应用通道父级
    rm.set_rule_channel(name.toStdString(), dlg.get_channel());
    // 应用规则父级：对比当前引用与目标勾选，增删对应规则值模式中的 {rule:本规则}
    int self_index = rm.get_rule_index(name.toStdString());
    auto current_refs = rm.get_rule_parent_rules(name.toStdString());
    std::vector<int> target = dlg.get_selected_rules();
    for (int target_index : target) {
        if (std::find(current_refs.begin(), current_refs.end(), target_index) == current_refs.end()) {
            std::string target_name = rm.get_rule_name_by_index(target_index);
            if (!target_name.empty()) {
                rm.add_rule_reference(target_name, self_index);
            }
        }
    }
    for (int current_index : current_refs) {
        if (std::find(target.begin(), target.end(), current_index) == target.end()) {
            std::string current_name = rm.get_rule_name_by_index(current_index);
            if (!current_name.empty()) {
                rm.remove_rule_reference(current_name, self_index);
            }
        }
    }

    // 持久化到规则文件并刷新表格
    if (rm.save_current_rule_file()) {
        LOG_MODULE("DGLABClient", "on_edit_parents", LOG_INFO,
            "规则 " << name.toStdString() << " 父级编辑完成并已保存");
    }
    update_rule_table();
}

void DGLABClient::on_delete_rule() {
    int row = rule_table_->currentRow();
    if (row < 0) return;
    QString name = rule_table_->item(row, 1)->text();
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
    // 消息格式: { "type": "active_message", "data": {...} }
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
                        refresh_channel_strength();
                        LOG_MODULE("DGLABClient", "on_active_message_received", LOG_DEBUG,
                            "A通道强度更新: " << A_strength_ << " (原值: " << A_strength_temp_ << ")");
                    }
                    int B_strength_temp_ = qBound(0, bStr, 200);
                    if (B_strength_temp_ != B_strength_) {
                        B_strength_ = B_strength_temp_;
                        refresh_channel_strength();
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

#include "DGLABClient.moc"
