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
#include <QToolButton>
#include <QWidget>

#include <map>
#include <string>
#include <vector>

// ============================================
// DGLABClient - 主窗口类
// ============================================
// 前置声明
class ModuleValuesDialog;

class DGLABClient : public QWidget {
    Q_OBJECT

public:
    // -------------------- 构造/析构 --------------------
    explicit DGLABClient(QWidget* parent = nullptr);
    ~DGLABClient();

    // -------------------- 初始化 --------------------
    /// @brief 初始化基础组件（端口校验、默认页、托盘、规则 UI、Python 管理器）
    void normal_init();
    /// @brief 初始化日志系统（等级、Sink、高亮器）
    void init_log();
    /// @brief 初始化界面标签显示（图标、通道信息、连接状态、IP/端口、主题）
    void init_label();
    /// @brief 连接各页面的信号槽
    void init_connect() const;
    /// @brief 初始化窗口样式（日志控件、控件属性、样式表、内联样式）
    void init_style();
    /// @brief 根据配置更新 UI 日志级别
    void change_ui_log_level();

    // -------------------- 页面导航 --------------------
    /// @brief 刷新窗口图标（资源缺失时显示加载失败提示）
    void refresh_icon();
    /// @brief 切换到主页
    void change_main_page();
    /// @brief 切换到配置页
    void change_config_page();
    /// @brief 切换到规则页
    void change_rule_page();
    /// @brief 切换到模块页
    void change_module_page();
    /// @brief 切换到关于页
    void change_about_page();
    /// @brief 连接页面导航按钮的点击信号
    void connect_about_page() const;

    // -------------------- 通道控制 --------------------
    /// @brief 刷新 A/B 通道强度显示与启动按钮文本
    void refresh_channel_info();
    /// @brief 启动/关闭 A 通道（启动时发送测试命令）
    void enable_A();
    /// @brief 启动/关闭 B 通道
    void enable_B();
    /// @brief 为 A/B 强度编辑标签配置输入验证器（0~200）
    void setup_channel_value_editor_input_validation();
    /// @brief 连接通道控制相关信号槽
    void connect_about_channel_contral() const;

    // -------------------- 连接相关 --------------------
    /// @brief 设置 IP 标签不可编辑（仅双击弹出选择器）
    void set_port_label_mode();
    /// @brief 刷新连接按钮文本（连接/断开）与端口显示
    void refresh_connect_label();
    /// @brief 处理连接/断开按钮点击（防重复触发）
    void handle_connect();
    /// @brief 弹出 IP 选择对话框并保存所选 IP 到配置
    void set_ip();
    /// @brief 从配置读取 IP/端口并刷新界面显示
    void refresh_ip_port_label();
    /// @brief 为端口编辑标签配置输入验证器（0~65535）
    void setup_port_input_validation();
    /// @brief 保存当前 IP/端口到系统配置
    void set_ip_port();
    /// @brief 缓存用户输入的端口（校验合法性）
    /// @param input 用户输入的端口字符串
    void cache_port(const QString& input);
    /// @brief 连接连接控制相关信号槽
    void connect_about_connect() const;
    /// @brief 获取缓存的 IP 地址
    /// @return 当前 IP 缓存
    inline QString get_ip_cache_() { return ip_cache_; };

    // -------------------- 主题相关 --------------------
    /// @brief 刷新主题标签显示
    void refresh_theme_label();
    /// @brief 连接主题选择相关信号槽
    void connect_about_theme() const;

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
    /// @brief 重写关闭事件: 托盘可见时最小化到托盘，否则接受关闭
    /// @param event 关闭事件对象
    void closeEvent(QCloseEvent* event) override;

private:
    // -------------------- 成员变量 --------------------
    Ui::DGLABClientClass ui_;                       ///< UI 界面
    QSyntaxHighlighter* log_highlighter_ = nullptr; ///< 日志高亮器
    QSystemTrayIcon* tray_icon_ = nullptr;          ///< 系统托盘图标
    QMenu* tray_menu_;                              ///< 托盘菜单
    QString current_qr_path_;                       ///< 当前二维码文件路径

    QString ip_cache_ = "127.0.0.1";   ///< 连接 IP 地址
    int port_cache_ = 9999;            ///< 连接端口
    bool connect_btn_loading_ = false; ///< 连接按钮加载状态
    bool is_connected_ = false;        ///< 连接状态
    Theme theme_ = LIGHT;              ///< 主题

    bool is_A_start = false; ///< A通道启动状态
    bool is_B_start = false; ///< B通道启动状态
    int A_strength_ = 0;     ///< 强度 A（0-200）
    int B_strength_ = 0;     ///< 强度 B（0-200）
    int A_limit_ = 200;      ///< A通道上限（默认200）
    int B_limit_ = 200;      ///< B通道上限（默认200）

    PythonSubprocessManager* py_manager_; ///< Python 子进程管理器

    LogLevel ui_log_level_ = LOG_DEBUG; ///< UI 日志级别
    bool use_fixed_width_log_ = false;  ///< 是否使用固定宽度日志格式
    LogSink qt_sink_;                   ///< Qt UI 日志输出通道

    // 规则 UI 控件
    QToolButton* rule_file_btn_; ///< 显示当前选中文件的按钮
    QMenu* rule_file_menu_;      ///< 弹出菜单
    QTableWidget* rule_table_;
    QPushButton* create_file_btn_;
    QPushButton* delete_file_btn_;
    QPushButton* save_file_btn_;
    QPushButton* add_rule_btn_;
    QPushButton* edit_rule_btn_;
    QPushButton* edit_parents_btn_;
    QPushButton* delete_rule_btn_;
    bool updating_rule_table_ = false; ///< 表格刷新标志（防止 itemChanged 递归）

    // 模块页 UI 控件
    QComboBox* module_period_combo_ = nullptr;        ///< 统一查询周期下拉框
    QPushButton* module_period_apply_btn_ = nullptr;  ///< 统一周期应用按钮
    QWidget* module_cards_widget_ = nullptr;          ///< 模块卡片容器
    ModuleValuesDialog* module_values_dialog_ = nullptr; ///< 模块数值弹窗（防重复打开）
    std::map<std::string, QLabel*> rule_value_labels_;    ///< "通道:规则名" → 数值标签（首页规则卡片）

    // -------------------- 私有辅助函数（初始化） --------------------
    /// @brief 配置日志控件为只读并设置默认字体
    void setup_debug_log();
    /// @brief 注册 Qt UI 日志输出 Sink（线程安全回主线程）
    void register_log_sink();
    /// @brief 创建日志文本高亮器（按日志等级着色）
    void create_log_highlighter();
    /// @brief 创建系统托盘图标与菜单（资源缺失时跳过）
    void create_tray_icon();

    /// @brief 根据配置加载 QSS 样式表并应用到全局
    void load_stylesheet();
    /// @brief 切换主题（std::string 版本）
    /// @param theme_str 主题英文名（如 "light"）
    void change_theme(const std::string& theme_str);
    /// @brief 切换主题（QString 版本）
    /// @param theme_str 主题英文名
    void change_theme(const QString& theme_str);
    /// @brief 设置日志控件的硬编码样式
    void setup_log_widget_style();
    /// @brief 应用内联样式（预留扩展点）
    void setup_inline_style();
    /// @brief 强制刷新所有子控件的样式
    void refresh_style();

    /// @brief 设置默认显示页面（主页）
    void setup_default_page();
    /// @brief 初始化 Python 子进程管理器并启动进程
    void init_python_manager();
    /// @brief 向 Python 端下发日志级别设置命令
    void reset_py_log_level();
    /// @brief 初始化数值模块管理器并构建模块页面（统一周期设置 + 模块卡片）
    void setup_module_ui();
    /// @brief 连接规则引擎信号（规则命令发送到 Python 端）
    void connect_rule_engine();
    /// @brief 填充首页 A/B 通道卡片（模块区域 + 规则区域）
    void setup_channel_cards();
    /// @brief 填充单个通道的模块卡片（模块名称 + 最小查询周期）
    /// @param layout 目标布局
    /// @param channel 通道（"A"/"B"）
    void populate_channel_module_card(QVBoxLayout* layout, const std::string& channel);
    /// @brief 填充单个通道的规则卡片（父级为该通道的规则名称 + 最近计算值）
    /// @param layout 目标布局
    /// @param channel 通道（"A"/"B"）
    void populate_channel_rule_card(QVBoxLayout* layout, const std::string& channel);
    /// @brief 规则结果变化时刷新对应通道规则卡片的数值
    /// @param rule_name 规则名称
    /// @param channel 通道（"A"/"B"）
    /// @param value 计算结果
    void refresh_channel_rule_cards(const QString& rule_name, const QString& channel, int value);
    /// @brief 点击模块卡片，弹出模块数值展示窗口
    /// @param module_name 模块名称
    void show_module_values(const QString& module_name);
    /// @brief 应用统一查询周期设置到所有数值
    void apply_module_period_setting();
    /// @brief 刷新所有模块卡片的周期显示
    void refresh_module_cards();
    /// @brief 创建单个模块卡片并放入网格布局
    /// @param module_name 模块名称
    /// @param layout 目标网格布局
    /// @param row 行号
    /// @param col 列号
    void create_module_card(const QString& module_name, QGridLayout* layout, int row, int col);

    // -------------------- 私有辅助函数（二维码） --------------------
    /// @brief 异步请求 Python 端生成二维码并获取路径
    void fetch_qr_path();
    /// @brief 显示二维码对话框（路径无效时给出提示）
    void show_qr_dialog();
    /// @brief 删除旧的二维码临时文件
    void delete_old_qr_file();

    // -------------------- 私有辅助函数（规则 UI） --------------------
    /// @brief 构建规则管理界面（文件选择、表格、操作按钮）
    void setup_rules_ui();
    /// @brief 刷新规则文件下拉菜单列表
    void refresh_rule_file_list();
    /// @brief 用当前规则集刷新规则表格
    void update_rule_table();

    // -------------------- 私有辅助函数（样式） --------------------
    /// @brief 为所有控件统一设置指定动态属性
    /// @param property 属性名（如 "type"、"button_type"）
    /// @param key 属性值
    void setup_widget_properties(const std::string& property, const std::string& key);
    /// @brief 为所有需要样式的控件设置 type 属性
    void apply_widget_properties();
    /// @brief 应用所有内联样式表
    void apply_inline_styles();
    /// @brief 判断当前主题文本色是否为浅色（白色系）
    /// @return 文本为浅色返回 true
    bool theme_text_is_light() const;
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
    /// @brief 清理日志消息中的 ANSI 转义序列并追加到日志控件
    /// @param message 原始日志消息
    /// @param level 日志等级（用于高亮）
    void append_log_message(const QString& message, int level);
    /// @brief 向指定文本编辑控件追加带换行的彩色文本
    /// @param edit 目标文本编辑控件
    /// @param text 要追加的文本
    void append_colored_text(QTextEdit* edit, const QString& text);

    // -------------------- 私有辅助函数（连接控制） --------------------
    /// @brief 刷新 A/B 通道强度显示标签
    void refresh_channel_strength();
    /// @brief 应用 A 通道强度修改（校验范围并发送命令）
    /// @param new_strength 新的强度值字符串
    void apply_A_strength(const QString& new_strength);
    /// @brief 应用 B 通道强度修改（校验范围并发送命令）
    /// @param new_strength 新的强度值字符串
    void apply_B_strength(const QString& new_strength);

signals:
    /// @brief 连接操作完成时发出
    /// @param success 是否成功
    /// @param message 结果消息
    void connect_finished(bool success, const QString& message);
    /// @brief 断开操作完成时发出
    /// @param success 是否成功
    /// @param message 结果消息
    void close_finished(bool success, const QString& message);

private slots:
    // 连接相关槽函数
    /// @brief 连接结果处理槽: 更新按钮状态与连接标志
    /// @param success 是否成功
    /// @param msg 结果消息
    void handle_connect_finished(bool success, const QString& msg);
    /// @brief 断开结果处理槽: 更新按钮状态并清理二维码
    /// @param success 是否成功
    /// @param msg 结果消息
    void handle_close_finished(bool success, const QString& msg);
    /// @brief 异步发起连接流程（先更新 WebSocket 地址再连接）
    void start_async_connect();
    /// @brief 异步发起断开连接流程
    void close_async_connect();

    // 主题相关槽函数
    /// @brief 打开主题选择对话框
    void show_theme_selector();
    /// @brief 切换主题（Theme 枚举版本）
    /// @param theme 目标主题枚举
    void change_theme(Theme theme);

    // 规则文件管理槽函数
    /// @brief 规则文件菜单项被选中时的处理
    /// @param action 被选中的菜单动作
    void on_rule_file_selected(QAction* action);
    /// @brief 新建规则文件（自动补全关键字与 .json 后缀）
    void on_create_rule_file();
    /// @brief 删除当前规则文件（默认文件不可删除）
    void on_delete_rule_file();
    /// @brief 保存当前规则文件
    void on_save_rule_file();

    // 规则管理槽函数
    /// @brief 添加规则（弹窗输入名称、通道、模式与值表达式）
    void on_add_rule();
    /// @brief 编辑当前选中的规则
    void on_edit_rule();
    /// @brief 编辑当前选中规则的父级（通道单选 + 规则多选）
    void on_edit_parents();
    /// @brief 删除当前选中的规则
    void on_delete_rule();
    /// @brief 规则表格单元格内容变化（父级列编辑同步到规则管理器）
    /// @param item 变化的单元格
    void on_rule_table_item_changed(QTableWidgetItem* item);

    // 消息处理槽函数
    /// @brief 处理 Python 端推送的主动消息（强度/断开/错误/绑定等）
    /// @param message 主动消息 JSON 对象
    void on_active_message_received(const QJsonObject& message);
};

#include "DGLABClient_impl.hpp"