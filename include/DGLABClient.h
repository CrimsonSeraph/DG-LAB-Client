#pragma once

#include "DebugLog.h"
#include "PythonSubprocessManager.h"
#include "ui_DGLABClient.h"

#include <QCloseEvent>
#include <QComboBox>
#include <QJsonObject>
#include <QMenu>
#include <QPushButton>
#include <QSyntaxHighlighter>
#include <QSystemTrayIcon>
#include <QTableWidget>
#include <QTextEdit>
#include <QWidget>

#include <string>
#include <vector>

class DGLABClient : public QWidget {
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

    void on_show_qr_btn_clicked();

    void change_ui_log_level();
    void set_port();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    std::vector<std::string> default_blacklist_ = { "radmin", "vmware", "virtualbox", "virtual", "vpn", "tap", "tun" };
    std::vector<std::string> default_whitelist_ = { "wlan" };

    Ui::DGLABClientClass ui_;
    void append_log_message(const QString& message, int level);
    void append_colored_text(QTextEdit* edit, const QString& text);
    QSyntaxHighlighter* log_highlighter_ = nullptr;
    QSystemTrayIcon* tray_icon_;
    QMenu* tray_menu_;
    QString current_qr_path_;

    bool start_connect_btn_loading_ = false;
    bool close_connect_btn_loading_ = false;

    bool is_connected_ = false;
    bool is_light_mode_ = true;

    PythonSubprocessManager* py_manager_;

    LogLevel ui_log_level_ = LOG_DEBUG;
    bool use_fixed_width_log_ = false;
    LogSink qt_sink_;

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
    void reset_py_log_level();
    void fetch_qr_path();
    void show_qr_dialog();
    void delete_old_qr_file();
    QString get_local_lan_ip();

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
    void close_finished(bool success, const QString& message);

private slots:
    void handle_connect_finished(bool success, const QString& msg);
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
