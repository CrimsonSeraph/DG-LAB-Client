/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "IpSelector.h"

#include "AppConfig.h"
#include "DebugLog.h"

#include <QHostAddress>

#include <string>
#include <vector>

namespace {

    constexpr char CONFIG_BLACKLIST[] = "app.network.ip_blacklist"; ///< 黑名单配置键
    constexpr char CONFIG_WHITELIST[] = "app.network.ip_whitelist"; ///< 白名单配置键
    constexpr char CONFIG_SELECTED_IP[] = "app.network.selected_ip"; ///< 手动选中地址配置键
    constexpr char FALLBACK_IP[] = "127.0.0.1";                     ///< 无有效网卡时的回退地址

    // 默认黑名单：虚拟网卡、VPN 与蓝牙网络（覆盖 Hyper-V / VMware / WSL / Radmin / TAP）
    std::vector<std::string> default_blacklist() {
        return {"vmware", "virtual", "vethernet", "hyper-v", "docker", "vbox", "vpn", "tap", "bluetooth"};
    }

    // 默认白名单：实体网卡名（有线 / 无线）
    std::vector<std::string> default_whitelist() {
        return {"以太网", "ethernet", "wlan", "wi-fi", "en0"};
    }

    // 配置里的字符串数组转 QStringList
    QStringList to_qstring_list(const std::vector<std::string>& values) {
        QStringList result;
        for (const auto& value : values) {
            result.append(QString::fromStdString(value));
        }
        return result;
    }

    // QStringList 转配置里的字符串数组
    std::vector<std::string> to_string_vector(const QStringList& values) {
        std::vector<std::string> result;
        result.reserve(static_cast<std::size_t>(values.size()));
        for (const auto& value : values) {
            result.push_back(value.toStdString());
        }
        return result;
    }

} // namespace

IpSelector::IpSelector(QObject* parent)
    : QObject(parent) {
    load_config();
    refresh_candidates();
    LOG_MODULE("IpSelector", "IpSelector", LOG_INFO,
        "地址选择器就绪，实际地址: " << active_ip().toStdString());
}

QString IpSelector::active_ip() const {
    if (!selected_ip_.isEmpty()) {
        return selected_ip_;
    }
    return auto_select_ip();
}

QString IpSelector::auto_select_ip() const {
    const auto interfaces = QNetworkInterface::allInterfaces();
    QString fallback;
    for (const auto& interface : interfaces) {
        if (!is_interface_usable(interface) || is_interface_excluded(interface)) {
            continue;
        }
        const QString address = ipv4_of_interface(interface);
        if (address.isEmpty()) {
            continue;
        }
        if (fallback.isEmpty()) {
            fallback = address;
        }
        const QString name = interface.name();
        const QString description = interface.humanReadableName();
        if (contains_any_keyword(name, whitelist_) || contains_any_keyword(description, whitelist_)) {
            return address;
        }
    }
    return fallback.isEmpty() ? QString::fromLatin1(FALLBACK_IP) : fallback;
}

bool IpSelector::contains_any_keyword(const QString& str, const QStringList& keywords) {
    for (const auto& keyword : keywords) {
        if (!keyword.isEmpty() && str.contains(keyword, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

void IpSelector::refresh_candidates() {
    candidates_.clear();
    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const auto& interface : interfaces) {
        if (!is_interface_usable(interface) || is_interface_excluded(interface)) {
            continue;
        }
        const QString address = ipv4_of_interface(interface);
        if (address.isEmpty()) {
            continue;
        }
        QVariantMap entry;
        entry.insert(QStringLiteral("name"), interface.name());
        entry.insert(QStringLiteral("description"), describe_interface(interface));
        entry.insert(QStringLiteral("address"), address);
        candidates_.append(entry);
    }
    LOG_MODULE("IpSelector", "refresh_candidates", LOG_DEBUG,
        "候选地址 " << candidates_.size() << " 个，实际地址: " << active_ip().toStdString());
    emit candidatesChanged();
    emit activeIpChanged();
}

bool IpSelector::add_blacklist_keyword(const QString& keyword) {
    const QString trimmed = keyword.trimmed();
    if (trimmed.isEmpty() || blacklist_.contains(trimmed)) {
        return false;
    }
    blacklist_.append(trimmed);
    save_lists();
    emit listsChanged();
    return true;
}

bool IpSelector::remove_blacklist_keyword(const QString& keyword) {
    if (!blacklist_.removeOne(keyword)) {
        return false;
    }
    save_lists();
    emit listsChanged();
    return true;
}

bool IpSelector::add_whitelist_keyword(const QString& keyword) {
    const QString trimmed = keyword.trimmed();
    if (trimmed.isEmpty() || whitelist_.contains(trimmed)) {
        return false;
    }
    whitelist_.append(trimmed);
    save_lists();
    emit listsChanged();
    return true;
}

bool IpSelector::remove_whitelist_keyword(const QString& keyword) {
    if (!whitelist_.removeOne(keyword)) {
        return false;
    }
    save_lists();
    emit listsChanged();
    return true;
}

void IpSelector::set_selected_ip(const QString& ip) {
    if (selected_ip_ == ip) {
        return;
    }
    selected_ip_ = ip;
    save_selected_ip();
    LOG_MODULE("IpSelector", "set_selected_ip", LOG_INFO,
        "手动选中地址: " << (ip.isEmpty() ? std::string("<自动>") : ip.toStdString()));
    emit selectedIpChanged();
    emit activeIpChanged();
}

bool IpSelector::is_interface_usable(const QNetworkInterface& interface) {
    const auto flags = interface.flags();
    return flags.testFlag(QNetworkInterface::IsUp) && flags.testFlag(QNetworkInterface::IsRunning)
        && !flags.testFlag(QNetworkInterface::IsLoopBack);
}

QString IpSelector::describe_interface(const QNetworkInterface& interface) {
    const QString description = interface.humanReadableName();
    return description.isEmpty() ? interface.name() : description;
}

bool IpSelector::is_interface_excluded(const QNetworkInterface& interface) const {
    return contains_any_keyword(interface.name(), blacklist_)
        || contains_any_keyword(interface.humanReadableName(), blacklist_);
}

QString IpSelector::ipv4_of_interface(const QNetworkInterface& interface) const {
    const auto entries = interface.addressEntries();
    for (const auto& entry : entries) {
        const QHostAddress address = entry.ip();
        if (address.protocol() == QAbstractSocket::IPv4Protocol && !address.isLoopback()) {
            return address.toString();
        }
    }
    return QString();
}

void IpSelector::load_config() {
    const auto& config = AppConfig::instance();
    blacklist_ = to_qstring_list(config.get_value<std::vector<std::string>>(CONFIG_BLACKLIST, default_blacklist()));
    whitelist_ = to_qstring_list(config.get_value<std::vector<std::string>>(CONFIG_WHITELIST, default_whitelist()));
    selected_ip_ = QString::fromStdString(config.get_value<std::string>(CONFIG_SELECTED_IP, std::string()));
}

void IpSelector::save_lists() {
    auto& config = AppConfig::instance();
    config.set_value<std::vector<std::string>>(CONFIG_BLACKLIST, to_string_vector(blacklist_));
    config.set_value<std::vector<std::string>>(CONFIG_WHITELIST, to_string_vector(whitelist_));
    config.save_all();
}

void IpSelector::save_selected_ip() {
    auto& config = AppConfig::instance();
    config.set_value<std::string>(CONFIG_SELECTED_IP, selected_ip_.toStdString());
    config.save_all();
}
