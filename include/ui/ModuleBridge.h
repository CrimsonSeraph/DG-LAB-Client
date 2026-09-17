/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

// ============================================
// ModuleBridge - 模块页桥接（QML 上下文属性 moduleBridge）
// ============================================
// 职责：汇总插件卡片与内置模块卡片、统一查询周期选项、被选中模块的数值列表，
//      并转发统一周期设置与插件启用/禁用操作。
class ModuleBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList modules READ modules NOTIFY dataChanged)
    Q_PROPERTY(QVariantList periodOptions READ period_options CONSTANT)
    Q_PROPERTY(int basePeriodMs READ base_period_ms NOTIFY dataChanged)
    Q_PROPERTY(QString selectedModule READ selected_module NOTIFY selectedChanged)
    Q_PROPERTY(QVariantList selectedValues READ selected_values NOTIFY selectedChanged)

public:
    explicit ModuleBridge(QObject* parent = nullptr);

    /// @brief 接入模块管理器信号
    void initialize();

    QVariantList modules() const;
    QVariantList period_options() const;
    int base_period_ms() const;
    QString selected_module() const { return selected_; }
    QVariantList selected_values() const;

    /// @brief 统一设置所有数值的查询周期（index 为 periodOptions 下标）
    Q_INVOKABLE void applyAllPeriod(int index);
    /// @brief 启用/禁用插件（按插件文件名）
    Q_INVOKABLE void togglePlugin(const QString& file_name);
    /// @brief 选中模块（用于数值展示弹窗）
    Q_INVOKABLE void selectModule(const QString& module_name);

signals:
    void dataChanged();
    void selectedChanged();

private:
    QString selected_;
};
