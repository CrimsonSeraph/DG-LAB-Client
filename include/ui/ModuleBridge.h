// Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QObject>
#include <QSet>
#include <QString>
#include <QVariantList>

class QNetworkAccessManager;

// ModuleBridge - 模块页桥接（QML 上下文属性 moduleBridge）
// 职责：汇总插件卡片与内置模块卡片、统一查询周期选项、被选中模块的数值列表，
//      并转发统一周期设置与插件启用/禁用操作
class ModuleBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList modules READ modules NOTIFY dataChanged)
    Q_PROPERTY(QVariantList periodOptions READ period_options CONSTANT)
    Q_PROPERTY(int basePeriodMs READ base_period_ms NOTIFY dataChanged)
    Q_PROPERTY(int uniformPeriodIndex READ uniform_period_index NOTIFY dataChanged)
    Q_PROPERTY(QString selectedModule READ selected_module NOTIFY selectedChanged)
    Q_PROPERTY(QString selectedDisplayName READ selected_display_name NOTIFY selectedChanged)
    Q_PROPERTY(QVariantList selectedValues READ selected_values NOTIFY selectedChanged)

public:
    explicit ModuleBridge(QObject* parent = nullptr);

    /// @brief 接入模块管理器信号
    void initialize();

    QVariantList modules() const;
    QVariantList period_options() const;
    int base_period_ms() const;
    /// @brief 全部已注册数值周期一致时该周期在 periodOptions 的下标，不一致返回 -1
    int uniform_period_index() const;
    QString selected_module() const { return selected_; }
    /// @brief 选中模块的展示名（命中插件取侧车 display_name，缺失回退模块标识）
    QString selected_display_name() const;
    QVariantList selected_values() const;

    /// @brief 统一设置所有数值的查询周期（index 为 periodOptions 下标）
    Q_INVOKABLE void applyAllPeriod(int index);
    /// @brief 启用/禁用插件（按插件文件名）
    Q_INVOKABLE void togglePlugin(const QString& file_name);
    /// @brief 选中模块（用于数值展示弹窗）
    Q_INVOKABLE void selectModule(const QString& module_name);
    /// @brief 头图源：纯文件名交给 modulebanner 提供器，绝对 http(s) URL 先查本地缓存
    /// @param file_name 插件文件名（动态库文件名，提供器据此定位侧车目录）
    /// @param banner 侧车 banner 字段原值
    /// @return 可直接赋给 Image.source 的串；无图或不认识的写法返回空串
    /// @note 远程头图首次显示仍用远程地址，同时异步落盘到插件目录，落盘后改走 image://
    Q_INVOKABLE QString bannerSource(const QString& file_name, const QString& banner);

signals:
    void dataChanged();
    void selectedChanged();
    /// @brief 插件启用/禁用结果（ok 为 false 时 error 为真实失败原因）
    void pluginToggled(const QString& file, bool ok, const QString& error);

private:
    /// @brief 远程头图的本地缓存路径（<插件扫描目录>/<dll 主干名>/<文件名>）
    /// @param file_name 插件文件名（动态库文件名）
    /// @param banner 远程头图地址
    /// @return 缓存文件绝对路径；地址取不出可用文件名或插件目录未知时为空串
    static QString banner_cache_path(const QString& file_name, const QString& banner);

    /// @brief 命中本地缓存时给出 image:// 串，未命中时发起一次异步下载
    /// @param file_name 插件文件名
    /// @param banner 远程头图地址
    /// @return 可直接赋给 Image.source 的本地源；缓存尚未就绪返回空串
    QString cached_banner_source(const QString& file_name, const QString& banner);

    /// @brief 异步下载远程头图到缓存路径（同一路径只发起一次）
    /// @param banner 远程头图地址
    /// @param path 缓存目标路径
    void start_banner_download(const QString& banner, const QString& path);

    /// @brief 远程头图下载结束：写入缓存或记录失败，成功时驱动界面重新取源
    void on_banner_finished();

    QString selected_;
    QNetworkAccessManager* banner_network_ = nullptr; ///< 头图缓存下载的网络访问管理器（懒创建）
    QSet<QString> banner_cached_;  ///< 已确认存在的缓存目标路径（命中后不再重复 stat）
    QSet<QString> banner_pending_; ///< 正在下载的缓存目标路径
    QSet<QString> banner_failed_;  ///< 下载或写盘失败过的缓存目标路径（本次进程内不再重试）
};
