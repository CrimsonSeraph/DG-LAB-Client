// Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

class QJsonObject;
class QNetworkAccessManager;
class QNetworkReply;

// MarketBridge - 插件市场桥接（QML 上下文属性 market）
// 职责：拉取插件清单、与本地已装版本比对给出 install / update / installed 三态，
//      把安装入口转发给 ModuleManager（下载、校验、解压与替换都在那边完成），
//      并把下载进度与安装结果回写到清单项上
class MarketBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString sourceUrl READ source_url CONSTANT)
    Q_PROPERTY(bool loading READ loading NOTIFY stateChanged)
    Q_PROPERTY(QString lastError READ last_error NOTIFY stateChanged)
    Q_PROPERTY(QVariantList plugins READ plugins NOTIFY pluginsChanged)
    Q_PROPERTY(QString updatedAt READ updated_at NOTIFY pluginsChanged)

public:
    explicit MarketBridge(QObject* parent = nullptr);

    /// @brief 接入模块管理器的安装进度与结果信号
    void initialize();

    QString source_url() const { return source_url_; }
    bool loading() const { return loading_; }
    QString last_error() const { return last_error_; }
    QVariantList plugins() const;
    QString updated_at() const { return updated_at_; }

    /// @brief 重新拉取插件清单
    Q_INVOKABLE void refresh();

    /// @brief 安装或更新指定插件；依赖缺失与平台无包都在下载前拒绝
    /// @param id 清单里的插件标识
    Q_INVOKABLE void install(const QString& id);

signals:
    void stateChanged();
    void pluginsChanged();

    /// @brief 清单刷新结束
    /// @param ok 是否成功
    /// @param error 失败原因（成功为空）
    void refreshFinished(bool ok, const QString& error);

    /// @brief 安装结束（成功与失败都发）
    /// @param id 清单里的插件标识
    /// @param ok 是否成功
    /// @param error 失败原因（成功为空）
    void installFinished(const QString& id, bool ok, const QString& error);

private slots:
    /// @brief 清单请求结束：解析条目并重算三态
    void on_manifest_reply();
    /// @brief 插件包下载进度：回写到当前安装项
    /// @param target 安装目标目录名（下载阶段为空串）
    /// @param received 已接收字节数
    /// @param total 总字节数
    void on_install_progress(const QString& target, qint64 received, qint64 total);
    /// @brief 插件包安装结束：回写结果并重算该项的三态
    /// @param target 安装目标目录名
    /// @param ok 是否成功
    /// @param error 失败原因
    void on_install_finished(const QString& target, bool ok, const QString& error);

private:
    /// @brief 清单项（变体已按当前平台筛好）
    struct MarketItem {
        QString id;                  ///< 插件标识（与侧车 plugin.json 的 id 同一套）
        QString name;                ///< 显示名称
        QString version;             ///< 清单里的版本
        QString description;         ///< 一句话简介
        QString banner;              ///< 头图源（只有绝对 http(s) 地址才保留）
        QStringList tags;            ///< 标签
        QStringList dependencies;    ///< 依赖的插件标识
        QString download_url;        ///< 当前平台的下载地址（无可用变体为空）
        QString sha256;              ///< 当前平台安装包的 SHA256
        QString local_version;       ///< 本地已装版本（未装为空）
        QString status;              ///< install / update / installed / installing
        QString status_text;         ///< 状态文案
        QString action_text;         ///< 操作按钮文案
        QString error;               ///< 该项的失败原因
        bool has_variant = false;    ///< 当前平台是否存在可用安装包
        bool action_enabled = false; ///< 操作按钮是否可用
        double progress = 0.0;       ///< 下载进度 0-1
    };

    /// @brief 本地已装插件版本表
    /// @return 插件标识到版本的映射（侧车缺失时版本为空串，目录名与侧车 id 各登记一次）
    QHash<QString, QString> installed_versions() const;

    /// @brief 当前平台键（windows-x64 / linux-arm64 等）
    /// @return 平台键，系统或架构无法判定时为空串
    QString platform_key() const;

    /// @brief 按清单 JSON 重建条目；正在安装的项保留进度
    /// @param root 清单根对象
    void apply_manifest(const QJsonObject& root);

    /// @brief 重算单项的三态、状态文案与按钮可用性
    /// @param item 清单项
    /// @param installing 该项是否正在安装
    void refresh_item_state(MarketItem& item, bool installing) const;

    /// @brief 按标识查找清单项
    /// @param id 插件标识
    /// @return 清单项指针，不存在返回 nullptr
    MarketItem* find_item(const QString& id);

    /// @brief 远程版本是否比本地版本新
    /// @param remote 清单版本
    /// @param local 本地版本
    /// @return 需要更新时返回 true
    static bool is_newer(const QString& remote, const QString& local);

    QNetworkAccessManager* network_ = nullptr; ///< 清单请求的网络访问管理器（懒创建）
    QNetworkReply* manifest_reply_ = nullptr;  ///< 当前清单请求回复
    QList<MarketItem> items_;                  ///< 清单项（仅主线程访问）
    QString source_url_;                       ///< 清单源地址（config app.market.registry_url）
    QString updated_at_;                       ///< 清单生成时间（registry 的 generatedAt）
    QString last_error_;                       ///< 最近一次刷新失败的原因
    QString installing_id_;                    ///< 正在安装的插件标识（空串表示空闲）
    bool loading_ = false;                     ///< 是否正在拉取清单
};
