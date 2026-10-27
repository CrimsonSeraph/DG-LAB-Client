/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "UiConnector.h"

#include "AppBridge.h"
#include "CoyoteBleController.h"
#include "DebugLog.h"
#include "DeviceController.h"
#include "HomeBridge.h"
#include "LogBridge.h"
#include "ModuleBridge.h"
#include "RuleGraphBridge.h"
#include "ThemeManager.h"
#include "UpdateBridge.h"
#include "WaveBridge.h"

#include <QColor>
#include <QHash>
#include <QKeyEvent>
#include <QMetaObject>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QVariantList>
#include <QVariantMap>

#include <utility>

namespace {

    /// @brief 页面切换按钮 objectName -> 页面索引（左侧导航与左下角面板入口共用）
    const QHash<QString, int>& nav_bindings() {
        static const QHash<QString, int> bindings = {
            {QStringLiteral("navHomeButton"), AppBridge::HomePage},
            {QStringLiteral("navPluginCenterButton"), AppBridge::PluginCenterPage},
            {QStringLiteral("navWaveButton"), AppBridge::WavePage},
            {QStringLiteral("navMiniGameButton"), AppBridge::MiniGamePage},
            {QStringLiteral("navLogButton"), AppBridge::LogPage},
            {QStringLiteral("panelSettingsButton"), AppBridge::ConfigPage},
            {QStringLiteral("panelAboutButton"), AppBridge::AboutPage}};
        return bindings;
    }

    /// @brief 输入类控件的类名：焦点落在这些控件（或其子项）里时，Enter 属于输入本身
    /// @note 用 QObject::inherits 走元对象链判断，不依赖 QtQuick 私有头里的具体类；
    ///       TextField 继承 TextInput，TextArea（QQuickTextArea）继承 TextEdit，
    ///       SpinBox 的 contentItem 就是 TextInput，三种都覆盖到
    const char* const k_text_entry_classes[] = {
        "QQuickTextInput", // TextInput / TextField / SpinBox 的 contentItem
        "QQuickTextEdit",  // TextEdit / TextArea
        "QQuickSpinBox",   // SpinBox 根项自己持焦时
        "QQuickComboBox",  // ComboBox：Enter 用于确认下拉选择，不该触发主操作
    };

    /// @brief 焦点对象或其任一父级是否属于输入类控件
    bool is_text_entry(QObject* object) {
        for (QObject* current = object; current != nullptr; current = current->parent()) {
            for (const char* class_name : k_text_entry_classes) {
                if (current->inherits(class_name)) {
                    return true;
                }
            }
        }
        return false;
    }

    /// @brief 读取 QML 样式单例的令牌值，避免在 C++ 里复制度量数字
    qreal qml_token(QObject* anchor, const char* singleton, const char* property, qreal fallback) {
        if (anchor == nullptr) {
            return fallback;
        }
        QQmlContext* context = qmlContext(anchor);
        QQmlEngine* engine = (context == nullptr) ? nullptr : context->engine();
        if (engine == nullptr) {
            return fallback;
        }
        QObject* object = engine->singletonInstance<QObject*>(QStringLiteral("Dglab"),
            QString::fromLatin1(singleton));
        if (object == nullptr) {
            return fallback;
        }
        const QVariant value = object->property(property);
        return value.isValid() ? value.toReal() : fallback;
    }

    /// @brief 从 "channelAStartButton" 这类名字中解析通道（A/B）
    QString channel_of(const QString& object_name) {
        const qsizetype index = object_name.indexOf(QStringLiteral("channel"));
        if (index < 0 || index + 8 >= object_name.size()) {
            return QString();
        }
        return object_name.mid(index + 7, 1);
    }

} // namespace

UiConnector::UiConnector(AppBridge* app, HomeBridge* home, ThemeManager* theme,
    DeviceController* device, ModuleBridge* module, WaveBridge* wave, CoyoteBleController* ble,
    RuleGraphBridge* rule_graph, LogBridge* log, UpdateBridge* update, QObject* parent)
    : QObject(parent)
    , app_(app)
    , home_(home)
    , theme_(theme)
    , device_(device)
    , module_(module)
    , wave_(wave)
    , ble_(ble)
    , rule_graph_(rule_graph)
    , log_(log)
    , update_(update) {
    if (log_ != nullptr) {
        // statusMessage 现在只在导出失败时发出，走 danger；成功走 exportChanged 带路径
        QObject::connect(log_, &LogBridge::statusMessage, this,
            [this](const QString& message) {
                if (app_ != nullptr) {
                    app_->showToast(message, AppBridge::StatusDanger);
                }
            });
        QObject::connect(log_, &LogBridge::exportChanged, this, [this]() {
            if (app_ != nullptr) {
                app_->showToast(QStringLiteral("日志已导出：") + log_->last_export_path(),
                    AppBridge::StatusInfo);
            }
        });
    }
    if (wave_ != nullptr) {
        // draftSent 只表示调用链走完：下发链路不可用时必须说「未下发」，不能报「已发送」
        QObject::connect(wave_, &WaveBridge::draftSent, this,
            [this](const QString& channel, int seconds) {
                if (app_ == nullptr) {
                    return;
                }
                if (device_ == nullptr || !device_->output_ready()) {
                    app_->showToast(QStringLiteral("通道 %1 未连接设备，波形未下发").arg(channel),
                        AppBridge::StatusWarning);
                    return;
                }
                app_->showToast(QStringLiteral("已发送到通道 %1（%2 秒）").arg(channel).arg(seconds),
                    AppBridge::StatusInfo);
            });
        // 保存/发送失败：编辑器状态行就地显示原因，同时补一次 danger 回执
        QObject::connect(wave_, &WaveBridge::draftChanged, this, [this]() {
            const QString error = wave_->last_error();
            if (error == wave_last_error_) {
                return;
            }
            wave_last_error_ = error;
            if (!error.isEmpty() && app_ != nullptr) {
                app_->showToast(error, AppBridge::StatusDanger);
            }
        });
    }
    if (module_ != nullptr) {
        // 插件启用成功时卡片徽标会变（就地反馈），只有失败才补 Toast
        QObject::connect(module_, &ModuleBridge::pluginToggled, this,
            [this](const QString& file, bool ok, const QString& error) {
                if (ok || app_ == nullptr) {
                    return;
                }
                app_->showToast(QStringLiteral("插件 %1 操作失败：%2").arg(file, error),
                    AppBridge::StatusDanger);
            });
    }
    if (device_ != nullptr) {
        QObject::connect(device_, &DeviceController::errorOccurred, this,
            [this](const QString& message) {
                if (app_ != nullptr) {
                    app_->showToast(message, AppBridge::StatusDanger);
                }
            });
        // device_.initialize() 早于本类构造，启动期（如中转服务端口占用）发出的 errorOccurred
        // 没有接收者；在此用 lastError 补弹一次 Toast，不改动 initialize() 的时序与信号语义。
        const QString startup_error = device_->last_error();
        if (!startup_error.isEmpty() && app_ != nullptr) {
            app_->showToast(startup_error, AppBridge::StatusDanger);
        }
    }
    if (rule_graph_ != nullptr) {
        QObject::connect(rule_graph_, &RuleGraphBridge::statusMessage, this,
            [this](const QString& message) {
                if (app_ != nullptr) {
                    app_->showToast(message, AppBridge::StatusInfo);
                }
            });
    }
    if (update_ != nullptr) {
        QObject::connect(update_, &UpdateBridge::toast_requested, this,
            [this](const QString& message, int level) {
                if (app_ != nullptr) {
                    app_->showToast(message, static_cast<AppBridge::StatusLevel>(level));
                }
            });
    }
}

void UiConnector::attach(QObject* root_object) {
    if (root_object == nullptr) {
        LOG_MODULE("UiConnector", "attach", LOG_ERROR, "根对象为空，无法建立 QML 连接");
        return;
    }
    root_ = root_object;

    // 键位统一入口：Esc（取消）/ Enter（主操作）由本类的事件过滤器接管，QML 不写键位处理器
    if (auto* window = qobject_cast<QQuickWindow*>(root_object)) {
        window->installEventFilter(this);
    }
    else {
        LOG_MODULE("UiConnector", "attach", LOG_WARN, "根对象不是 QQuickWindow，Esc/Enter 不可用");
    }

    for (auto it = nav_bindings().constBegin(); it != nav_bindings().constEnd(); ++it) {
        auto* button = root_object->findChild<QObject*>(it.key());
        if (button == nullptr) {
            LOG_MODULE("UiConnector", "attach", LOG_WARN, "未找到页面切换按钮: " + it.key().toStdString());
            continue;
        }
        QObject::connect(button, SIGNAL(clicked()), this, SLOT(on_nav_button_clicked()));
    }
    connect_clicked(root_object, "homeModuleEntryButton", SLOT(on_home_module_entry_clicked()));
    connect_clicked(root_object, "homeRuleEditorButton", SLOT(on_rule_editor_clicked()));
    connect_clicked(root_object, "configRuleEditorButton", SLOT(on_rule_editor_clicked()));
    connect_clicked(root_object, "ruleEditorCloseButton", SLOT(on_rule_editor_close_clicked()));
    connect_clicked(root_object, "ruleSaveButton", SLOT(on_rule_save_clicked()));
    connect_clicked(root_object, "ruleFitViewButton", SLOT(on_rule_fit_view_clicked()));

    // 通用确认弹层：弹层缺失只告警，ask_confirm 会退化为不执行待确认动作
    confirm_dialog_ = root_object->findChild<QObject*>(QStringLiteral("confirmDialog"));
    if (confirm_dialog_.isNull()) {
        LOG_MODULE("UiConnector", "attach", LOG_WARN, "未找到控件: confirmDialog");
    }
    connect_clicked(root_object, "confirmAcceptButton", SLOT(on_confirm_accepted()));
    connect_clicked(root_object, "confirmCancelButton", SLOT(on_confirm_canceled()));

    // 主页看板只留通道启停：强度调节与波形选择已移出本页，不在此处连接
    connect_clicked(root_object, "channelAStartButton", SLOT(on_channel_toggle_clicked()));
    connect_clicked(root_object, "channelBStartButton", SLOT(on_channel_toggle_clicked()));

    connect_clicked(root_object, "configConnectButton", SLOT(on_config_connect_clicked()));
    connect_clicked(root_object, "configThemeSelectButton", SLOT(on_theme_select_clicked()));
    connect_clicked(root_object, "configThemeCustomButton", SLOT(on_theme_custom_clicked()));
    connect_clicked(root_object, "customThemeSaveButton", SLOT(on_custom_theme_save_clicked()));
    connect_clicked(root_object, "customThemePrimaryButton", SLOT(on_custom_primary_clicked()));
    connect_clicked(root_object, "customThemeSecondaryButton", SLOT(on_custom_secondary_clicked()));
    if (auto* primary = root_object->findChild<QObject*>(QStringLiteral("primaryColorDialog"))) {
        QObject::connect(primary, SIGNAL(accepted()), this, SLOT(on_primary_color_accepted()));
    }
    if (auto* secondary = root_object->findChild<QObject*>(QStringLiteral("secondaryColorDialog"))) {
        QObject::connect(secondary, SIGNAL(accepted()), this, SLOT(on_secondary_color_accepted()));
    }
    watch_items(root_object, "themePresetList", QStringLiteral("themePresetClick"),
        SLOT(on_theme_preset_clicked()));

    connect_clicked(root_object, "modulePeriodApplyButton", SLOT(on_module_period_applied()));
    if (auto* combo = root_object->findChild<QObject*>(QStringLiteral("modulePeriodCombo"))) {
        QObject::connect(combo, SIGNAL(activated(int)), this, SLOT(on_module_period_applied()));
    }
    watch_items(root_object, "moduleCardFlow", QStringLiteral("moduleCardClick"),
        SLOT(on_module_card_clicked()));
    watch_items(root_object, "moduleCardFlow", QStringLiteral("pluginToggleClick"),
        SLOT(on_plugin_toggle_clicked()));

    // 波形库与编辑器
    connect_clicked(root_object, "configWaveChannelAButton", SLOT(on_wave_channel_clicked()));
    connect_clicked(root_object, "configWaveChannelBButton", SLOT(on_wave_channel_clicked()));
    connect_clicked(root_object, "configWaveSelectButton", SLOT(on_wave_select_clicked()));
    connect_clicked(root_object, "configWaveCreateButton", SLOT(on_wave_create_clicked()));
    connect_clicked(root_object, "waveSelectCreateButton", SLOT(on_wave_create_clicked()));
    connect_clicked(root_object, "waveSelectCloseButton", SLOT(on_wave_select_close_clicked()));
    connect_clicked(root_object, "waveAddSectionButton", SLOT(on_wave_add_section_clicked()));
    connect_clicked(root_object, "waveRemoveSectionButton", SLOT(on_wave_remove_section_clicked()));
    connect_clicked(root_object, "waveApplySectionButton", SLOT(on_wave_apply_section_clicked()));
    connect_clicked(root_object, "waveApplyRawButton", SLOT(on_wave_apply_raw_clicked()));
    connect_clicked(root_object, "waveSaveButton", SLOT(on_wave_save_clicked()));
    connect_clicked(root_object, "waveSendAButton", SLOT(on_wave_send_clicked()));
    connect_clicked(root_object, "waveSendBButton", SLOT(on_wave_send_clicked()));
    connect_clicked(root_object, "waveEditorCloseButton", SLOT(on_wave_editor_close_clicked()));
    watch_items(root_object, "waveLibraryList", QStringLiteral("waveSelectClick"),
        SLOT(on_wave_library_item_clicked()));
    watch_items(root_object, "waveLibraryList", QStringLiteral("waveDeleteClick"),
        SLOT(on_wave_delete_clicked()));
    watch_items(root_object, "waveSectionList", QStringLiteral("waveSectionClick"),
        SLOT(on_wave_section_clicked()));

    // 蓝牙直连
    connect_clicked(root_object, "bleScanButton", SLOT(on_ble_scan_clicked()));
    connect_clicked(root_object, "bleDisconnectButton", SLOT(on_ble_disconnect_clicked()));
    connect_clicked(root_object, "bleStrengthADecrease", SLOT(on_ble_strength_clicked()));
    connect_clicked(root_object, "bleStrengthAIncrease", SLOT(on_ble_strength_clicked()));
    connect_clicked(root_object, "bleStrengthBDecrease", SLOT(on_ble_strength_clicked()));
    connect_clicked(root_object, "bleStrengthBIncrease", SLOT(on_ble_strength_clicked()));
    watch_items(root_object, "bleDeviceList", QStringLiteral("bleDeviceClick"),
        SLOT(on_ble_device_clicked()));

    // 日志
    connect_clicked(root_object, "logExportButton", SLOT(on_log_export_clicked()));
    connect_clicked(root_object, "logClearButton", SLOT(on_log_clear_clicked()));
    if (auto* combo = root_object->findChild<QObject*>(QStringLiteral("logLevelCombo"))) {
        QObject::connect(combo, SIGNAL(activated(int)), this, SLOT(on_log_level_changed()));
    }
    // 日志列表贴底跟随：用户滚动结束后按 atYEnd 维护 QML 侧 stickToBottom，
    // 上滚回看历史时不跟新日志，滚回底部后恢复跟随。
    auto* log_list = root_object->findChild<QObject*>(QStringLiteral("configLogList"));
    if (log_list == nullptr) {
        LOG_MODULE("UiConnector", "attach", LOG_WARN, "未找到控件: configLogList");
    }
    else {
        log_list_ = log_list;
        QObject::connect(log_list, SIGNAL(movementEnded()), this, SLOT(on_log_list_movement_ended()));
        if (log_ != nullptr) {
            QObject::connect(log_, &LogBridge::messagesChanged, this, &UiConnector::on_log_messages_changed);
        }
    }

    // 更新模块
    connect_clicked(root_object, "configCheckUpdateButton", SLOT(on_check_update_clicked()));
    connect_clicked(root_object, "panelCheckUpdateButton", SLOT(on_check_update_clicked()));
    connect_clicked(root_object, "updateDownloadButton", SLOT(on_update_download_clicked()));
    connect_clicked(root_object, "updateApplyButton", SLOT(on_update_apply_clicked()));
    connect_clicked(root_object, "aboutReleaseUrlButton", SLOT(on_update_open_release_clicked()));

    LOG_MODULE("UiConnector", "attach", LOG_INFO, "QML 连接已完成");
}

QObject* UiConnector::find(const char* object_name) const {
    if (root_.isNull()) {
        return nullptr;
    }
    return root_->findChild<QObject*>(QString::fromLatin1(object_name));
}

void UiConnector::connect_clicked(QObject* root_object, const char* object_name, const char* slot) {
    auto* target = root_object->findChild<QObject*>(QString::fromLatin1(object_name));
    if (target == nullptr) {
        LOG_MODULE("UiConnector", "connect_clicked", LOG_WARN, std::string("未找到控件: ") + object_name);
        return;
    }
    QObject::connect(target, SIGNAL(clicked()), this, slot);
}

void UiConnector::open_dialog(const char* object_name) {
    auto* dialog = find(object_name);
    if (dialog == nullptr) {
        LOG_MODULE("UiConnector", "open_dialog", LOG_WARN, std::string("未找到对话框: ") + object_name);
        return;
    }
    QMetaObject::invokeMethod(dialog, "open");
}

void UiConnector::close_dialog(const char* object_name) {
    auto* dialog = find(object_name);
    if (dialog != nullptr) {
        QMetaObject::invokeMethod(dialog, "close");
    }
}

bool UiConnector::dialog_visible(const char* object_name) const {
    QObject* dialog = find(object_name);
    return dialog != nullptr && dialog->property("visible").toBool();
}

bool UiConnector::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() != QEvent::KeyPress || root_.isNull() || watched != root_.data()) {
        return QObject::eventFilter(watched, event);
    }
    // 确认弹层盖在最上面时，键位归它自己的按钮路径，不穿透到底下的编辑器
    if (dialog_visible("confirmDialog")) {
        return false;
    }
    auto* key_event = static_cast<QKeyEvent*>(event);
    if (key_event->key() == Qt::Key_Escape) {
        // Esc = 取消：未保存改动先弹确认，未改动直接关（三个对话框都是 NoAutoClose，这里是唯一入口）
        if (dialog_visible("ruleEditorDialog")) {
            on_rule_editor_close_clicked();
            return true;
        }
        if (dialog_visible("waveEditorDialog")) {
            on_wave_editor_close_clicked();
            return true;
        }
        if (dialog_visible("waveSelectDialog")) {
            on_wave_select_close_clicked();
            return true;
        }
        return false;
    }
    if (key_event->key() == Qt::Key_Return || key_event->key() == Qt::Key_Enter) {
        auto* window = qobject_cast<QQuickWindow*>(watched);
        // 输入框里的 Enter 属于输入本身：直接放行，不消费事件
        if (is_text_entry((window == nullptr) ? nullptr : window->activeFocusItem())) {
            return false;
        }
        // Enter = 主操作：规则编辑器保存规则图，波形编辑器保存草稿，波形库无主操作
        if (dialog_visible("ruleEditorDialog")) {
            on_rule_save_clicked();
            return true;
        }
        if (dialog_visible("waveEditorDialog")) {
            on_wave_save_clicked();
            return true;
        }
        return false;
    }
    return false;
}

void UiConnector::ask_confirm(const QString& title, const QString& message, bool danger,
    std::function<void()> on_accept) {
    if (confirm_dialog_.isNull()) {
        LOG_MODULE("UiConnector", "ask_confirm", LOG_WARN,
            "确认弹层缺失，待确认动作未执行: " + title.toStdString());
        return;
    }
    confirm_dialog_->setProperty("titleText", title);
    confirm_dialog_->setProperty("messageText", message);
    confirm_dialog_->setProperty("acceptDanger", danger);
    confirm_callback_ = std::move(on_accept);
    QMetaObject::invokeMethod(confirm_dialog_, "open");
}

void UiConnector::on_confirm_accepted() {
    // 先摘下并清空回调，再关闭、执行：关闭动作本身可能再次 ask_confirm（连续确认），
    // 后关闭会把新弹层一起关掉；清空回调则保证一次点击只触发一次动作。
    auto action = std::move(confirm_callback_);
    confirm_callback_ = nullptr;
    close_dialog("confirmDialog");
    if (action) {
        action();
    }
}

void UiConnector::on_confirm_canceled() {
    confirm_callback_ = nullptr;
    close_dialog("confirmDialog");
}

void UiConnector::watch_items(QObject* root_object, const char* owner_name, const QString& item_name,
    const char* slot) {
    auto* owner = root_object->findChild<QObject*>(QString::fromLatin1(owner_name));
    if (owner == nullptr) {
        LOG_MODULE("UiConnector", "watch_items", LOG_WARN, std::string("未找到容器: ") + owner_name);
        return;
    }
    // 列表用 contentItem 承载委托；普通布局项本身即可视子树根
    QQuickItem* item = owner->property("contentItem").value<QQuickItem*>();
    if (item == nullptr) {
        item = qobject_cast<QQuickItem*>(owner);
    }
    if (item == nullptr) {
        LOG_MODULE("UiConnector", "watch_items", LOG_WARN, std::string("容器无可视子树: ") + owner_name);
        return;
    }
    ItemWatch watch;
    watch.owner = item;
    watch.item_name = item_name;
    watch.slot = slot;
    watches_.append(watch);
    scan_dynamic_items(item, item_name, slot);
    QObject::connect(item, SIGNAL(childrenChanged()), this, SLOT(on_tracked_children_changed()));
}

void UiConnector::scan_dynamic_items(QQuickItem* parent, const QString& item_name, const char* slot) {
    const auto children = parent->childItems();
    for (QQuickItem* child : children) {
        if (child->objectName() == item_name && !connected_items_.contains(child)) {
            connected_items_.insert(child);
            // 旧式 SIGNAL 宏按签名精确匹配：MouseArea 的 clicked 带 QQuickMouseEvent* 参数，
            // Button 系控件是无参 clicked()，两者都要覆盖。
            const QMetaObject* meta = child->metaObject();
            if (meta->indexOfSignal("clicked()") >= 0) {
                QObject::connect(child, SIGNAL(clicked()), this, slot);
            }
            else if (meta->indexOfSignal("clicked(QQuickMouseEvent*)") >= 0) {
                QObject::connect(child, SIGNAL(clicked(QQuickMouseEvent*)), this, slot);
            }
        }
        scan_dynamic_items(child, item_name, slot);
    }
}

void UiConnector::on_tracked_children_changed() {
    for (const auto& watch : watches_) {
        scan_dynamic_items(watch.owner, watch.item_name, watch.slot);
    }
}

void UiConnector::on_check_update_clicked() {
    if (update_ != nullptr) {
        update_->on_check_update_clicked();
    }
}

void UiConnector::on_update_download_clicked() {
    if (update_ != nullptr) {
        update_->on_start_download_clicked();
    }
}

void UiConnector::on_update_apply_clicked() {
    if (update_ != nullptr) {
        ask_confirm(tr("重启并安装更新"), tr("应用将关闭并自动完成更新，是否继续？"), false,
            [this]() {
                update_->on_apply_update_clicked();
            });
    }
}

void UiConnector::on_update_open_release_clicked() {
    if (update_ != nullptr) {
        update_->open_release_url();
    }
}

void UiConnector::on_nav_button_clicked() {
    auto* button = sender();
    if (button == nullptr) {
        return;
    }
    const auto page = nav_bindings().value(button->objectName(), -1);
    if (page >= 0) {
        app_->navigate(page);
    }
}

void UiConnector::on_home_module_entry_clicked() {
    app_->navigate(AppBridge::ModulePage);
}

void UiConnector::on_rule_editor_clicked() {
    if (rule_graph_ != nullptr) {
        rule_graph_->refreshModuleValues();
    }
    open_dialog("ruleEditorDialog");
}

void UiConnector::on_rule_editor_close_clicked() {
    auto* dialog = find("ruleEditorDialog");
    if (dialog == nullptr) {
        LOG_MODULE("UiConnector", "on_rule_editor_close_clicked", LOG_WARN,
            "未找到对话框: ruleEditorDialog");
        return;
    }
    const bool dirty = rule_graph_ != nullptr && rule_graph_->property("dirty").toBool();
    if (!dirty) {
        QMetaObject::invokeMethod(dialog, "close");
        return;
    }
    // 有未保存改动：关闭会丢弃这些改动，先确认
    ask_confirm(tr("放弃未保存的改动？"), tr("规则图有未保存的修改，关闭后将丢失。"), true,
        [guard = QPointer<QObject>(dialog)]() {
            if (!guard.isNull()) {
                QMetaObject::invokeMethod(guard.data(), "close");
            }
        });
}

void UiConnector::on_rule_save_clicked() {
    if (rule_graph_ == nullptr) {
        return;
    }
    rule_graph_->save();
}

void UiConnector::on_rule_fit_view_clicked() {
    auto* canvas = qobject_cast<QQuickItem*>(find("ruleCanvas"));
    if (canvas == nullptr || rule_graph_ == nullptr) {
        LOG_MODULE("UiConnector", "on_rule_fit_view_clicked", LOG_WARN, "未找到画布: ruleCanvas");
        return;
    }
    // 度量全部从 QML 令牌读取，不在 C++ 里复写数值
    const qreal padding = qml_token(canvas, "Metrics", "spacingXl", 16.0);
    const qreal bottom_inset = qml_token(canvas, "Metrics", "spacingLg", 12.0);
    const qreal zoom_min = qml_token(canvas, "ComponentStyle", "graphZoomMin", 0.4);
    const qreal zoom_max = qml_token(canvas, "ComponentStyle", "graphZoomMax", 2.0);

    bool has_node = false;
    qreal min_x = 0.0;
    qreal min_y = 0.0;
    qreal max_x = 0.0;
    qreal max_y = 0.0;
    for (const QVariant& entry : rule_graph_->nodes()) {
        const QVariantMap node = entry.toMap();
        const qreal x = node.value(QStringLiteral("x")).toReal();
        const qreal y = node.value(QStringLiteral("y")).toReal();
        // 节点高度与 QML 委托同一公式：头 + 端口内缩 + 端口数 × 端口间距 + 底部留白
        const qreal node_height = node.value(QStringLiteral("headerHeight")).toReal() + node.value(QStringLiteral("portInset")).toReal() + qMax(1, node.value(QStringLiteral("inputCount")).toInt()) * node.value(QStringLiteral("portSpacing")).toReal() + bottom_inset;
        const qreal right = x + node.value(QStringLiteral("width")).toReal();
        const qreal bottom = y + node_height;
        if (!has_node) {
            has_node = true;
            min_x = x;
            min_y = y;
            max_x = right;
            max_y = bottom;
            continue;
        }
        min_x = qMin(min_x, x);
        min_y = qMin(min_y, y);
        max_x = qMax(max_x, right);
        max_y = qMax(max_y, bottom);
    }

    // 无节点：回到 1.0 并把世界原点摆在画布中央
    qreal zoom = 1.0;
    qreal pan_x = canvas->width() / 2.0;
    qreal pan_y = canvas->height() / 2.0;
    if (has_node) {
        const qreal box_width = qMax<qreal>(1.0, max_x - min_x);
        const qreal box_height = qMax<qreal>(1.0, max_y - min_y);
        const qreal available_width = qMax<qreal>(1.0, canvas->width() - padding * 2.0);
        const qreal available_height = qMax<qreal>(1.0, canvas->height() - padding * 2.0);
        zoom = qBound(zoom_min,
            qMin(available_width / box_width, available_height / box_height), zoom_max);
        pan_x = padding + (available_width - box_width * zoom) / 2.0 - min_x * zoom;
        pan_y = padding + (available_height - box_height * zoom) / 2.0 - min_y * zoom;
    }
    canvas->setProperty("zoom", zoom);
    canvas->setProperty("panX", pan_x);
    canvas->setProperty("panY", pan_y);
}

void UiConnector::on_channel_toggle_clicked() {
    auto* button = sender();
    if (button == nullptr || home_ == nullptr) {
        return;
    }
    const QString channel = channel_of(button->objectName());
    const bool running = channel == QStringLiteral("A") ? home_->running_a() : home_->running_b();
    if (!running) {
        home_->toggleChannel(channel);
        return;
    }
    // 关闭运行中的通道会立即停止输出，先确认再切换
    ask_confirm(tr("关闭通道 %1？").arg(channel), tr("关闭后该通道将停止输出。"), false,
        [this, channel]() {
            if (home_ != nullptr) {
                home_->toggleChannel(channel);
            }
        });
}

void UiConnector::on_wave_channel_clicked() {
    auto* button = sender();
    if (button == nullptr || wave_ == nullptr) {
        return;
    }
    const QString name = button->objectName();
    wave_->setTargetChannel(name.endsWith(QStringLiteral("AButton")) ? QStringLiteral("A") : QStringLiteral("B"));
}

void UiConnector::on_wave_select_clicked() {
    open_dialog("waveSelectDialog");
}

void UiConnector::on_wave_create_clicked() {
    if (wave_ == nullptr) {
        return;
    }
    close_dialog("waveSelectDialog");
    wave_->beginCreate();
    open_dialog("waveEditorDialog");
}

void UiConnector::on_wave_library_item_clicked() {
    auto* item = sender();
    if (item == nullptr || wave_ == nullptr) {
        return;
    }
    wave_->assignWave(item->property("waveName").toString());
    close_dialog("waveSelectDialog");
}

void UiConnector::on_wave_delete_clicked() {
    auto* item = sender();
    if (item == nullptr || wave_ == nullptr) {
        return;
    }
    const QString name = item->property("waveName").toString();
    if (name.isEmpty()) {
        return;
    }
    // 删除不可恢复：先确认，确认后按名字删除（委托可能在关闭列表时被销毁，故捕获名字）
    ask_confirm(tr("删除波形 %1？").arg(name), tr("删除后无法恢复，已指派到通道的引用会一并清除。"), true,
        [this, name]() {
            if (wave_ != nullptr) {
                wave_->deleteWave(name);
            }
        });
}

void UiConnector::on_wave_section_clicked() {
    auto* item = sender();
    if (item != nullptr && wave_ != nullptr) {
        wave_->selectSection(item->property("sectionIndex").toInt());
    }
}

void UiConnector::on_wave_add_section_clicked() {
    if (wave_ != nullptr) {
        wave_->addSection();
    }
}

void UiConnector::on_wave_remove_section_clicked() {
    if (wave_ != nullptr) {
        wave_->removeSelectedSection();
    }
}

void UiConnector::on_wave_apply_section_clicked() {
    if (wave_ == nullptr) {
        return;
    }
    auto value_of = [this](const char* name) {
        auto* spin = find(name);
        return spin == nullptr ? 0 : spin->property("value").toInt();
    };
    wave_->applySectionEdits(value_of("waveSectionDurationSpin"), value_of("waveSectionFreqStartSpin"),
        value_of("waveSectionFreqEndSpin"), value_of("waveSectionStrengthStartSpin"),
        value_of("waveSectionStrengthEndSpin"));
}

void UiConnector::on_wave_apply_raw_clicked() {
    if (wave_ == nullptr) {
        return;
    }
    auto* area = find("waveRawFramesArea");
    if (area != nullptr) {
        wave_->applyRawFrames(area->property("text").toString());
    }
}

void UiConnector::on_wave_save_clicked() {
    if (wave_ == nullptr) {
        return;
    }
    auto* name_field = find("waveNameField");
    const QString name = name_field == nullptr ? QString() : name_field->property("text").toString();
    // 结果反馈统一由 WaveBridge::lastError 与编辑器状态行承担：成功不弹 Toast，失败弹 danger
    wave_->saveDraft(name);
}

void UiConnector::on_wave_send_clicked() {
    auto* button = sender();
    if (button == nullptr || wave_ == nullptr) {
        return;
    }
    wave_->sendDraft(button->objectName().endsWith(QStringLiteral("AButton"))
                         ? QStringLiteral("A")
                         : QStringLiteral("B"));
}

void UiConnector::on_wave_editor_close_clicked() {
    auto* dialog = find("waveEditorDialog");
    if (dialog == nullptr) {
        LOG_MODULE("UiConnector", "on_wave_editor_close_clicked", LOG_WARN,
            "未找到对话框: waveEditorDialog");
        return;
    }
    const bool dirty = wave_ != nullptr && wave_->draft_dirty();
    if (!dirty) {
        QMetaObject::invokeMethod(dialog, "close");
        return;
    }
    // 有未保存草稿：关闭会丢弃这些改动，先确认
    ask_confirm(tr("放弃未保存的草稿？"), tr("波形编辑器有未保存的修改，关闭后将丢失。"), true,
        [guard = QPointer<QObject>(dialog)]() {
            if (!guard.isNull()) {
                QMetaObject::invokeMethod(guard.data(), "close");
            }
        });
}

void UiConnector::on_wave_select_close_clicked() {
    close_dialog("waveSelectDialog");
}

void UiConnector::on_ble_scan_clicked() {
    if (ble_ != nullptr) {
        ble_->startScan();
    }
}

void UiConnector::on_ble_device_clicked() {
    auto* item = sender();
    if (item != nullptr && ble_ != nullptr) {
        ble_->connectDevice(item->property("address").toString());
    }
}

void UiConnector::on_ble_disconnect_clicked() {
    if (ble_ != nullptr) {
        ble_->disconnectDevice();
    }
}

void UiConnector::on_log_export_clicked() {
    if (log_ != nullptr) {
        log_->exportLogs();
    }
}

void UiConnector::on_log_clear_clicked() {
    if (log_ != nullptr) {
        log_->clear();
    }
}

void UiConnector::on_log_list_movement_ended() {
    auto* list = sender();
    if (list == nullptr) {
        return;
    }
    // 滚动结束：到底部则恢复跟随，否则记住用户停留位置
    const bool at_end = list->property("atYEnd").toBool();
    list->setProperty("stickToBottom", at_end);
    if (!at_end) {
        log_scroll_y_ = list->property("contentY").toReal();
    }
}

void UiConnector::on_log_messages_changed() {
    auto* list = log_list_.data();
    if (list == nullptr) {
        return;
    }
    // 日志模型每次整体替换：ListView 会把第一个条目重新锚回顶部（contentY 归零）。
    // 等本轮事件循环把列表重新布局完，再按 stickToBottom 写回贴底或用户停留位置。
    QMetaObject::invokeMethod(list, [this, list]() {
        if (list->property("stickToBottom").toBool()) {
            list->setProperty("contentY",
                qMax<qreal>(0.0,
                    list->property("contentHeight").toReal() - list->property("height").toReal()));
        }
        else if (list->property("count").toInt() > 0) {
            list->setProperty("contentY", log_scroll_y_);
        } }, Qt::QueuedConnection);
}

void UiConnector::on_log_level_changed() {
    if (log_ == nullptr) {
        return;
    }
    auto* combo = find("logLevelCombo");
    log_->setLevelFilter(combo == nullptr ? 0 : combo->property("currentIndex").toInt());
}

void UiConnector::on_ble_strength_clicked() {
    auto* button = sender();
    if (button == nullptr || ble_ == nullptr) {
        return;
    }
    const QString name = button->objectName();
    const int channel = name.contains(QStringLiteral("StrengthA")) ? 1 : 2;
    const int delta = name.contains(QStringLiteral("Increase")) ? 1 : -1;
    ble_->setStrength(channel, delta > 0 ? 1 : 0, 1);
}

void UiConnector::on_config_connect_clicked() {
    if (device_ == nullptr) {
        return;
    }
    auto* ip_field = find("configIpField");
    auto* port_field = find("configPortField");
    const QString ip = (ip_field != nullptr) ? ip_field->property("text").toString() : QString();
    const int port = (port_field != nullptr) ? port_field->property("text").toInt() : 0;
    if (!ip.isEmpty() && port > 0) {
        device_->setEndpoint(ip, port);
    }
    if (device_->connected()) {
        device_->disconnectDevice();
    }
    else {
        device_->connectDevice();
    }
}

void UiConnector::on_theme_select_clicked() {
    open_dialog("themePresetDialog");
}

void UiConnector::on_theme_custom_clicked() {
    auto* dialog = find("customThemeDialog");
    if (dialog == nullptr || theme_ == nullptr) {
        return;
    }
    dialog->setProperty("customPrimary", theme_->primary());
    dialog->setProperty("customSecondary", theme_->secondary());
    open_dialog("customThemeDialog");
}

void UiConnector::on_theme_preset_clicked() {
    auto* item = sender();
    if (item == nullptr || theme_ == nullptr) {
        return;
    }
    theme_->applyPreset(item->property("mode").toString());
    close_dialog("themePresetDialog");
}

void UiConnector::on_custom_theme_save_clicked() {
    auto* dialog = find("customThemeDialog");
    if (dialog == nullptr || theme_ == nullptr) {
        return;
    }
    theme_->applyCustom(dialog->property("customPrimary").value<QColor>(),
        dialog->property("customSecondary").value<QColor>());
    close_dialog("customThemeDialog");
    // 自定义主题不落库、无持久化痕迹，只能靠 Toast 说明「已保存并应用」
    if (app_ != nullptr) {
        app_->showToast(QStringLiteral("已保存并应用自定义主题"), AppBridge::StatusSuccess);
    }
}

void UiConnector::on_custom_primary_clicked() {
    auto* dialog = find("customThemeDialog");
    auto* picker = find("primaryColorDialog");
    if (dialog != nullptr && picker != nullptr) {
        picker->setProperty("color", dialog->property("customPrimary"));
        QMetaObject::invokeMethod(picker, "open");
    }
}

void UiConnector::on_custom_secondary_clicked() {
    auto* dialog = find("customThemeDialog");
    auto* picker = find("secondaryColorDialog");
    if (dialog != nullptr && picker != nullptr) {
        picker->setProperty("color", dialog->property("customSecondary"));
        QMetaObject::invokeMethod(picker, "open");
    }
}

void UiConnector::on_primary_color_accepted() {
    auto* picker = sender();
    auto* dialog = find("customThemeDialog");
    if (picker != nullptr && dialog != nullptr) {
        dialog->setProperty("customPrimary", picker->property("selectedColor"));
    }
}

void UiConnector::on_secondary_color_accepted() {
    auto* picker = sender();
    auto* dialog = find("customThemeDialog");
    if (picker != nullptr && dialog != nullptr) {
        dialog->setProperty("customSecondary", picker->property("selectedColor"));
    }
}

void UiConnector::on_module_period_applied() {
    if (module_ == nullptr) {
        return;
    }
    auto* combo = find("modulePeriodCombo");
    module_->applyAllPeriod(combo != nullptr ? combo->property("currentIndex").toInt() : 0);
}

void UiConnector::on_plugin_toggle_clicked() {
    auto* button = sender();
    if (button != nullptr && module_ != nullptr) {
        module_->togglePlugin(button->property("fileName").toString());
    }
}

void UiConnector::on_module_card_clicked() {
    auto* item = sender();
    if (item == nullptr || module_ == nullptr) {
        return;
    }
    module_->selectModule(item->property("moduleName").toString());
    open_dialog("moduleValuesDialog");
}
