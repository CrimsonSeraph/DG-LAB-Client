/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "UiConnector.h"

#include "AppBridge.h"
#include "DebugLog.h"
#include "DeviceController.h"
#include "HomeBridge.h"
#include "ModuleBridge.h"
#include "ThemeManager.h"
#include "WaveBridge.h"

#include <QColor>
#include <QHash>
#include <QMetaObject>
#include <QQuickItem>

namespace {

    /// @brief 导航按钮 objectName -> 页面索引
    const QHash<QString, int>& nav_bindings() {
        static const QHash<QString, int> bindings = {
            { QStringLiteral("navHomeButton"), AppBridge::HomePage },
            { QStringLiteral("navConfigButton"), AppBridge::ConfigPage },
            { QStringLiteral("navModuleButton"), AppBridge::ModulePage },
            { QStringLiteral("navAboutButton"), AppBridge::AboutPage }
        };
        return bindings;
    }

    /// @brief 从 "channelAStrengthSpin" 这类名字中解析通道（A/B）
    QString channel_of(const QString& object_name) {
        const int index = object_name.indexOf(QStringLiteral("channel"));
        if (index < 0 || index + 8 >= object_name.size()) {
            return QString();
        }
        return object_name.mid(index + 7, 1);
    }

} // namespace

UiConnector::UiConnector(AppBridge* app, HomeBridge* home, ThemeManager* theme,
    DeviceController* device, ModuleBridge* module, WaveBridge* wave, QObject* parent)
    : QObject(parent)
    , app_(app)
    , home_(home)
    , theme_(theme)
    , device_(device)
    , module_(module)
    , wave_(wave) {
}

void UiConnector::attach(QObject* root_object) {
    if (root_object == nullptr) {
        LOG_MODULE("UiConnector", "attach", LOG_ERROR, "根对象为空，无法建立 QML 连接");
        return;
    }
    root_ = root_object;

    for (auto it = nav_bindings().constBegin(); it != nav_bindings().constEnd(); ++it) {
        auto* button = root_object->findChild<QObject*>(it.key());
        if (button == nullptr) {
            LOG_MODULE("UiConnector", "attach", LOG_WARN, "未找到导航按钮: " + it.key().toStdString());
            continue;
        }
        QObject::connect(button, SIGNAL(clicked()), this, SLOT(on_nav_button_clicked()));
    }
    connect_clicked(root_object, "homeModuleEntryButton", SLOT(on_home_module_entry_clicked()));
    connect_clicked(root_object, "homeRuleEditorButton", SLOT(on_rule_editor_clicked()));
    connect_clicked(root_object, "configRuleEditorButton", SLOT(on_rule_editor_clicked()));

    connect_clicked(root_object, "channelAStartButton", SLOT(on_channel_toggle_clicked()));
    connect_clicked(root_object, "channelBStartButton", SLOT(on_channel_toggle_clicked()));
    connect_clicked(root_object, "channelAStrengthIncrease", SLOT(on_strength_adjust_clicked()));
    connect_clicked(root_object, "channelAStrengthDecrease", SLOT(on_strength_adjust_clicked()));
    connect_clicked(root_object, "channelBStrengthIncrease", SLOT(on_strength_adjust_clicked()));
    connect_clicked(root_object, "channelBStrengthDecrease", SLOT(on_strength_adjust_clicked()));
    connect_clicked(root_object, "channelASelectWaveButton", SLOT(on_wave_select_clicked()));
    connect_clicked(root_object, "channelBSelectWaveButton", SLOT(on_wave_select_clicked()));
    const char* spins[] = { "channelAStrengthSpin", "channelBStrengthSpin" };
    for (const char* name : spins) {
        auto* spin = root_object->findChild<QObject*>(QString::fromLatin1(name));
        if (spin != nullptr) {
            QObject::connect(spin, SIGNAL(valueModified()), this, SLOT(on_strength_modified()));
        }
    }

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
            QObject::connect(child, SIGNAL(clicked()), this, slot);
        }
        scan_dynamic_items(child, item_name, slot);
    }
}

void UiConnector::on_tracked_children_changed() {
    for (const auto& watch : watches_) {
        scan_dynamic_items(watch.owner, watch.item_name, watch.slot);
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
    app_->setStatus(QStringLiteral("规则编辑器将在后续阶段实现"));
}

void UiConnector::on_channel_toggle_clicked() {
    auto* button = sender();
    if (button != nullptr && home_ != nullptr) {
        home_->toggleChannel(channel_of(button->objectName()));
    }
}

void UiConnector::on_strength_modified() {
    auto* spin = sender();
    if (spin != nullptr && home_ != nullptr) {
        home_->setChannelStrength(channel_of(spin->objectName()), spin->property("value").toInt());
    }
}

void UiConnector::on_strength_adjust_clicked() {
    auto* button = sender();
    if (button == nullptr || home_ == nullptr) {
        return;
    }
    const QString name = button->objectName();
    home_->adjustChannelStrength(channel_of(name), name.contains(QStringLiteral("Increase")) ? 1 : -1);
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
    auto* button = sender();
    if (button == nullptr || wave_ == nullptr) {
        return;
    }
    // 首页通道按钮会先指定目标通道，再打开波形库
    if (button->objectName().startsWith(QStringLiteral("channel"))) {
        wave_->setTargetChannel(channel_of(button->objectName()));
    }
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
    if (item != nullptr && wave_ != nullptr) {
        wave_->deleteWave(item->property("waveName").toString());
    }
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
    if (name.isEmpty()) {
        app_->setStatus(QStringLiteral("请先填写波形名称"));
        return;
    }
    if (wave_->saveDraft(name)) {
        app_->setStatus(QStringLiteral("波形已保存：") + name);
    }
    else {
        app_->setStatus(QStringLiteral("波形保存失败"));
    }
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
    close_dialog("waveEditorDialog");
}

void UiConnector::on_wave_select_close_clicked() {
    close_dialog("waveSelectDialog");
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
