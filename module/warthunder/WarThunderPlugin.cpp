/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "WarThunderPlugin.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include <cmath>

// 调试日志限流间隔（毫秒）
static const qint64 k_log_interval_ms = 1000;

// 在遥测数据中按键名大小写不敏感取值
static bool find_field(const QJsonObject& state, const QString& key, double* out) {
    for (auto it = state.constBegin(); it != state.constEnd(); ++it) {
        if (it.key().compare(key, Qt::CaseInsensitive) != 0) {
            continue;
        }
        if (it.value().isDouble()) {
            *out = it.value().toDouble();
            return true;
        }
        return false;
    }
    return false;
}

// 提取空速（优先 IAS，缺失回退 TAS）
static double read_airspeed(const QJsonObject& state) {
    double value = 0.0;
    if (find_field(state, QStringLiteral("IAS, km/h"), &value)) {
        return value;
    }
    if (find_field(state, QStringLiteral("TAS, km/h"), &value)) {
        return value;
    }
    return 0.0;
}

// 提取高度（缺失按 0）
static double read_altitude(const QJsonObject& state) {
    double value = 0.0;
    if (find_field(state, QStringLiteral("altitude, m"), &value)) {
        return value;
    }
    return 0.0;
}

// 提取数据有效性（布尔或数值，缺失按 0）
static int read_valid(const QJsonObject& state) {
    const QJsonValue value = state.value(QStringLiteral("valid"));
    if (value.isBool()) {
        return value.toBool() ? 1 : 0;
    }
    if (value.isDouble()) {
        return value.toDouble() != 0.0 ? 1 : 0;
    }
    return 0;
}

// 生命周期实现（public）

PluginError WarThunderPlugin::initialize() {
    if (initialized_) {
        return PluginError::AlreadyInitialized;
    }
    if (!host_) {
        PLUGIN_LOG(this, PluginLogLevel::Error, "缺少宿主上下文，无法注册模块数值");
        return PluginError::NotInitialized;
    }
    // 接口地址与轮询周期由宿主配置覆盖
    endpoint_ = QString::fromStdString(
        host_->get_config_value("app.module.warthunder.endpoint", "http://127.0.0.1:8111"));
    while (endpoint_.endsWith(QLatin1Char('/'))) {
        endpoint_.chop(1);
    }
    period_ms_ = parse_period_ms(host_->get_config_value("app.module.warthunder.period_ms", "500"));
    host_->register_module_values(name(), create_default_values(), {"A", "B"});

    network_ = std::make_unique<QNetworkAccessManager>();
    timer_ = std::make_unique<QTimer>();
    timer_->setInterval(period_ms_);
    QObject::connect(timer_.get(), &QTimer::timeout, timer_.get(), [this]() { request_state(); });
    initialized_ = true;
    timer_->start();
    request_state();
    PLUGIN_LOG(this, PluginLogLevel::Info,
        "战争雷霆遥测插件初始化完成，接口: " << endpoint_.toStdString() << "，周期: " << period_ms_ << "ms");
    return PluginError::Ok;
}

void WarThunderPlugin::uninitialize() {
    if (timer_) {
        timer_->stop();
    }
    if (reply_) {
        QNetworkReply* reply = reply_;
        reply_ = nullptr;
        // 中止可能同步触发 finished，先断开连接
        QObject::disconnect(reply, nullptr, nullptr, nullptr);
        reply->abort();
    }
    timer_.reset();
    // 应答由管理器持有，随管理器一同销毁
    network_.reset();
    if (host_) {
        host_->unregister_module(name());
    }
    initialized_ = false;
    last_log_ms_ = 0;
    PLUGIN_LOG(this, PluginLogLevel::Info, "战争雷霆遥测插件反初始化完成");
}

// 数值与配置解析（private，静态）

std::vector<ModuleValue> WarThunderPlugin::create_default_values() {
    std::vector<ModuleValue> values;
    values.emplace_back("speed", "空速", QueryPeriod::HALF_SECOND, "IAS, km/h", 0, 5000);
    values.emplace_back("altitude", "高度", QueryPeriod::HALF_SECOND, "altitude, m", -1000, 30000);
    values.emplace_back("valid", "数据有效", QueryPeriod::HALF_SECOND, "valid", 0, 1);
    return values;
}

int WarThunderPlugin::parse_period_ms(const std::string& text) {
    bool ok = false;
    const int period = QString::fromStdString(text).trimmed().toInt(&ok);
    if (!ok || period < 100) {
        return 500;
    }
    return period;
}

// 遥测轮询（private）

void WarThunderPlugin::request_state() {
    if (!network_ || reply_) {
        return;
    }
    QNetworkRequest request(QUrl(endpoint_ + QStringLiteral("/state")));
    request.setTransferTimeout(1500);
    QNetworkReply* reply = network_->get(request);
    if (!reply) {
        return;
    }
    reply_ = reply;
    QObject::connect(reply, &QNetworkReply::finished, reply, [this, reply]() {
        on_state_reply(reply);
        reply->deleteLater();
    });
}

void WarThunderPlugin::on_state_reply(QNetworkReply* reply) {
    reply_ = nullptr;
    if (!reply || !host_) {
        return;
    }
    if (reply->error() != QNetworkReply::NoError) {
        // 本地遥测服务未运行属正常路径：只记限流调试日志并标记无效
        log_throttled("遥测接口不可用: " + reply->errorString().toStdString());
        host_->set_value(name(), "valid", 0);
        return;
    }
    QJsonParseError parse_error;
    const QJsonDocument document = QJsonDocument::fromJson(reply->readAll(), &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
        log_throttled("遥测数据解析失败，跳过本帧");
        host_->set_value(name(), "valid", 0);
        return;
    }
    const QJsonObject state = document.object();
    const int speed = static_cast<int>(std::lround(read_airspeed(state)));
    const int altitude = static_cast<int>(std::lround(read_altitude(state)));
    host_->set_value(name(), "speed", speed);
    host_->set_value(name(), "altitude", altitude);
    host_->set_value(name(), "valid", read_valid(state));
}

void WarThunderPlugin::log_throttled(const std::string& message) {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - last_log_ms_ < k_log_interval_ms) {
        return;
    }
    last_log_ms_ = now;
    PLUGIN_LOG(this, PluginLogLevel::Debug, message);
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
    return new WarThunderPlugin();
}

PLUGIN_EXPORT void destroy_plugin(IPlugin* plugin) {
    delete plugin;
}
}
