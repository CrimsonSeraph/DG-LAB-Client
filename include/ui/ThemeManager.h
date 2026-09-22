/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QColor>
#include <QList>
#include <QObject>
#include <QString>
#include <QVariantList>

// ============================================
// ThemeManager - 主题令牌提供者（QML 单例 Theme）
// ============================================
// 职责：维护当前主题（16 套预设 + 自定义主/副色），向 QML 暴露语义化颜色令牌，
// 并将用户选择持久化到 user.json 的 app.ui 下。
//
// 说明：颜色令牌全部集中在 C++，QML 只引用令牌名，不在页面里写颜色字面量；
//      预设主/副色沿用原 QSS 主题的配色（浅色/深色 + 14 套扩展主题），
//      文字/边框/语义色按 WCAG 对比度下限逐主题校正，浅色主题下也不会出现不可读文字。
class ThemeManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString themeName READ theme_name NOTIFY themeChanged)
    Q_PROPERTY(QString displayName READ display_name NOTIFY themeChanged)
    Q_PROPERTY(bool custom READ is_custom NOTIFY themeChanged)
    Q_PROPERTY(bool dark READ is_dark NOTIFY themeChanged)
    Q_PROPERTY(QColor primary READ primary NOTIFY themeChanged)
    Q_PROPERTY(QColor secondary READ secondary NOTIFY themeChanged)
    Q_PROPERTY(QColor windowBg READ window_bg NOTIFY themeChanged)
    Q_PROPERTY(QColor surface READ surface NOTIFY themeChanged)
    Q_PROPERTY(QColor surfaceAlt READ surface_alt NOTIFY themeChanged)
    Q_PROPERTY(QColor surfaceSubtle READ surface_subtle NOTIFY themeChanged)
    Q_PROPERTY(QColor border READ border NOTIFY themeChanged)
    Q_PROPERTY(QColor divider READ divider NOTIFY themeChanged)
    Q_PROPERTY(QColor textPrimary READ text_primary NOTIFY themeChanged)
    Q_PROPERTY(QColor textSecondary READ text_secondary NOTIFY themeChanged)
    Q_PROPERTY(QColor textMuted READ text_muted NOTIFY themeChanged)
    Q_PROPERTY(QColor accent READ accent NOTIFY themeChanged)
    Q_PROPERTY(QColor accentText READ accent_text NOTIFY themeChanged)
    Q_PROPERTY(QColor hover READ hover NOTIFY themeChanged)
    Q_PROPERTY(QColor pressed READ pressed NOTIFY themeChanged)
    Q_PROPERTY(QColor selectionBg READ selection_bg NOTIFY themeChanged)
    Q_PROPERTY(QColor success READ success NOTIFY themeChanged)
    Q_PROPERTY(QColor warning READ warning NOTIFY themeChanged)
    Q_PROPERTY(QColor danger READ danger NOTIFY themeChanged)
    Q_PROPERTY(QVariantList presets READ presets CONSTANT)

public:
    explicit ThemeManager(QObject* parent = nullptr);

    /// @brief 从配置读取当前主题并计算令牌（需在 AppConfig 初始化后调用）
    void initialize();

    QString theme_name() const { return mode_; }
    QString display_name() const { return display_; }
    bool is_custom() const { return custom_; }
    bool is_dark() const { return dark_; }
    QColor primary() const { return primary_; }
    QColor secondary() const { return secondary_; }
    QColor window_bg() const { return window_bg_; }
    QColor surface() const { return surface_; }
    QColor surface_alt() const { return surface_alt_; }
    QColor surface_subtle() const { return surface_subtle_; }
    QColor border() const { return border_; }
    QColor divider() const { return divider_; }
    QColor text_primary() const { return text_primary_; }
    QColor text_secondary() const { return text_secondary_; }
    QColor text_muted() const { return text_muted_; }
    QColor accent() const { return accent_; }
    QColor accent_text() const { return accent_text_; }
    QColor hover() const { return hover_; }
    QColor pressed() const { return pressed_; }
    QColor selection_bg() const { return selection_bg_; }
    QColor success() const { return success_; }
    QColor warning() const { return warning_; }
    QColor danger() const { return danger_; }
    QVariantList presets() const;

    /// @brief 应用预设主题（mode 为预设英文名，如 "light"/"night"）
    Q_INVOKABLE void applyPreset(const QString& mode);
    /// @brief 应用自定义主/副色（写入 app.ui.custom.*，主题名记为 custom）
    Q_INVOKABLE void applyCustom(const QColor& primary, const QColor& secondary);

signals:
    void themeChanged();

private:
    struct Preset {
        QString mode;
        QString name;
        QColor primary;
        QColor secondary;
        bool dark;
    };

    void build_presets();
    const Preset* find_preset(const QString& mode) const;
    void recompute();
    void persist(const QString& mode, const QColor& primary, const QColor& secondary);

    QList<Preset> preset_list_;
    QString mode_ = "light";
    QString display_ = "浅色模式";
    bool custom_ = false;
    bool dark_ = false;
    QColor primary_;
    QColor secondary_;
    QColor window_bg_;
    QColor surface_;
    QColor surface_alt_;
    QColor surface_subtle_;
    QColor border_;
    QColor divider_;
    QColor text_primary_;
    QColor text_secondary_;
    QColor text_muted_;
    QColor accent_;
    QColor accent_text_;
    QColor hover_;
    QColor pressed_;
    QColor selection_bg_;
    QColor success_ = QColor(0x2E, 0x7D, 0x5B);
    QColor warning_ = QColor(0xB7, 0x79, 0x1F);
    QColor danger_ = QColor(0xC0, 0x39, 0x2B);
};
