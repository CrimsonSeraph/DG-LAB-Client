/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QNetworkInterface>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

// IpSelector - 中转服务的局域网 IPv4 地址选择器（QML 上下文属性 device.ipSelector）
// 职责：枚举可用网卡的 IPv4、按黑名单过滤与白名单优先排序、维护手动选中地址，
//      并把名单与选中地址持久化到 config/user.json 的 app.network.*。
// 依赖：QNetworkInterface 与 AppConfig，不含任何 Widgets 类型。
class IpSelector : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList candidates READ candidates NOTIFY candidatesChanged)
    Q_PROPERTY(QStringList blacklist READ blacklist NOTIFY listsChanged)
    Q_PROPERTY(QStringList whitelist READ whitelist NOTIFY listsChanged)
    Q_PROPERTY(QString selectedIp READ selected_ip NOTIFY selectedIpChanged)
    Q_PROPERTY(QString activeIp READ active_ip NOTIFY activeIpChanged)

public:
    explicit IpSelector(QObject* parent = nullptr);
    ~IpSelector() override = default;

    /// @brief 过滤后的候选网卡列表（name / description / address 三个字段）
    QVariantList candidates() const { return candidates_; }
    /// @brief 当前黑名单关键词（网卡名或描述包含则忽略）
    QStringList blacklist() const { return blacklist_; }
    /// @brief 当前白名单关键词（匹配则优先选择）
    QStringList whitelist() const { return whitelist_; }
    /// @brief 手动选中的地址（空字符串表示按名单自动选择）
    QString selected_ip() const { return selected_ip_; }

    /// @brief 实际使用的地址：手动选中优先，其次按名单自动选择，都没有则回退 127.0.0.1
    QString active_ip() const;
    /// @brief 按名单自动选择地址（白名单优先，其次首个有效地址，无有效值回退 127.0.0.1）
    QString auto_select_ip() const;
    /// @brief 检查字符串是否包含任意关键词（大小写不敏感）
    /// @param str 待检查字符串
    /// @param keywords 关键词列表
    /// @return 命中任一关键词返回 true
    static bool contains_any_keyword(const QString& str, const QStringList& keywords);

    /// @brief 重新枚举候选地址并广播（名单变化或对话框打开时调用）
    Q_INVOKABLE void refresh_candidates();
    /// @brief 追加黑名单关键词并持久化
    /// @param keyword 关键词（空白或重复时忽略）
    /// @return 关键词被写入返回 true
    Q_INVOKABLE bool add_blacklist_keyword(const QString& keyword);
    /// @brief 删除黑名单关键词并持久化
    /// @param keyword 关键词
    /// @return 关键词被移除返回 true
    Q_INVOKABLE bool remove_blacklist_keyword(const QString& keyword);
    /// @brief 追加白名单关键词并持久化
    /// @param keyword 关键词（空白或重复时忽略）
    /// @return 关键词被写入返回 true
    Q_INVOKABLE bool add_whitelist_keyword(const QString& keyword);
    /// @brief 删除白名单关键词并持久化
    /// @param keyword 关键词
    /// @return 关键词被移除返回 true
    Q_INVOKABLE bool remove_whitelist_keyword(const QString& keyword);
    /// @brief 设置手动选中的地址并持久化（空字符串恢复自动选择）
    /// @param ip 选中的 IPv4 地址
    Q_INVOKABLE void set_selected_ip(const QString& ip);

signals:
    void candidatesChanged();
    void listsChanged();
    void selectedIpChanged();
    void activeIpChanged();

private:
    QVariantList candidates_; ///< 过滤后的候选网卡（name / description / address）
    QStringList blacklist_;   ///< 黑名单关键词（网卡名或描述包含则忽略）
    QStringList whitelist_;   ///< 白名单关键词（匹配则优先选择）
    QString selected_ip_;     ///< 手动选中的 IPv4 地址（空表示自动选择）

    /// @brief 判断网卡是否可作为候选（已启用、正在运行、非回环）
    static bool is_interface_usable(const QNetworkInterface& interface);
    /// @brief 取网卡描述（humanReadableName 为空时回退网卡名）
    static QString describe_interface(const QNetworkInterface& interface);
    /// @brief 判断网卡是否被黑名单过滤
    bool is_interface_excluded(const QNetworkInterface& interface) const;
    /// @brief 取网卡的非回环 IPv4 地址（无则返回空字符串）
    QString ipv4_of_interface(const QNetworkInterface& interface) const;
    /// @brief 从 user.json 读取名单与手动选中地址
    void load_config();
    /// @brief 把名单写入 user.json 并落盘
    void save_lists();
    /// @brief 把手动选中地址写入 user.json 并落盘
    void save_selected_ip();
};
