#pragma once

#include "DebugLog.h"
#include "PythonSubprocessManager.h"

#include <QtWidgets/QWidget>
#include "ui_DGLABClient.h"
#include <QSyntaxHighlighter>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QCloseEvent>
#include <QTableWidget>

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

    void on_start_connect_btn_clicked();
    void on_close_connect_btn_clicked();
    void on_start_btn_clicked();
    void on_close_btn_clicked();

    void change_ui_log_level();
    void set_port();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    Ui::DGLABClientClass ui;
    void append_log_message(const QString& message, int level);
    void append_colored_text(QTextEdit* edit, const QString& text);
    QSyntaxHighlighter* log_highlighter = nullptr;
    QSystemTrayIcon* tray_icon;
    QMenu* tray_menu;

    bool start_connect_btn_loading = false;
    bool close_connect_btn_loading = false;

    bool is_connected = false;
    bool is_light_mode = true;

    PythonSubprocessManager* m_pyManager;

    LogLevel ui_log_level = LOG_DEBUG;
    LogSink qtSink;

    template<typename Callback>
    void async_call(const QJsonObject& cmd, int timeout, Callback&& callback);

    void setup_debug_log();
    void register_log_sink();
    void create_log_highlighter();
    void load_main_image();
    void create_tray_icon();
    void setup_widget_properties(const std::string& property, const std::string& key);
    void load_stylesheet();
    void load_light_stylesheet();
    void load_night_stylesheet();
    void change_theme();
    void setup_log_widget_style();
    void setup_connections();
    void setup_port_input_validation();
    void setup_default_page();
    void init_python_manager();

    QComboBox* rule_file_combo_;
    QTableWidget* rule_table_;
    QPushButton* create_file_btn_;
    QPushButton* delete_file_btn_;
    QPushButton* save_file_btn_;
    QPushButton* add_rule_btn_;
    QPushButton* edit_rule_btn_;
    QPushButton* delete_rule_btn_;
    void setup_rules_ui();
    void init_rule_manager();
    void refresh_rule_file_list();
    void update_rule_table();

signals:
    void connect_finished(bool success, const QString& message);
    void code_content_ready(const QString& content);
    void close_finished(bool success, const QString& message);

private slots:
    void handle_connect_finished(bool success, const QString& msg);
    void handle_code_content_ready(const QString& content);
    void handle_close_finished(bool success, const QString& msg);
    void start_async_connect();
    void close_async_connect();

    void on_rule_file_changed(int index);
    void on_create_rule_file();
    void on_delete_rule_file();
    void on_save_rule_file();
    void on_add_rule();
    void on_edit_rule();
    void on_delete_rule();
};
