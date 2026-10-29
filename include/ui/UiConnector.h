/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <QQuickWindow>
#include <QSet>
#include <QString>
#include <QTimer>

#include <functional>

class AppBridge;
class QColor;
class CoyoteBleController;
class DeviceController;
class HomeBridge;
class IpSelector;
class LogBridge;
class ModuleBridge;
class RuleGraphBridge;
class ThemeManager;
class WaveBridge;
class QQuickItem;
class UpdateBridge;

// UiConnector - QML 与 C++ 的连接集中点
// QML 不写信号处理器（onClicked / Connections），每个交互控件都有稳定的 objectName，
// 加载后由本类按 objectName 显式建立连接；列表 / 中继器 / 布局动态生成的委托由
// watch_items() 沿可视子树扫描热区。约定与例外见 src/ui/README.md。
class UiConnector : public QObject {
    Q_OBJECT

public:
    UiConnector(AppBridge* app, HomeBridge* home, ThemeManager* theme, DeviceController* device,
        ModuleBridge* module, WaveBridge* wave, CoyoteBleController* ble, RuleGraphBridge* rule_graph,
        LogBridge* log, UpdateBridge* update, QObject* parent = nullptr);

    /// @brief 在 engine.load 之后调用，连接根对象下的全部交互控件
    void attach(QObject* root_object);

    /// @brief 迷你窗对象（attach 之后可用，供托盘控制器绑定）
    QQuickWindow* mini_window() const { return mini_window_.data(); }

    /// @brief 弹出确认弹层，用户点确认后执行 on_accept；取消只关闭弹层
    /// @note 同一时刻只保留一个待确认动作：再次调用会覆盖上一次的回调
    void ask_confirm(const QString& title, const QString& message, bool danger,
        std::function<void()> on_accept);

protected:
    /// @brief 主窗口事件过滤器：把 Esc（取消）与 Enter（主操作）接到四个对话框上
    /// @note QML 不为键位写处理器；四个对话框都是 closePolicy: Popup.NoAutoClose，
    ///       Esc 不会由 Popup 自己处理，本过滤器是唯一入口
    /// @note 确认弹层可见时只认 Esc = 取消、Enter = 确认，且不穿透到底下的编辑器
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    // 导航
    void on_nav_button_clicked();
    void on_plugin_detail_requested(const QString& plugin_key);
    void on_rule_editor_clicked();
    void on_rule_editor_close_clicked();
    void on_rule_discard_save_clicked();
    void on_rule_discard_discard_clicked();
    void on_rule_discard_cancel_clicked();
    void on_rule_save_clicked();
    void on_rule_fit_view_clicked();
    // 通用确认弹层
    void on_confirm_accepted();
    void on_confirm_canceled();
    // 首页通道
    void on_channel_toggle_clicked();
    // 首页看板：默认设备下拉
    void on_home_default_device_activated(int index);
    // 配置：连接
    void on_config_connect_clicked();
    // 配置：网卡 IP 选择
    void on_config_select_ip_clicked();
    void on_ip_blacklist_add_clicked();
    void on_ip_blacklist_remove_clicked();
    void on_ip_whitelist_add_clicked();
    void on_ip_whitelist_remove_clicked();
    void on_ip_candidate_clicked();
    void on_ip_auto_clicked();
    void on_ip_select_confirm_clicked();
    void on_ip_select_cancel_clicked();
    // 配置：主题
    void on_theme_select_clicked();
    void on_theme_custom_clicked();
    void on_theme_preset_clicked();
    void on_custom_theme_save_clicked();
    void on_custom_primary_clicked();
    void on_custom_secondary_clicked();
    void on_primary_color_accepted();
    void on_secondary_color_accepted();
    // 自绘取色对话框：滑杆、HEX 输入与两个按钮（均经 colorDialogOwner 找回所属对话框）
    void on_color_dialog_slider_moved();
    void on_color_dialog_hex_edited();
    void on_color_dialog_hex_finished();
    void on_color_dialog_confirm_clicked();
    void on_color_dialog_cancel_clicked();
    // 插件中心：统一查询周期
    void on_module_period_applied();
    // 插件中心：详情页选中插件变化时同步数值页签的模块标识
    void on_plugin_detail_item_changed();
    // 插件卡规则入口：没有 plugin_<插件标识>.json 时新建，有则直接编辑；打开编辑器并切到该插件图
    void on_plugin_rule_entry_clicked();
    // 波形库与编辑器
    void on_wave_select_clicked();
    void on_wave_channel_clicked();
    void on_wave_create_clicked();
    void on_wave_library_item_clicked();
    void on_wave_delete_clicked();
    void on_wave_section_clicked();
    void on_wave_add_section_clicked();
    void on_wave_remove_section_clicked();
    void on_wave_apply_section_clicked();
    void on_wave_apply_raw_clicked();
    void on_wave_save_clicked();
    void on_wave_send_clicked();
    void on_wave_editor_close_clicked();
    void on_wave_select_close_clicked();
    // 波形导入导出
    void on_wave_import_clicked();
    void on_wave_export_clicked();
    void on_wave_export_cancel_clicked();
    void on_wave_export_confirm_clicked();
    void on_wave_export_format_selected(int index);
    void on_wave_export_scope_selected(int index);
    void on_wave_export_file_accepted();
    void on_wave_import_file_accepted();
    void on_wave_conflict_requested(int count);
    void on_wave_conflict_overwrite_clicked();
    void on_wave_conflict_rename_clicked();
    void on_wave_conflict_skip_clicked();
    void on_wave_conflict_cancel_clicked();
    // 波形标签
    void on_wave_tag_edit_clicked();
    void on_wave_tag_toggle_clicked();
    void on_wave_tag_create_clicked();
    void on_wave_tag_delete_clicked();
    void on_wave_tag_dialog_close_clicked();
    // 蓝牙直连（设备管理页）：扫描与强度增减
    void on_ble_scan_clicked();
    void on_ble_strength_clicked();
    // 设备管理页：扫描结果行连接 / 断开、已连接链路行断开、默认设备、按设备通道上下限
    void on_device_scan_action_clicked();
    void on_device_link_disconnect_clicked();
    void on_device_default_activated(int index);
    void on_device_limit_device_activated(int index);
    void on_device_limit_value_modified();
    void on_device_limits_changed();
    // 设备管理页：通道级回退值（app.channels.<通道>，设备未单独设置时使用）
    void on_device_channel_fallback_modified();
    // 日志
    void on_log_export_clicked();
    void on_log_clear_clicked();
    void on_log_level_changed();
    void on_log_export_level_changed();
    void on_log_export_range_changed();
    void on_log_export_dir_applied();
    void on_log_list_movement_ended();
    void on_log_messages_changed();
    // 动态内容变化后重扫热区
    void on_tracked_children_changed();
    // 软件更新
    void on_check_update_clicked();
    void on_panel_check_update_clicked();
    void on_update_download_clicked();
    void on_update_apply_clicked();
    void on_update_open_release_clicked();
    void on_update_dialog_close_clicked();
    // 迷你窗
    void on_home_mini_window_clicked();
    void on_mini_window_lock_clicked();
    void on_mini_window_close_clicked();
    void on_mini_window_geometry_changed();
    void on_mini_window_opacity_moved();
    void on_mini_window_size_clicked();
    void on_mini_window_visibility_changed();
    void on_mini_window_wave_reset();
    void on_mini_window_wave_advanced();

private:
    void connect_clicked(QObject* root_object, const char* object_name, const char* slot);
    void open_dialog(const char* object_name);
    void close_dialog(const char* object_name);
    /// @brief 对话框是否处于可见状态（键位过滤器只在可见时生效）
    bool dialog_visible(const char* object_name) const;
    /// @brief 监听列表 contentItem 或布局项，并连接其中名为 item_name 的热区
    void watch_items(QObject* root_object, const char* owner_name, const QString& item_name, const char* slot);
    void scan_dynamic_items(QQuickItem* parent, const QString& item_name, const char* slot);
    QObject* find(const char* object_name) const;
    /// @brief 取根对象所在 QML 上下文里的上下文属性对象（如注入的 eventBridge）
    /// @param name 上下文属性名
    /// @return 对象指针，取不到时返回空
    QObject* context_object(const char* name) const;
    /// @brief 把选中地址写入地址栏并走既有地址更新路径（二维码载荷随之刷新）
    void apply_selected_ip(const QString& ip);
    /// @brief 按关键词输入框的内容增删名单（空关键词给出提示）
    /// @param field_name 关键词输入框 objectName
    /// @param whitelist 为 true 时操作白名单，否则操作黑名单
    void add_keyword_from_field(const char* field_name, bool whitelist);
    /// @brief 定位迷你窗并恢复上次保存的位置（越界时夹回可见区域）
    void setup_mini_window(QObject* root_object);
    /// @brief 把迷你窗当前位置、尺寸、透明度与大小档位写回配置
    void save_mini_window_state();
    /// @brief 把滚动波形游标处的最近若干帧推给 HomeBridge
    void apply_mini_window_wave();
    /// @brief 按当前选中的设备回填四条上下限（无在线设备时回填通道回退值并标注「默认」）
    void refresh_device_limits_fields();
    /// @brief 刷新插件卡「创建规则 / 编辑规则」状态：把已有规则图文件的插件标识推给插件中心页
    /// @note 触发点：加载插件列表、启用 / 停用插件（ModuleBridge::dataChanged）、保存规则（RuleGraphBridge::pluginGraphsChanged）
    void refresh_plugin_rule_entries();
    /// @brief 上下限编辑目标列表：在线输出设备 + 已记忆设备（app.devices 里出现过、且不是纯输入设备的 MAC）
    /// @return 列表项含 address / name / label / online，离线项名称带「离线」
    QVariantList limit_targets() const;
    /// @brief 打开系统文件对话框（保存对话框先写入建议文件名）
    /// @param object_name 文件对话框 objectName
    /// @param suggested_path 建议文件路径（为空时不写入）
    void open_file_dialog(const char* object_name, const QString& suggested_path);

    /// @brief 绑定自绘取色对话框：三条滑杆、HEX 输入与确定 / 取消的交互都在这里连接
    void bind_color_dialog(QObject* dialog);
    /// @brief 取回控件所属的取色对话框（绑定控件时记在 colorDialogOwner 上）
    QObject* color_dialog_of(QObject* control) const;
    /// @brief 打开自绘取色对话框，初始值取目标属性上的当前颜色
    /// @param dialog_object_name 取色对话框 objectName
    /// @param target_property 自定义主题对话框上的颜色属性（customPrimary / customSecondary）
    void open_color_dialog(const char* dialog_object_name, const char* target_property);
    /// @brief 把颜色写入取色对话框：预览、HEX 回显与三条滑杆同步
    /// @param update_hex_field 为 false 时保留输入框文本（用户正在输入 HEX）
    void set_color_dialog_color(QObject* dialog, const QColor& color, bool update_hex_field);
    /// @brief 由三条滑杆的当前值重算颜色并刷新预览与 HEX 回显
    void sync_color_dialog_from_hsv(QObject* dialog);
    /// @brief 由 HEX 文本更新颜色，格式非法时只写错误提示
    void sync_color_dialog_from_hex(QObject* dialog, const QString& text);
    /// @brief 编辑结束把合法的 HEX 文本规范化成 #RRGGBB
    void normalize_color_dialog_hex(QObject* dialog);

    struct ItemWatch {
        QQuickItem* owner = nullptr;
        QString item_name;
        const char* slot = nullptr;
    };

    AppBridge* app_ = nullptr;
    HomeBridge* home_ = nullptr;
    ThemeManager* theme_ = nullptr;
    DeviceController* device_ = nullptr;
    ModuleBridge* module_ = nullptr;
    WaveBridge* wave_ = nullptr;
    CoyoteBleController* ble_ = nullptr;
    IpSelector* ip_selector_ = nullptr; ///< 局域网地址选择器（device.ipSelector）
    RuleGraphBridge* rule_graph_ = nullptr;
    LogBridge* log_ = nullptr;
    QPointer<QObject> root_;
    QPointer<QObject> confirm_dialog_;       ///< 通用确认弹层
    std::function<void()> confirm_callback_; ///< 确认后要执行的动作（取消即丢弃）
    QPointer<QObject> log_list_;             ///< 日志列表（贴底跟随在此维护）
    qreal log_scroll_y_ = 0.0;               ///< 用户滚动停留时的 contentY
    QString wave_last_error_;                ///< 上一次已反馈过的波形错误（避免重复 Toast）
    QPointer<QQuickWindow> mini_window_;     ///< 迷你窗（QML 侧 MiniWindow.qml）
    QTimer mini_window_save_timer_;          ///< 迷你窗状态保存的延迟合并计时器
    QTimer mini_window_wave_timer_;          ///< 迷你窗波形滚动计时器（100 ms，与波形帧同拍）
    qint64 mini_window_wave_cursor_ = 0;     ///< 波形滚动游标（帧序号，循环播放）
    QString limits_device_;                   ///< 设备管理页正在编辑上下限的设备 MAC（空串表示通道回退值）
    QList<ItemWatch> watches_;
    QSet<QObject*> connected_items_;
    UpdateBridge* update_ = nullptr;
};
