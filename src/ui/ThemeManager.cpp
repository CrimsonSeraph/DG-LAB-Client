/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "ThemeManager.h"

#include "AppConfig.h"
#include "DebugLog.h"

#include <QVariantMap>

#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace {

    /// @brief 线性混合两个颜色（t=0 取 a，t=1 取 b）
    QColor mix(const QColor& a, const QColor& b, qreal t) {
        return QColor::fromRgbF(
            a.redF() * (1.0 - t) + b.redF() * t,
            a.greenF() * (1.0 - t) + b.greenF() * t,
            a.blueF() * (1.0 - t) + b.blueF() * t);
    }

    /// @brief 粗略亮度（0~1），用于挑选底色与判断自定义主题明暗
    qreal luminance(const QColor& c) {
        return (0.2126 * c.redF() + 0.7152 * c.greenF() + 0.0722 * c.blueF());
    }

    /// @brief sRGB 单通道线性化（WCAG 2.x 定义）
    qreal linear_channel(qreal channel) {
        return channel <= 0.03928 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
    }

    /// @brief WCAG 相对亮度（0~1）
    qreal relative_luminance(const QColor& c) {
        return 0.2126 * linear_channel(c.redF()) + 0.7152 * linear_channel(c.greenF()) + 0.0722 * linear_channel(c.blueF());
    }

    /// @brief WCAG 对比度（1~21），与前景/背景顺序无关
    qreal contrast_ratio(const QColor& a, const QColor& b) {
        const qreal la = relative_luminance(a);
        const qreal lb = relative_luminance(b);
        return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
    }

    /// @brief 将前景色朝远离背景色的方向调整，直到对比度达到 target
    QColor ensure_contrast(QColor foreground, const QColor& background, qreal target) {
        if (contrast_ratio(foreground, background) >= target) {
            return foreground;
        }
        const QColor limit = relative_luminance(background) < 0.5 ? QColor(Qt::white) : QColor(Qt::black);
        for (int step = 1; step <= 100; ++step) {
            const QColor candidate = mix(foreground, limit, step / 100.0);
            if (contrast_ratio(candidate, background) >= target) {
                return candidate;
            }
        }
        return limit;
    }

    /// @brief 依次对多个背景保证对比度（同一主题内调整方向一致，取最严格结果）
    QColor ensure_contrast_on(QColor foreground, std::initializer_list<QColor> backgrounds, qreal target) {
        for (const QColor& background : backgrounds) {
            foreground = ensure_contrast(foreground, background, target);
        }
        return foreground;
    }

    /// @brief 将背景色朝远离文字色的方向调整，直到纯黑/纯白文字在其上达到 target
    QColor ensure_background(QColor background, const QColor& text, qreal target) {
        if (contrast_ratio(text, background) >= target) {
            return background;
        }
        const QColor limit = relative_luminance(text) < 0.5 ? QColor(Qt::white) : QColor(Qt::black);
        for (int step = 1; step <= 100; ++step) {
            const QColor candidate = mix(background, limit, step / 100.0);
            if (contrast_ratio(text, candidate) >= target) {
                return candidate;
            }
        }
        return limit;
    }

    /// @brief 选择对比色（在给定背景上取黑/白中对比度更高者）
    QColor contrast_text(const QColor& background) {
        const QColor dark(0x1F, 0x2A, 0x44);
        const QColor light(Qt::white);
        return contrast_ratio(light, background) >= contrast_ratio(dark, background) ? light : dark;
    }

    // 语义色基准值：每个主题都从基准色重新校正，避免主题间反复调整产生漂移
    const QColor kSuccess(0x2E, 0x7D, 0x5B);
    const QColor kWarning(0xB7, 0x79, 0x1F);
    const QColor kDanger(0xC0, 0x39, 0x2B);

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
        { QStringLiteral("prussian_blue_fog"), QStringLiteral("普鲁士雾灰"), QColor(0x00, 0x31, 0x53), QColor(0xE5, 0xDD, 0xD7), false },
        { QStringLiteral("midnight_blue"), QStringLiteral("午夜蓝"), QColor(0x14, 0x22, 0x40), QColor(0x5B, 0x8D, 0xEF), true },
        { QStringLiteral("forest_green"), QStringLiteral("森野绿"), QColor(0x16, 0x33, 0x2A), QColor(0x7B, 0xC9, 0x8A), true }
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
        // 浅色主题：先保证底色浅到能承载深色文字，再逐级校正文字与边框对比度
        surface_ = ensure_background(mix(light, Qt::white, 0.62), QColor(Qt::black), 7.05);
        surface_alt_ = ensure_background(mix(light, Qt::white, 0.45), QColor(Qt::black), 4.60);
        window_bg_ = ensure_background(mix(light, Qt::white, 0.30), QColor(Qt::black), 4.60);
        surface_subtle_ = mix(light, Qt::white, 0.80);
        text_primary_ = ensure_contrast(mix(dark_c, Qt::black, 0.55), surface_, 7.0);
        text_secondary_ = ensure_contrast_on(mix(dark_c, Qt::white, 0.05), { surface_, surface_alt_, window_bg_ }, 4.5);
        text_muted_ = ensure_contrast_on(mix(dark_c, Qt::white, 0.30), { surface_, surface_alt_, window_bg_ }, 4.5);
        accent_ = dark_c;
        // 过浅的强调色在浅色底上不可读，统一压暗到可用范围
        if (luminance(accent_) > 0.72) {
            accent_ = accent_.darker(170);
        }
        border_ = mix(dark_c, Qt::white, 0.68);
        divider_ = mix(dark_c, Qt::white, 0.82);
    }
    else {
        // 深色主题：底色压暗到能承载接近纯白的文字，再派生次级表面
        surface_ = ensure_background(dark_c, QColor(Qt::white), 7.05);
        surface_alt_ = ensure_background(mix(surface_, Qt::white, 0.08), QColor(Qt::white), 4.60);
        window_bg_ = ensure_background(mix(surface_, Qt::black, 0.15), QColor(Qt::white), 4.60);
        surface_subtle_ = mix(surface_, Qt::white, 0.03);
        text_primary_ = ensure_contrast(mix(light, Qt::white, 0.88), surface_, 7.0);
        text_secondary_ = ensure_contrast_on(mix(light, Qt::white, 0.58), { surface_, surface_alt_, window_bg_ }, 4.5);
        text_muted_ = ensure_contrast_on(mix(light, Qt::white, 0.38), { surface_, surface_alt_, window_bg_ }, 4.5);
        accent_ = light;
        border_ = mix(surface_, Qt::white, 0.22);
        divider_ = mix(surface_, Qt::white, 0.14);
    }

    // 文字需同时在 surface / surfaceAlt / windowBg 上可读：surface 上达 WCAG AAA（7:1），其余至少达 AA（4.5:1）
    text_primary_ = ensure_contrast_on(text_primary_, { surface_alt_, window_bg_ }, 4.5);

    // 强调色作为文字/图标需在表面上可读，其上的对比文字也需可读
    accent_ = ensure_contrast_on(accent_, { surface_, surface_alt_ }, 4.5);
    accent_text_ = ensure_contrast(contrast_text(accent_), accent_, 4.5);

    // 边框与分隔线在 surface 与 surfaceAlt 上都需可见
    border_ = ensure_contrast_on(border_, { surface_, surface_alt_ }, 1.35);
    divider_ = ensure_contrast_on(divider_, { surface_, surface_alt_ }, 1.15);

    // 语义色随主题校正，保证作为文字/描边时可见
    success_ = ensure_contrast(kSuccess, surface_, 4.5);
    warning_ = ensure_contrast(kWarning, surface_, 4.5);
    danger_ = ensure_contrast(kDanger, surface_, 4.5);

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
