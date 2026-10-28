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
#include "IpSelector.h"
#include "LogBridge.h"
#include "ModuleBridge.h"
#include "RuleGraphBridge.h"
#include "ThemeManager.h"
#include "UpdateBridge.h"
#include "WaveBridge.h"

#include <QColor>
#include <QGuiApplication>
#include <QHash>
#include <QJSValue>
#include <QKeyEvent>
#include <QMetaObject>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScreen>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <algorithm>
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

    /// @brief 从 "channelAStartButton" / "miniWindowChannelAToggleButton" 这类名字中解析通道（A/B）
    /// @note 大小写不敏感：既有的看板按钮是 "channelA..."，迷你窗按钮是 "...ChannelA..."
    QString channel_of(const QString& object_name) {
        const qsizetype index = object_name.indexOf(QStringLiteral("channel"), 0, Qt::CaseInsensitive);
        if (index < 0 || index + 8 >= object_name.size()) {
            return QString();
        }
        return object_name.mid(index + 7, 1).toUpper();
    }

    constexpr qsizetype kMiniWindowWaveFrames = 32; ///< 迷你窗滚动窗口帧数（每帧 100 ms）

    // 强度指令参数取值，语义见 include/ble/CoyoteBleController.h:56：
    // setStrength(channel: 1=A / 2=B；mode: 0 减 / 1 增 / 2 设为；value: 步数或目标值)
    constexpr int kStrengthChannelA = 1;     ///< 通道号：A
    constexpr int kStrengthChannelB = 2;     ///< 通道号：B
    constexpr int kStrengthModeDecrease = 0; ///< 强度模式：减少
    constexpr int kStrengthModeIncrease = 1; ///< 强度模式：增加
    constexpr int kStrengthStep = 1;         ///< 单次点击的调节步数

    /// @brief 取以 cursor 结尾的最近若干帧，序列不足时按实际长度取
    QVariantList rolling_window(const QVariantList& series, qint64 cursor) {
        QVariantList window;
        const qsizetype total = series.size();
        if (total <= 0) {
            return window;
        }
        const qsizetype count = std::min<qsizetype>(kMiniWindowWaveFrames, total);
        const qsizetype head = static_cast<qsizetype>(cursor % total);
        for (qsizetype step = count; step > 0; --step) {
            window.append(series.at((head - (step - 1) + total) % total));
        }
        return window;
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
        // 导入导出：名称冲突打开冲突弹窗，结果与失败统一走 Toast
        QObject::connect(wave_, &WaveBridge::importConflictRequested, this,
            &UiConnector::on_wave_conflict_requested);
        QObject::connect(wave_, &WaveBridge::importFinished, this,
            [this](const QString& summary, bool clean) {
                if (app_ != nullptr) {
                    app_->showToast(summary, clean ? AppBridge::StatusInfo : AppBridge::StatusWarning);
                }
            });
        QObject::connect(wave_, &WaveBridge::exportFinished, this, [this](const QString& summary) {
            if (app_ != nullptr) {
                app_->showToast(summary, AppBridge::StatusInfo);
            }
        });
        QObject::connect(wave_, &WaveBridge::operationFailed, this, [this](const QString& message) {
            if (app_ != nullptr) {
                app_->showToast(message, AppBridge::StatusDanger);
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
    if (app_ != nullptr) {
        // 带插件标识的导航：切页在 AppBridge 完成，这里负责把页面选中项定位到目标插件
        QObject::connect(app_, &AppBridge::pluginDetailRequested, this,
            &UiConnector::on_plugin_detail_requested);
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
    connect_clicked(root_object, "homePluginEntryButton", SLOT(on_home_plugin_entry_clicked()));
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

    // 迷你窗：首页入口打开窗口，锁定与关闭按钮由这里连接
    connect_clicked(root_object, "homeMiniWindowEntryButton", SLOT(on_home_mini_window_clicked()));
    setup_mini_window(root_object);
    connect_clicked(root_object, "miniWindowLockButton", SLOT(on_mini_window_lock_clicked()));
    connect_clicked(root_object, "miniWindowCloseButton", SLOT(on_mini_window_close_clicked()));
    // 迷你窗交互：强度增减复用蓝牙强度槽，通道启停复用看板启停槽（两个槽都按 objectName 解析）
    connect_clicked(root_object, "miniWindowStrengthADecrease", SLOT(on_ble_strength_clicked()));
    connect_clicked(root_object, "miniWindowStrengthAIncrease", SLOT(on_ble_strength_clicked()));
    connect_clicked(root_object, "miniWindowStrengthBDecrease", SLOT(on_ble_strength_clicked()));
    connect_clicked(root_object, "miniWindowStrengthBIncrease", SLOT(on_ble_strength_clicked()));
    connect_clicked(root_object, "miniWindowChannelAToggleButton", SLOT(on_channel_toggle_clicked()));
    connect_clicked(root_object, "miniWindowChannelBToggleButton", SLOT(on_channel_toggle_clicked()));
    connect_clicked(root_object, "miniWindowSizeSmallButton", SLOT(on_mini_window_size_clicked()));
    connect_clicked(root_object, "miniWindowSizeMediumButton", SLOT(on_mini_window_size_clicked()));
    connect_clicked(root_object, "miniWindowSizeLargeButton", SLOT(on_mini_window_size_clicked()));
    // 透明度滑杆没有 clicked：拖动由 AppSlider 自己的 moved() 转发，一次拖动只落盘一次
    if (auto* opacity_slider = root_object->findChild<QObject*>(QStringLiteral("miniWindowOpacitySlider"))) {
        QObject::connect(opacity_slider, SIGNAL(moved()), this, SLOT(on_mini_window_opacity_moved()));
    }
    else {
        LOG_MODULE("UiConnector", "attach", LOG_WARN, "未找到控件: miniWindowOpacitySlider");
    }

    connect_clicked(root_object, "configConnectButton", SLOT(on_config_connect_clicked()));
    // 网卡 IP 选择：入口在连接卡表单下方，对话框与候选列表全部按 objectName 连接
    ip_selector_ = (device_ != nullptr) ? qobject_cast<IpSelector*>(device_->ip_selector()) : nullptr;
    connect_clicked(root_object, "configSelectIpButton", SLOT(on_config_select_ip_clicked()));
    connect_clicked(root_object, "ipBlacklistAddButton", SLOT(on_ip_blacklist_add_clicked()));
    connect_clicked(root_object, "ipWhitelistAddButton", SLOT(on_ip_whitelist_add_clicked()));
    connect_clicked(root_object, "ipSelectAutoButton", SLOT(on_ip_auto_clicked()));
    connect_clicked(root_object, "ipSelectConfirmButton", SLOT(on_ip_select_confirm_clicked()));
    connect_clicked(root_object, "ipSelectCancelButton", SLOT(on_ip_select_cancel_clicked()));
    watch_items(root_object, "ipBlacklistList", QStringLiteral("ipBlacklistRemoveClick"),
        SLOT(on_ip_blacklist_remove_clicked()));
    watch_items(root_object, "ipWhitelistList", QStringLiteral("ipWhitelistRemoveClick"),
        SLOT(on_ip_whitelist_remove_clicked()));
    watch_items(root_object, "ipCandidateList", QStringLiteral("ipCandidateClick"),
        SLOT(on_ip_candidate_clicked()));
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

    // 统一查询周期：控件在插件中心，下拉与「应用」共用一个槽
    connect_clicked(root_object, "modulePeriodApplyButton", SLOT(on_module_period_applied()));
    if (auto* combo = root_object->findChild<QObject*>(QStringLiteral("modulePeriodCombo"))) {
        QObject::connect(combo, SIGNAL(activated(int)), this, SLOT(on_module_period_applied()));
    }

    // 插件详情：选中插件变化时把模块标识交给模块桥接，供「数值」页签读取
    if (auto* detail = root_object->findChild<QObject*>(QStringLiteral("pluginDetail"))) {
        if (detail->metaObject()->indexOfSignal("itemChanged()") >= 0) {
            QObject::connect(detail, SIGNAL(itemChanged()), this,
                SLOT(on_plugin_detail_item_changed()));
        }
        else {
            LOG_MODULE("UiConnector", "attach", LOG_WARN,
                "pluginDetail 缺少 item 变更信号，数值页签不会跟随选中插件刷新");
        }
    }

    // 波形库与编辑器
    connect_clicked(root_object, "waveChannelAButton", SLOT(on_wave_channel_clicked()));
    connect_clicked(root_object, "waveChannelBButton", SLOT(on_wave_channel_clicked()));
    connect_clicked(root_object, "waveSelectButton", SLOT(on_wave_select_clicked()));
    connect_clicked(root_object, "waveCreateButton", SLOT(on_wave_create_clicked()));
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
    // 波形导入导出：入口在通道波形卡底部，格式与范围在导出对话框里选择
    connect_clicked(root_object, "waveImportButton", SLOT(on_wave_import_clicked()));
    connect_clicked(root_object, "waveExportButton", SLOT(on_wave_export_clicked()));
    connect_clicked(root_object, "waveExportCancelButton", SLOT(on_wave_export_cancel_clicked()));
    connect_clicked(root_object, "waveExportConfirmButton", SLOT(on_wave_export_confirm_clicked()));
    connect_clicked(root_object, "waveConflictOverwriteButton", SLOT(on_wave_conflict_overwrite_clicked()));
    connect_clicked(root_object, "waveConflictRenameButton", SLOT(on_wave_conflict_rename_clicked()));
    connect_clicked(root_object, "waveConflictSkipButton", SLOT(on_wave_conflict_skip_clicked()));
    if (auto* export_format = root_object->findChild<QObject*>(QStringLiteral("waveExportFormatSegment"))) {
        QObject::connect(export_format, SIGNAL(selected(int)), this,
            SLOT(on_wave_export_format_selected(int)));
    }
    if (auto* export_scope = root_object->findChild<QObject*>(QStringLiteral("waveExportScopeSegment"))) {
        QObject::connect(export_scope, SIGNAL(selected(int)), this,
            SLOT(on_wave_export_scope_selected(int)));
    }
    // 系统文件对话框：只用它挑选路径，读写全部落在 WaveLibrary
    if (auto* export_file = root_object->findChild<QObject*>(QStringLiteral("waveExportFileDialog"))) {
        QObject::connect(export_file, SIGNAL(accepted()), this, SLOT(on_wave_export_file_accepted()));
    }
    if (auto* import_file = root_object->findChild<QObject*>(QStringLiteral("waveImportFileDialog"))) {
        QObject::connect(import_file, SIGNAL(accepted()), this, SLOT(on_wave_import_file_accepted()));
    }

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
    // 手动导出选项：级别 / 范围（三态）/ 导出目录，写入 user.json 的 app.log.manual.*
    if (auto* combo = root_object->findChild<QObject*>(QStringLiteral("logExportLevelCombo"))) {
        QObject::connect(combo, SIGNAL(activated(int)), this, SLOT(on_log_export_level_changed()));
    }
    if (auto* combo = root_object->findChild<QObject*>(QStringLiteral("logExportRangeCombo"))) {
        QObject::connect(combo, SIGNAL(activated(int)), this, SLOT(on_log_export_range_changed()));
    }
    connect_clicked(root_object, "logExportDirApplyButton", SLOT(on_log_export_dir_applied()));
    // 日志列表贴底跟随：用户滚动结束后按 atYEnd 维护 QML 侧 stickToBottom，
    // 上滚回看历史时不跟新日志，滚回底部后恢复跟随。
    auto* log_list = root_object->findChild<QObject*>(QStringLiteral("logList"));
    if (log_list == nullptr) {
        LOG_MODULE("UiConnector", "attach", LOG_WARN, "未找到控件: logList");
    }
    else {
        log_list_ = log_list;
        QObject::connect(log_list, SIGNAL(movementEnded()), this, SLOT(on_log_list_movement_ended()));
        if (log_ != nullptr) {
            QObject::connect(log_, &LogBridge::messagesChanged, this, &UiConnector::on_log_messages_changed);
        }
    }

    // 更新模块：入口在左下角面板，状态与动作全部落在独立的更新对话框
    connect_clicked(root_object, "panelCheckUpdateButton", SLOT(on_panel_check_update_clicked()));
    connect_clicked(root_object, "updateDialogCheckButton", SLOT(on_check_update_clicked()));
    connect_clicked(root_object, "updateDialogDownloadButton", SLOT(on_update_download_clicked()));
    connect_clicked(root_object, "updateDialogApplyButton", SLOT(on_update_apply_clicked()));
    connect_clicked(root_object, "updateDialogReleaseUrlButton", SLOT(on_update_open_release_clicked()));
    connect_clicked(root_object, "updateDialogCloseButton", SLOT(on_update_dialog_close_clicked()));

    LOG_MODULE("UiConnector", "attach", LOG_INFO, "QML 连接已完成");
}

QObject* UiConnector::find(const char* object_name) const {
    if (root_.isNull()) {
        return nullptr;
    }
    return root_->findChild<QObject*>(QString::fromLatin1(object_name));
}

void UiConnector::apply_selected_ip(const QString& ip) {
    if (device_ == nullptr || ip.isEmpty()) {
        return;
    }
    auto* port_field = find("configPortField");
    const int port = (port_field != nullptr) ? port_field->property("text").toInt() : device_->port();
    // 先把地址写回地址栏，再走既有 setEndpoint：地址栏、绑定地址与二维码载荷不会分叉
    if (auto* ip_field = find("configIpField")) {
        ip_field->setProperty("text", ip);
    }
    device_->setEndpoint(ip, port > 0 ? port : device_->port());
}

void UiConnector::add_keyword_from_field(const char* field_name, bool whitelist) {
    if (ip_selector_ == nullptr) {
        return;
    }
    auto* field = find(field_name);
    const QString keyword = (field != nullptr) ? field->property("text").toString().trimmed() : QString();
    if (keyword.isEmpty()) {
        if (app_ != nullptr) {
            app_->showToast(QStringLiteral("关键词不能为空"), AppBridge::StatusWarning);
        }
        return;
    }
    const bool added = whitelist ? ip_selector_->add_whitelist_keyword(keyword)
                                 : ip_selector_->add_blacklist_keyword(keyword);
    if (!added) {
        return;
    }
    if (field != nullptr) {
        field->setProperty("text", QString());
    }
    ip_selector_->refresh_candidates();
}

void UiConnector::setup_mini_window(QObject* root_object) {
    auto* window = root_object->findChild<QQuickWindow*>(QStringLiteral("miniWindow"));
    if (window == nullptr) {
        LOG_MODULE("UiConnector", "setup_mini_window", LOG_WARN,
            "未找到迷你窗 miniWindow，迷你窗入口不可用");
        return;
    }
    mini_window_ = window;

    QScreen* primary = QGuiApplication::primaryScreen();
    if (primary == nullptr) {
        LOG_MODULE("UiConnector", "setup_mini_window", LOG_WARN, "无可用屏幕信息，迷你窗保持默认位置");
        return;
    }

    // 位置取上次保存值，未保存过时落在主屏右上角；宽度由大小档位、高度由内容决定
    const bool restored = home_ != nullptr && home_->mini_window_width() > 0;
    const qreal margin = qml_token(root_object, "Metrics", "spacing3xl", 28.0);
    const int width = std::max(1, window->width());
    const int height = std::max(1, window->height());

    QPoint position(0, 0);
    if (restored) {
        position = QPoint(home_->mini_window_x(), home_->mini_window_y());
    }
    QScreen* screen = QGuiApplication::screenAt(
        QRect(position, QSize(width, height)).center());
    if (screen == nullptr) {
        // 落点不在任何屏幕上（拔掉外接屏 / 分辨率变更）：回到主屏
        screen = primary;
    }
    const QRect area = screen->availableGeometry();
    const int visible_width = std::min(width, area.width());
    const int visible_height = std::min(height, area.height());
    if (!restored) {
        position = QPoint(static_cast<int>(area.right() - visible_width - margin * 2),
            static_cast<int>(area.top() + margin * 2));
    }
    const int x = std::clamp(position.x(), area.left(),
        std::max(area.left(), area.right() - visible_width + 1));
    const int y = std::clamp(position.y(), area.top(),
        std::max(area.top(), area.bottom() - visible_height + 1));

    mini_window_save_timer_.setSingleShot(true);
    mini_window_save_timer_.setInterval(400);
    connect(&mini_window_save_timer_, &QTimer::timeout, this,
        &UiConnector::save_mini_window_state);
    connect(window, &QWindow::xChanged, this, &UiConnector::on_mini_window_geometry_changed);
    connect(window, &QWindow::yChanged, this, &UiConnector::on_mini_window_geometry_changed);
    connect(window, &QWindow::widthChanged, this, &UiConnector::on_mini_window_geometry_changed);
    connect(window, &QWindow::heightChanged, this, &UiConnector::on_mini_window_geometry_changed);

    // 波形滚动只在窗口可见时跑：100 ms 一拍，与波形帧长度一致（QML 侧不另开计时器）
    mini_window_wave_timer_.setInterval(100);
    connect(&mini_window_wave_timer_, &QTimer::timeout, this, &UiConnector::on_mini_window_wave_advanced);
    connect(window, &QWindow::visibleChanged, this, &UiConnector::on_mini_window_visibility_changed);
    if (wave_ != nullptr) {
        connect(wave_, &WaveBridge::dataChanged, this, &UiConnector::on_mini_window_wave_reset);
    }

    // 只挪位置：宽高由 QML 绑定（大小档位 + 内容高度）决定，setGeometry 会把绑定打断
    window->setPosition(x, y);
    // 首次运行没有存档：把落点写回配置，下次启动从同一位置恢复
    save_mini_window_state();
    LOG_MODULE("UiConnector", "setup_mini_window", LOG_DEBUG,
        "迷你窗位置恢复: " << x << "," << y << " " << width << "x" << height);
}

void UiConnector::save_mini_window_state() {
    if (mini_window_.isNull() || home_ == nullptr) {
        return;
    }
    home_->saveMiniWindowState(mini_window_->x(), mini_window_->y(),
        mini_window_->width(), mini_window_->height());
}

void UiConnector::apply_mini_window_wave() {
    if (home_ == nullptr || wave_ == nullptr) {
        return;
    }
    home_->setMiniWindowWave(rolling_window(wave_->amplitude_a(), mini_window_wave_cursor_),
        rolling_window(wave_->amplitude_b(), mini_window_wave_cursor_));
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

void UiConnector::open_file_dialog(const char* object_name, const QString& suggested_path) {
    auto* dialog = find(object_name);
    if (dialog == nullptr) {
        LOG_MODULE("UiConnector", "open_file_dialog", LOG_WARN,
            std::string("未找到文件对话框: ") + object_name);
        return;
    }
    // 保存对话框：打开前写入建议文件名（一次性写属性，QML 侧不对 currentFile 绑定）
    if (!suggested_path.isEmpty()) {
        dialog->setProperty("currentFile", QUrl::fromLocalFile(suggested_path));
    }
    QMetaObject::invokeMethod(dialog, "open");
}

bool UiConnector::dialog_visible(const char* object_name) const {
    QObject* dialog = find(object_name);
    return dialog != nullptr && dialog->property("visible").toBool();
}

bool UiConnector::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() != QEvent::KeyPress || root_.isNull() || watched != root_.data()) {
        return QObject::eventFilter(watched, event);
    }
    auto* key_event = static_cast<QKeyEvent*>(event);
    // 确认弹层盖在最上面时键位归它：Esc = 取消、Enter = 确认（弹层是 NoAutoClose，自己不处理键位），
    // 也不穿透到底下的编辑器
    if (dialog_visible("confirmDialog")) {
        if (key_event->key() == Qt::Key_Escape) {
            on_confirm_canceled();
            return true;
        }
        if (key_event->key() == Qt::Key_Return || key_event->key() == Qt::Key_Enter) {
            auto* confirm_window = qobject_cast<QQuickWindow*>(watched);
            // 弹层内目前没有输入控件；将来加入时仍让输入框先拿到 Enter
            if (is_text_entry((confirm_window == nullptr) ? nullptr : confirm_window->activeFocusItem())) {
                return false;
            }
            on_confirm_accepted();
            return true;
        }
        return false;
    }
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
        if (dialog_visible("ipSelectDialog")) {
            on_ip_select_cancel_clicked();
            return true;
        }
        if (dialog_visible("waveExportDialog")) {
            on_wave_export_cancel_clicked();
            return true;
        }
        if (dialog_visible("waveConflictDialog")) {
            on_wave_conflict_cancel_clicked();
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
        if (dialog_visible("ipSelectDialog")) {
            on_ip_select_confirm_clicked();
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
    // 主窗口隐藏到托盘或最小化时先拉起：弹层是主窗口内的 Popup，主窗口不可见时弹层也看不见
    auto* host_window = qobject_cast<QQuickWindow*>(root_.data());
    if (host_window != nullptr && (!host_window->isVisible()
        || (host_window->windowState() & Qt::WindowMinimized) != 0)) {
        if (!host_window->isVisible()) {
            host_window->show();
        }
        if ((host_window->windowState() & Qt::WindowMinimized) != 0) {
            host_window->showNormal();
        }
        host_window->raise();
        host_window->requestActivate();
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

void UiConnector::on_panel_check_update_clicked() {
    // 面板按钮一次动作两件事：先打开更新对话框展示状态，再触发一次检查
    open_dialog("updateDialog");
    on_check_update_clicked();
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

void UiConnector::on_update_dialog_close_clicked() {
    // 关闭只收起对话框：下载状态在 UpdateBridge 里，重开后进度照常显示
    close_dialog("updateDialog");
}

void UiConnector::on_home_mini_window_clicked() {
    if (mini_window_.isNull()) {
        if (app_ != nullptr) {
            app_->showToast(QStringLiteral("迷你窗不可用"), AppBridge::StatusDanger);
        }
        return;
    }
    if ((mini_window_->windowState() & Qt::WindowMinimized) != 0) {
        mini_window_->showNormal();
    }
    else {
        mini_window_->show();
    }
    mini_window_->raise();
    mini_window_->requestActivate();
}

void UiConnector::on_mini_window_lock_clicked() {
    if (home_ != nullptr) {
        home_->toggleMiniWindowLock();
    }
}

void UiConnector::on_mini_window_close_clicked() {
    if (mini_window_.isNull()) {
        return;
    }
    save_mini_window_state();
    mini_window_->hide();
}

void UiConnector::on_mini_window_geometry_changed() {
    mini_window_save_timer_.start();
}

void UiConnector::on_mini_window_opacity_moved() {
    auto* slider = find("miniWindowOpacitySlider");
    if (slider == nullptr || home_ == nullptr) {
        return;
    }
    // 拖动中即时生效：QML 的 Window.opacity 绑在 home.miniWindowOpacity 上，落盘交给合并计时器
    home_->setMiniWindowOpacity(slider->property("value").toReal());
    mini_window_save_timer_.start();
}

void UiConnector::on_mini_window_size_clicked() {
    auto* button = sender();
    if (button == nullptr || home_ == nullptr) {
        return;
    }
    const QString name = button->objectName();
    if (name.endsWith(QStringLiteral("SmallButton"))) {
        home_->setMiniWindowSize(QStringLiteral("small"));
    }
    else if (name.endsWith(QStringLiteral("LargeButton"))) {
        home_->setMiniWindowSize(QStringLiteral("large"));
    }
    else {
        home_->setMiniWindowSize(QStringLiteral("medium"));
    }
    mini_window_save_timer_.start();
}

void UiConnector::on_mini_window_visibility_changed() {
    if (mini_window_.isNull()) {
        return;
    }
    if (!mini_window_->isVisible()) {
        mini_window_wave_timer_.stop();
        return;
    }
    mini_window_wave_cursor_ = 0;
    apply_mini_window_wave();
    mini_window_wave_timer_.start();
}

void UiConnector::on_mini_window_wave_reset() {
    mini_window_wave_cursor_ = 0;
    if (!mini_window_.isNull() && mini_window_->isVisible()) {
        apply_mini_window_wave();
    }
}

void UiConnector::on_mini_window_wave_advanced() {
    ++mini_window_wave_cursor_;
    apply_mini_window_wave();
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

void UiConnector::on_home_plugin_entry_clicked() {
    app_->navigate(AppBridge::PluginCenterPage);
}

// 定位到指定插件详情：解析插件键后在插件中心页设置选中项，目标不存在时只提示
void UiConnector::on_plugin_detail_requested(const QString& plugin_key) {
    auto* page = find("pluginCenterPage");
    if (page == nullptr) {
        LOG_MODULE("UiConnector", "on_plugin_detail_requested", LOG_WARN,
            "未找到插件中心页面: pluginCenterPage");
        return;
    }
    // 键可以是插件文件名或模块标识：首页看板持有文件名，插件详情的数值页签持有模块标识
    QString file_name;
    if (module_ != nullptr) {
        const QVariantList items = module_->property("modules").toList();
        for (const QVariant& entry : items) {
            const QVariantMap item = entry.toMap();
            if (item.value(QStringLiteral("kind")).toString() != QStringLiteral("plugin")) {
                continue;
            }
            const QString candidate = item.value(QStringLiteral("fileName")).toString();
            if (candidate.isEmpty()) {
                continue;
            }
            if (candidate == plugin_key
                || item.value(QStringLiteral("moduleName")).toString() == plugin_key) {
                file_name = candidate;
                break;
            }
        }
    }
    if (file_name.isEmpty()) {
        // 目标插件未扫描到或已卸载：停留在插件中心列表，不打开空白详情
        if (app_ != nullptr) {
            app_->showToast(tr("未找到插件 %1，已打开插件中心").arg(plugin_key),
                AppBridge::StatusWarning);
        }
        return;
    }
    page->setProperty("selectedFile", file_name);
    page->setProperty("detailVisible", true);
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

void UiConnector::on_wave_import_clicked() {
    open_file_dialog("waveImportFileDialog", QString());
}

void UiConnector::on_wave_export_clicked() {
    open_dialog("waveExportDialog");
}

void UiConnector::on_wave_export_cancel_clicked() {
    close_dialog("waveExportDialog");
}

void UiConnector::on_wave_export_confirm_clicked() {
    if (wave_ == nullptr) {
        return;
    }
    // 不合法组合（.pulse + 全部波形、目标通道未绑定波形）一律不静默导出
    if (!wave_->export_allowed()) {
        if (app_ != nullptr) {
            app_->showToast(wave_->export_hint(), AppBridge::StatusWarning);
        }
        return;
    }
    close_dialog("waveExportDialog");
    open_file_dialog("waveExportFileDialog", wave_->export_suggested_path());
}

void UiConnector::on_wave_export_format_selected(int index) {
    if (wave_ != nullptr) {
        wave_->setExportFormat(index);
    }
}

void UiConnector::on_wave_export_scope_selected(int index) {
    if (wave_ != nullptr) {
        wave_->setExportScope(index);
    }
}

void UiConnector::on_wave_export_file_accepted() {
    auto* dialog = find("waveExportFileDialog");
    if (dialog == nullptr || wave_ == nullptr) {
        return;
    }
    const QString path = dialog->property("selectedFile").toUrl().toLocalFile();
    wave_->exportToFile(path);
}

void UiConnector::on_wave_import_file_accepted() {
    auto* dialog = find("waveImportFileDialog");
    if (dialog == nullptr || wave_ == nullptr) {
        return;
    }
    const QString path = dialog->property("selectedFile").toUrl().toLocalFile();
    wave_->importFromFile(path);
}

void UiConnector::on_wave_conflict_requested(int count) {
    if (wave_ == nullptr) {
        return;
    }
    // 重命名建议名由 C++ 给出：单条冲突可在弹窗里改写，多条冲突按序号自动生成
    if (auto* field = find("waveConflictRenameField")) {
        field->setProperty("text", wave_->pending_rename());
    }
    open_dialog("waveConflictDialog");
    LOG_MODULE("UiConnector", "on_wave_conflict_requested", LOG_INFO,
        "波形名称冲突 " << count << " 条，等待用户选择覆盖 / 重命名 / 跳过");
}

void UiConnector::on_wave_conflict_overwrite_clicked() {
    if (wave_ == nullptr) {
        return;
    }
    wave_->resolveConflict(QStringLiteral("overwrite"), QString());
    close_dialog("waveConflictDialog");
}

void UiConnector::on_wave_conflict_rename_clicked() {
    if (wave_ == nullptr) {
        return;
    }
    auto* field = find("waveConflictRenameField");
    const QString name = (field == nullptr) ? QString() : field->property("text").toString().trimmed();
    wave_->resolveConflict(QStringLiteral("rename"), name);
    close_dialog("waveConflictDialog");
}

void UiConnector::on_wave_conflict_skip_clicked() {
    if (wave_ == nullptr) {
        return;
    }
    wave_->resolveConflict(QStringLiteral("skip"), QString());
    close_dialog("waveConflictDialog");
}

void UiConnector::on_wave_conflict_cancel_clicked() {
    if (wave_ != nullptr) {
        wave_->cancelPendingImport();
    }
    close_dialog("waveConflictDialog");
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

void UiConnector::on_log_export_level_changed() {
    if (log_ == nullptr) {
        return;
    }
    auto* combo = find("logExportLevelCombo");
    log_->setExportLevel(combo == nullptr ? 0 : combo->property("currentIndex").toInt());
}

void UiConnector::on_log_export_range_changed() {
    if (log_ == nullptr) {
        return;
    }
    auto* combo = find("logExportRangeCombo");
    log_->setExportRange(combo == nullptr ? 1 : combo->property("currentIndex").toInt());
}

void UiConnector::on_log_export_dir_applied() {
    if (log_ == nullptr) {
        return;
    }
    auto* field = find("logExportDirField");
    if (field != nullptr) {
        log_->setExportDir(field->property("text").toString());
    }
}

void UiConnector::on_ble_strength_clicked() {
    auto* button = sender();
    if (button == nullptr || ble_ == nullptr) {
        return;
    }
    const QString name = button->objectName();
    const int channel = name.contains(QStringLiteral("StrengthA"))
        ? kStrengthChannelA : kStrengthChannelB;
    const int mode = name.contains(QStringLiteral("Increase"))
        ? kStrengthModeIncrease : kStrengthModeDecrease;
    ble_->setStrength(channel, mode, kStrengthStep);
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

void UiConnector::on_config_select_ip_clicked() {
    if (ip_selector_ == nullptr) {
        return;
    }
    ip_selector_->refresh_candidates();
    auto* dialog = find("ipSelectDialog");
    if (dialog != nullptr) {
        dialog->setProperty("pendingIp", device_ != nullptr ? device_->ip() : ip_selector_->active_ip());
    }
    open_dialog("ipSelectDialog");
}

void UiConnector::on_ip_blacklist_add_clicked() {
    add_keyword_from_field("ipBlacklistField", false);
}

void UiConnector::on_ip_blacklist_remove_clicked() {
    auto* button = sender();
    if (ip_selector_ == nullptr || button == nullptr) {
        return;
    }
    if (ip_selector_->remove_blacklist_keyword(button->property("keyword").toString())) {
        ip_selector_->refresh_candidates();
    }
}

void UiConnector::on_ip_whitelist_add_clicked() {
    add_keyword_from_field("ipWhitelistField", true);
}

void UiConnector::on_ip_whitelist_remove_clicked() {
    auto* button = sender();
    if (ip_selector_ == nullptr || button == nullptr) {
        return;
    }
    if (ip_selector_->remove_whitelist_keyword(button->property("keyword").toString())) {
        ip_selector_->refresh_candidates();
    }
}

void UiConnector::on_ip_candidate_clicked() {
    auto* item = sender();
    auto* dialog = find("ipSelectDialog");
    if (item == nullptr || dialog == nullptr) {
        return;
    }
    dialog->setProperty("pendingIp", item->property("address").toString());
}

void UiConnector::on_ip_auto_clicked() {
    if (ip_selector_ == nullptr) {
        return;
    }
    // 恢复自动选择：清掉手动选中后立即按名单选一次并生效，对话框同时收起
    ip_selector_->set_selected_ip(QString());
    ip_selector_->refresh_candidates();
    const QString ip = ip_selector_->active_ip();
    auto* dialog = find("ipSelectDialog");
    if (dialog != nullptr) {
        dialog->setProperty("pendingIp", ip);
    }
    apply_selected_ip(ip);
    close_dialog("ipSelectDialog");
}

void UiConnector::on_ip_select_confirm_clicked() {
    auto* dialog = find("ipSelectDialog");
    if (dialog == nullptr || ip_selector_ == nullptr) {
        return;
    }
    const QString ip = dialog->property("pendingIp").toString();
    if (ip.isEmpty()) {
        return;
    }
    ip_selector_->set_selected_ip(ip);
    apply_selected_ip(ip);
    close_dialog("ipSelectDialog");
}

void UiConnector::on_ip_select_cancel_clicked() {
    close_dialog("ipSelectDialog");
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

// 详情页选中插件变化时把模块标识交给模块桥接，供「数值」页签读取
void UiConnector::on_plugin_detail_item_changed() {
    if (module_ == nullptr) {
        return;
    }
    auto* detail = sender();
    if (detail == nullptr) {
        return;
    }
    QString module_name = detail->property("moduleName").toString();
    if (module_name.isEmpty()) {
        // 模块标识为空时从 item 字段取同一份标识
        const QVariantMap item = detail->property("item").value<QJSValue>().toVariant().toMap();
        module_name = item.value(QStringLiteral("moduleName")).toString();
    }
    module_->selectModule(module_name);
}
