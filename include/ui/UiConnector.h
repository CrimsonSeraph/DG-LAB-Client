/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>

#include <functional>

class AppBridge;
class CoyoteBleController;
class DeviceController;
class HomeBridge;
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

    /// @brief 弹出确认弹层，用户点确认后执行 on_accept；取消只关闭弹层
    /// @note 同一时刻只保留一个待确认动作：再次调用会覆盖上一次的回调
    void ask_confirm(const QString& title, const QString& message, bool danger,
        std::function<void()> on_accept);

protected:
    /// @brief 主窗口事件过滤器：把 Esc（取消）与 Enter（主操作）接到三个对话框上
    /// @note QML 不为键位写处理器；三个对话框都是 closePolicy: Popup.NoAutoClose，
    ///       Esc 不会由 Popup 自己处理，本过滤器是唯一入口
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    // 导航
    void on_nav_button_clicked();
    void on_home_module_entry_clicked();
    void on_rule_editor_clicked();
    void on_rule_editor_close_clicked();
    void on_rule_save_clicked();
    void on_rule_fit_view_clicked();
    // 通用确认弹层
    void on_confirm_accepted();
    void on_confirm_canceled();
    // 首页通道
    void on_channel_toggle_clicked();
    void on_strength_modified();
    void on_strength_adjust_clicked();
    void on_wave_select_clicked();
    // 配置：连接
    void on_config_connect_clicked();
    // 配置：主题
    void on_theme_select_clicked();
    void on_theme_custom_clicked();
    void on_theme_preset_clicked();
    void on_custom_theme_save_clicked();
    void on_custom_primary_clicked();
    void on_custom_secondary_clicked();
    void on_primary_color_accepted();
    void on_secondary_color_accepted();
    // 模块页
    void on_module_period_applied();
    void on_plugin_toggle_clicked();
    void on_module_card_clicked();
    // 波形库与编辑器（on_wave_select_clicked 见"首页通道"）
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
    // 蓝牙直连
    void on_ble_scan_clicked();
    void on_ble_device_clicked();
    void on_ble_disconnect_clicked();
    void on_ble_strength_clicked();
    // 日志
    void on_log_export_clicked();
    void on_log_clear_clicked();
    void on_log_level_changed();
    void on_log_list_movement_ended();
    void on_log_messages_changed();
    // 动态内容变化后重扫热区
    void on_tracked_children_changed();
    // 软件更新
    void on_check_update_clicked();
    void on_update_download_clicked();
    void on_update_apply_clicked();

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
    RuleGraphBridge* rule_graph_ = nullptr;
    LogBridge* log_ = nullptr;
    QPointer<QObject> root_;
    QPointer<QObject> confirm_dialog_;       ///< 通用确认弹层
    std::function<void()> confirm_callback_; ///< 确认后要执行的动作（取消即丢弃）
    QPointer<QObject> log_list_;             ///< 日志列表（贴底跟随在此维护）
    qreal log_scroll_y_ = 0.0;               ///< 用户滚动停留时的 contentY
    QString wave_last_error_;                ///< 上一次已反馈过的波形错误（避免重复 Toast）
    QList<ItemWatch> watches_;
    QSet<QObject*> connected_items_;
    UpdateBridge* update_ = nullptr;
};
