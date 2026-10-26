/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "BuckshotPlugin.h"

#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QWebSocket>

#include <cmath>

// 默认监听端口
static const int k_default_port = 8137;

// 事件类型白名单
static const char* const k_event_types[] = {"hp", "damage", "shield"};

// 判断事件类型是否在白名单内
static bool is_known_type(const QString& type) {
    for (const char* known : k_event_types) {
        if (type == QLatin1String(known)) {
            return true;
        }
    }
    return false;
}

// 生命周期实现（public）

PluginError BuckshotPlugin::initialize() {
    if (initialized_) {
        return PluginError::AlreadyInitialized;
    }
    if (!host_) {
        PLUGIN_LOG(this, PluginLogLevel::Error, "缺少宿主上下文，无法注册模块数值");
        return PluginError::NotInitialized;
    }
    // 监听端口由宿主配置覆盖，非法值回退默认端口
    const std::string port_text = host_->get_config_value("app.module.buckshot.port", "8137");
    bool ok = false;
    const int configured_port = QString::fromStdString(port_text).trimmed().toInt(&ok);
    if (!ok || configured_port <= 0 || configured_port > 65535) {
        PLUGIN_LOG(this, PluginLogLevel::Warn,
            "端口配置无效: " << port_text << "，回退默认端口 " << k_default_port);
        port_ = k_default_port;
    } else {
        port_ = configured_port;
    }

    server_ = std::make_unique<QWebSocketServer>(QStringLiteral("DG-LAB Buckshot"),
        QWebSocketServer::NonSecureMode);
    if (!server_->listen(QHostAddress::LocalHost, static_cast<quint16>(port_))) {
        PLUGIN_LOG(this, PluginLogLevel::Warn,
            "WebSocket 服务端监听失败: " << server_->errorString().toStdString()
                << "，端口: " << port_);
        server_.reset();
        return PluginError::ResourceError;
    }
    QObject::connect(server_.get(), &QWebSocketServer::newConnection, server_.get(),
        [this]() { on_new_connection(); });

    host_->register_module_values(name(), create_default_values(), {"A", "B"});
    initialized_ = true;
    PLUGIN_LOG(this, PluginLogLevel::Info,
        "恶魔轮盘插件初始化完成，监听 127.0.0.1:" << port_);
    return PluginError::Ok;
}

void BuckshotPlugin::uninitialize() {
    close_clients();
    if (server_) {
        server_->close();
        server_.reset();
    }
    if (host_) {
        host_->unregister_module(name());
    }
    initialized_ = false;
    PLUGIN_LOG(this, PluginLogLevel::Info, "恶魔轮盘插件反初始化完成");
}

// 数值定义（private，静态）

std::vector<ModuleValue> BuckshotPlugin::create_default_values() {
    std::vector<ModuleValue> values;
    values.emplace_back("hp", "生命值", QueryPeriod::QUARTER_SECOND, "hp");
    values.emplace_back("damage", "本回合伤害", QueryPeriod::QUARTER_SECOND, "damage");
    values.emplace_back("shield", "护盾值", QueryPeriod::QUARTER_SECOND, "shield");
    return values;
}

// 连接与事件（private）

void BuckshotPlugin::on_new_connection() {
    if (!server_) {
        return;
    }
    while (server_->hasPendingConnections()) {
        QWebSocket* socket = server_->nextPendingConnection();
        if (!socket) {
            continue;
        }
        clients_.append(socket);
        QObject::connect(socket, &QWebSocket::textMessageReceived, socket,
            [this](const QString& message) { on_text_message(message); });
        QObject::connect(socket, &QWebSocket::disconnected, socket, [this, socket]() {
            clients_.removeAll(socket);
            socket->deleteLater();
        });
    }
}

void BuckshotPlugin::on_text_message(const QString& message) {
    const QJsonDocument document = QJsonDocument::fromJson(message.toUtf8());
    if (!document.isObject()) {
        PLUGIN_LOG(this, PluginLogLevel::Debug, "忽略非对象消息帧");
        return;
    }
    const QJsonObject event = document.object();
    const QString type = event.value(QStringLiteral("type")).toString();
    const QJsonValue raw_value = event.value(QStringLiteral("value"));
    if (!raw_value.isDouble()) {
        PLUGIN_LOG(this, PluginLogLevel::Debug,
            "消息缺少数值字段，已忽略，type: " << type.toStdString());
        return;
    }
    if (!is_known_type(type)) {
        PLUGIN_LOG(this, PluginLogLevel::Debug,
            "未知事件类型，已忽略: " << type.toStdString());
        return;
    }
    if (host_) {
        host_->set_value(name(), type.toStdString(), static_cast<int>(std::lround(raw_value.toDouble())));
    }
}

void BuckshotPlugin::close_clients() {
    for (QWebSocket* socket : clients_) {
        if (!socket) {
            continue;
        }
        // 先断开信号再关闭，避免关闭过程中回调已释放的插件状态
        QObject::disconnect(socket, nullptr, nullptr, nullptr);
        socket->close(QWebSocketProtocol::CloseCodeNormal, QStringLiteral("plugin unloaded"));
        socket->deleteLater();
    }
    clients_.clear();
}

// 动态库导出（extern "C" 工厂函数）
extern "C" {
PLUGIN_EXPORT int get_plugin_api_version() {
    // 宿主加载时校验：返回值必须等于 PLUGIN_API_VERSION
    return PLUGIN_API_VERSION;
}

PLUGIN_EXPORT IPlugin* create_plugin() {
    // 内存隔离：实例在插件内部 new，由 destroy_plugin 在插件内部 delete，
    // 宿主不得直接 delete 或跨模块分配内存
    return new BuckshotPlugin();
}

PLUGIN_EXPORT void destroy_plugin(IPlugin* plugin) {
    delete plugin;
}
}
