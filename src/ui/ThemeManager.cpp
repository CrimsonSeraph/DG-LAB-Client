/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "ThemeManager.h"

#include "AppConfig.h"
#include "DebugLog.h"

#include <QVariantMap>

namespace {

    /// @brief 线性混合两个颜色（t=0 取 a，t=1 取 b）
    QColor mix(const QColor& a, const QColor& b, qreal t) {
        return QColor::fromRgbF(
            a.redF() * (1.0 - t) + b.redF() * t,
            a.greenF() * (1.0 - t) + b.greenF() * t,
            a.blueF() * (1.0 - t) + b.blueF() * t);
    }

    /// @brief 相对亮度（0~1），用于判断颜色深浅
    qreal luminance(const QColor& c) {
        return (0.2126 * c.redF() + 0.7152 * c.greenF() + 0.0722 * c.blueF());
    }

    /// @brief 选择对比色（浅底给深字，深底给浅字）
    QColor contrast_text(const QColor& background) {
        return luminance(background) > 0.55 ? QColor(0x1F, 0x2A, 0x44) : QColor(0xFF, 0xFF, 0xFF);
    }

} // namespace

ThemeManager::ThemeManager(QObject* parent)
    : QObject(parent) {
    build_presets();
    recompute();
}

void ThemeManager::build_presets() {
    preset_list_ = {
        { QStringLiteral("light"), QStringLiteral("浅色模式"), QColor(0xE8, 0xF0, 0xFE), QColor(0xD4, 0xE6, 0xF1), false },
        { QStringLiteral("night"), QStringLiteral("深色模式"), QColor(0x2C, 0x3E, 0x50), QColor(0x1A, 0x25, 0x2F), true },
        { QStringLiteral("charcoal_pink"), QStringLiteral("炭黑甜粉"), QColor(0x1A, 0x1A, 0x1D), QColor(0xE6, 0x39, 0x7C), true },
        { QStringLiteral("deepsea_cream"), QStringLiteral("深海奶白"), QColor(0x12, 0x2E, 0x8A), QColor(0xF5, 0xEF, 0xEA), false },
        { QStringLiteral("vine_purple_tea_green"), QStringLiteral("藤紫钛绿"), QColor(0x91, 0xC5, 0x3A), QColor(0x5E, 0x55, 0xA2), true },
        { QStringLiteral("offwhite_camellia"), QStringLiteral("无白茶花"), QColor(0xF1, 0xDD, 0xDF), QColor(0xE7, 0x2D, 0x48), false },
        { QStringLiteral("dark_blue_clear_blue"), QStringLiteral("捣蓝清水"), QColor(0x11, 0x30, 0x56), QColor(0x91, 0xD5, 0xD3), true },
        { QStringLiteral("klein_yellow"), QStringLiteral("克莱因黄"), QColor(0x00, 0x2E, 0xA6), QColor(0xFF, 0xE7, 0x6F), true },
        { QStringLiteral("mars_green_rose"), QStringLiteral("马尔斯玫瑰"), QColor(0x01, 0x84, 0x7F), QColor(0xF9, 0xD2, 0xE4), true },
        { QStringLiteral("hermes_orange_navy"), QStringLiteral("爱马仕深蓝"), QColor(0xFF, 0x77, 0x0F), QColor(0x00, 0x00, 0x26), true },
        { QStringLiteral("tiffany_blue_cheese"), QStringLiteral("蒂芙尼奶酪"), QColor(0x81, 0xD8, 0xCF), QColor(0xF8, 0xF5, 0xD6), false },
        { QStringLiteral("china_red_yellow"), QStringLiteral("中国红黄"), QColor(0xFF, 0x00, 0x00), QColor(0xFA, 0xEA, 0xD3), false },
        { QStringLiteral("vandyke_brown_khaki"), QStringLiteral("凡戴克棕卡其"), QColor(0x49, 0x2D, 0x22), QColor(0xD8, 0xC7, 0xB5), false },
        { QStringLiteral("prussian_blue_fog"), QStringLiteral("普鲁士雾灰"), QColor(0x00, 0x31, 0x53), QColor(0xE5, 0xDD, 0xD7), false }
    };
}

const ThemeManager::Preset* ThemeManager::find_preset(const QString& mode) const {
    for (const auto& preset : preset_list_) {
        if (preset.mode == mode) {
            return &preset;
        }
    }
    return nullptr;
}

void ThemeManager::initialize() {
    const auto& config = AppConfig::instance();
    mode_ = QString::fromStdString(config.get_value<std::string>("app.ui.theme", "light"));
    if (mode_ == QStringLiteral("custom")) {
        const QString primary = QString::fromStdString(config.get_value<std::string>("app.ui.custom.primary", "#4C8DFF"));
        const QString secondary = QString::fromStdString(config.get_value<std::string>("app.ui.custom.secondary", "#1F2A44"));
        primary_ = QColor(primary);
        secondary_ = QColor(secondary);
        custom_ = true;
        if (!primary_.isValid()) {
            primary_ = QColor(0x4C, 0x8D, 0xFF);
        }
        if (!secondary_.isValid()) {
            secondary_ = QColor(0x1F, 0x2A, 0x44);
        }
    }
    else {
        const Preset* preset = find_preset(mode_);
        if (preset == nullptr) {
            mode_ = QStringLiteral("light");
            preset = find_preset(mode_);
        }
        primary_ = preset->primary;
        secondary_ = preset->secondary;
        custom_ = false;
    }
    recompute();
    LOG_MODULE("ThemeManager", "initialize", LOG_DEBUG, "主题已加载: " + mode_.toStdString());
}

void ThemeManager::applyPreset(const QString& mode) {
    const Preset* preset = find_preset(mode);
    if (preset == nullptr) {
        LOG_MODULE("ThemeManager", "applyPreset", LOG_WARN, "未知主题: " + mode.toStdString());
        return;
    }
    mode_ = preset->mode;
    primary_ = preset->primary;
    secondary_ = preset->secondary;
    custom_ = false;
    recompute();
    persist(mode_, primary_, secondary_);
}

void ThemeManager::applyCustom(const QColor& primary, const QColor& secondary) {
    if (!primary.isValid() || !secondary.isValid()) {
        return;
    }
    mode_ = QStringLiteral("custom");
    primary_ = primary;
    secondary_ = secondary;
    custom_ = true;
    recompute();
    persist(mode_, primary_, secondary_);
}

void ThemeManager::persist(const QString& mode, const QColor& primary, const QColor& secondary) {
    auto& config = AppConfig::instance();
    config.set_value<std::string>("app.ui.theme", mode.toStdString());
    config.set_value<std::string>("app.ui.custom.primary", primary.name(QColor::HexRgb).toStdString());
    config.set_value<std::string>("app.ui.custom.secondary", secondary.name(QColor::HexRgb).toStdString());
    config.save_all();
}

void ThemeManager::recompute() {
    // 预设：主/副色直接取表；自定义：由 C++ 派生显示名与明暗
    if (custom_) {
        display_ = QStringLiteral("自定义主题");
        dark_ = luminance(primary_) < 0.45;
    }
    else {
        const Preset* preset = find_preset(mode_);
        if (preset != nullptr) {
            display_ = preset->name;
            dark_ = preset->dark;
        }
    }

    // 从主/副色中区分「底色」与「强调色」
    const QColor light = luminance(primary_) >= luminance(secondary_) ? primary_ : secondary_;
    const QColor dark_c = (light == primary_) ? secondary_ : primary_;

    if (!dark_) {
        window_bg_ = mix(light, Qt::white, 0.30);
        surface_ = mix(light, Qt::white, 0.62);
        surface_alt_ = mix(light, Qt::white, 0.45);
        surface_subtle_ = mix(light, Qt::white, 0.80);
        border_ = mix(dark_c, Qt::white, 0.68);
        divider_ = mix(dark_c, Qt::white, 0.82);
        text_primary_ = mix(dark_c, Qt::black, 0.55);
        text_secondary_ = mix(dark_c, Qt::white, 0.20);
        text_muted_ = mix(dark_c, Qt::white, 0.48);
        accent_ = dark_c;
        // 过浅的强调色在浅色底上不可读，统一压暗到可用范围
        if (luminance(accent_) > 0.72) {
            accent_ = accent_.darker(170);
        }
    }
    else {
        window_bg_ = mix(dark_c, Qt::black, 0.15);
        surface_ = dark_c;
        surface_alt_ = mix(dark_c, Qt::white, 0.08);
        surface_subtle_ = mix(dark_c, Qt::white, 0.03);
        border_ = mix(dark_c, Qt::white, 0.22);
        divider_ = mix(dark_c, Qt::white, 0.14);
        text_primary_ = mix(light, Qt::white, 0.88);
        text_secondary_ = mix(light, Qt::white, 0.58);
        text_muted_ = mix(light, Qt::white, 0.38);
        accent_ = light;
    }

    accent_text_ = contrast_text(accent_);
    hover_ = mix(accent_, dark_ ? Qt::white : Qt::black, 0.14);
    pressed_ = mix(accent_, dark_ ? Qt::black : Qt::white, 0.14);
    selection_bg_ = mix(accent_, dark_ ? Qt::black : Qt::white, dark_ ? 0.62 : 0.74);

    emit themeChanged();
}

QVariantList ThemeManager::presets() const {
    QVariantList list;
    for (const auto& preset : preset_list_) {
        QVariantMap item;
        item.insert(QStringLiteral("mode"), preset.mode);
        item.insert(QStringLiteral("name"), preset.name);
        item.insert(QStringLiteral("primary"), preset.primary.name(QColor::HexRgb));
        item.insert(QStringLiteral("secondary"), preset.secondary.name(QColor::HexRgb));
        item.insert(QStringLiteral("dark"), preset.dark);
        list.append(item);
    }
    return list;
}
