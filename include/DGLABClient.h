/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "DebugLog.h"
#include "PythonSubprocessManager.h"
#include "ThemeSelectorDialog.h"
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

// ============================================
// DGLABClient - 主窗口类
// ============================================
class DGLABClient : public QWidget {
    Q_OBJECT

public:
    // -------------------- 构造/析构 --------------------
    explicit DGLABClient(QWidget* parent = nullptr);
    ~DGLABClient();

    // -------------------- 公共接口（页面导航） --------------------
    void on_main_first_btn_clicked();
    void on_main_config_btn_clicked();
    void on_main_setting_btn_clicked();
    void on_main_about_btn_clicked();

    // -------------------- 公共接口（连接控制） --------------------
    void on_start_connect_btn_clicked();
    void on_close_connect_btn_clicked();
    void on_start_btn_clicked();
    void on_close_btn_clicked();
    void on_show_qr_btn_clicked();

    // -------------------- 公共接口（设置） --------------------
    void change_ui_log_level();
    void set_port();

    // -------------------- 模板方法（异步调用） --------------------
    /// @brief 异步调用 Python 服务
    /// @tparam Callback 回调函数类型 (bool success, QString message)
    /// @param cmd 要发送的 JSON 命令
    /// @param timeout 超时时间（毫秒）
    /// @param callback 回调函数
    template<typename Callback>
    void async_call(const QJsonObject& cmd, int timeout, Callback&& callback);

protected:
    // -------------------- 重写 QWidget 虚函数 --------------------
    void closeEvent(QCloseEvent* event) override;

private:
    // -------------------- 常量 --------------------
    std::vector<std::string> default_blacklist_ = { "radmin", "vmware", "virtualbox", "virtual", "vpn", "tap", "tun" };
    std::vector<std::string> default_whitelist_ = { "wlan" };

    // -------------------- 成员变量 --------------------
    Ui::DGLABClientClass ui_;   ///< UI 界面
    QSyntaxHighlighter* log_highlighter_ = nullptr; ///< 日志高亮器
    QSystemTrayIcon* tray_icon_;    ///< 系统托盘图标
    QMenu* tray_menu_;  ///< 托盘菜单
    QString current_qr_path_;   ///< 当前二维码文件路径

    bool start_connect_btn_loading_ = false;    ///< 连接按钮加载状态
    bool close_connect_btn_loading_ = false;    ///< 断开按钮加载状态
    bool is_connected_ = false; ///< 连接状态
    Theme theme_ = LIGHT; ///< 主题

    int A_strength_ = 0;    ///< 强度 A（0-200）
    int B_strength_ = 0;    ///< 强度 B（0-200）
    int A_limit_ = 200; ///< A通道上限（默认200）
    int B_limit_ = 200; ///< B通道上限（默认200）
    bool is_A_info_locked_ = false;   ///< A通道强度禁止手动更改状态
    bool is_B_info_locked_ = false;   ///< B通道强度禁止手动更改状态

    PythonSubprocessManager* py_manager_;   ///< Python 子进程管理器

    LogLevel ui_log_level_ = LOG_DEBUG; ///< UI 日志级别
    bool use_fixed_width_log_ = false;  ///< 是否使用固定宽度日志格式
    LogSink qt_sink_;   ///< Qt UI 日志输出通道

    // 规则 UI 控件
    QComboBox* rule_file_combo_;
    QTableWidget* rule_table_;
    QPushButton* create_file_btn_;
    QPushButton* delete_file_btn_;
    QPushButton* save_file_btn_;
    QPushButton* add_rule_btn_;
    QPushButton* edit_rule_btn_;
    QPushButton* delete_rule_btn_;

    // -------------------- 私有辅助函数（初始化） --------------------
    void setup_debug_log();
    void register_log_sink();
    void create_log_highlighter();
    void load_main_image();
    void create_tray_icon();
    void load_stylesheet();
    void change_theme(const std::string& theme_str);
    void change_theme(const QString& theme_str);
    void setup_log_widget_style();
    void setup_connections();
    void setup_port_input_validation();
    void setup_default_page();
    void init_python_manager();
    void reset_py_log_level();
    void init_port_input_placeholder();
    void init_channel_info();

    // -------------------- 私有辅助函数（二维码） --------------------
    void fetch_qr_path();
    void show_qr_dialog();
    void delete_old_qr_file();

    // -------------------- 私有辅助函数（网络） --------------------
    QString get_local_lan_ip();

    // -------------------- 私有辅助函数（规则 UI） --------------------
    void setup_rules_ui();
    void init_rule_manager();
    void refresh_rule_file_list();
    void update_rule_table();

    // -------------------- 私有辅助函数（样式） --------------------
    void setup_widget_properties(const std::string& property, const std::string& key);
    // @brief 为所有需要样式的控件设置 type 属性
    void apply_widget_properties();
    /// @brief 应用所有内联样式表
    void apply_inline_styles();
    /// @brief 将主题转化成文本（英文）
    static QString theme_to_mode_string(Theme theme);
    /// @brief 将文本转化成主题（英文）
    static Theme mode_string_to_theme(const std::string& theme_str);
    static Theme mode_string_to_theme(const QString& theme_str);
    /// @brief 将主题转化成文本（中文）
    static QString theme_to_mode_string_cn(Theme theme);
    /// @brief 将文本转化成主题（中文）
    static Theme mode_string_to_theme_cn(const std::string& theme_str);
    static Theme mode_string_to_theme_cn(const QString& theme_str);

    // -------------------- 私有辅助函数（日志） --------------------
    void append_log_message(const QString& message, int level);
    void append_colored_text(QTextEdit* edit, const QString& text);

    // -------------------- 私有辅助函数（连接控制） --------------------
    void refresh_A_display();
    void refresh_B_display();
    void apply_A_strength_from_input();
    void apply_B_strength_from_input();

signals:
    void connect_finished(bool success, const QString& message);
    void close_finished(bool success, const QString& message);

private slots:
    // 连接相关槽函数
    void on_connect_finished(bool success, const QString& msg);
    void on_close_finished(bool success, const QString& msg);
    void start_async_connect();
    void close_async_connect();

    // 主题相关槽函数
    void show_theme_selector();
    void change_theme(Theme theme);

    // 规则文件管理槽函数
    void on_rule_file_changed(int index);
    void on_create_rule_file();
    void on_delete_rule_file();
    void on_save_rule_file();

    // 规则管理槽函数
    void on_add_rule();
    void on_edit_rule();
    void on_delete_rule();

    // 消息处理槽函数
    void on_active_message_received(const QJsonObject& message);

    // 强度槽函数
    void setup_channel_value_editor_input_validation();
    void change_A_lock();
    void change_B_lock();
};

#include "DGLABClient_impl.hpp"
