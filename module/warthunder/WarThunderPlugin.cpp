/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "WarThunderPlugin.h"

#include <QDateTime>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QString>
#include <QUrl>

#include <cmath>

// 调试日志限流间隔（毫秒）
static const qint64 k_log_interval_ms = 1000;

// 遥测数值来源与定标描述（与 README 数值表逐项对应）
struct FieldSpec {
    const char* value_id;     ///< 注册的数值 ID（侧车事件按此引用）
    const char* display_name; ///< 数值中文名称（模块页展示）
    const char* primary_key;  ///< 来源主键（规范化后比较）
    const char* fallback_key; ///< 后备键，空串表示无后备
    double factor;            ///< 定标倍数：原值 × factor 后四舍五入
    int min_value;            ///< 写入下限（越界钳制）
    int max_value;            ///< 写入上限（越界钳制）
};

// 数值表：主键取不到数值时尝试后备键，两者都取不到就跳过该数值，保留上一次的值
static const FieldSpec k_fields[] = {
    {"speed", "空速", "IAS, km/h", "TAS, km/h", 1.0, 0, 5000},
    {"tas", "真空速", "TAS, km/h", "IAS, km/h", 1.0, 0, 5000},
    {"mach", "马赫数", "M", "MACH", 1000.0, 0, 5000},
    {"altitude", "高度", "altitude, m", "H, m", 1.0, -1000, 30000},
    {"climb", "垂直速度", "Vy, m/s", "vertical speed, m/s", 10.0, -10000, 10000},
    {"aoa", "迎角", "AoA, deg", "AoA", 10.0, -900, 900},
    {"g_load", "过载", "Ny", "Ny, g", 10.0, -500, 500},
    {"throttle", "油门", "throttle 1, %", "throttle, %", 1.0, 0, 100},
    {"rpm", "转速", "RPM 1, %", "RPM 1", 1.0, 0, 100},
    {"gear", "起落架", "gear, %", "gear", 1.0, 0, 1},
    {"flaps", "襟翼", "flaps, %", "flaps", 1.0, 0, 100},
    {"valid", "数据有效", "valid", "", 1.0, 0, 1},
};

// 数值表条目数
static const int k_field_count = static_cast<int>(sizeof(k_fields) / sizeof(k_fields[0]));

// 键名规范化：转小写并去掉空白，用于大小写与空格不敏感匹配
static QString normalize_key(const QString& key) {
    QString normalized;
    normalized.reserve(key.size());
    for (int i = 0; i < key.size(); ++i) {
        const QChar character = key.at(i);
        if (character.isSpace()) {
            continue;
        }
        normalized.append(character.toLower());
    }
    return normalized;
}

// 建立规范化键名索引；同一规范化键名重复出现时保留首个
static QHash<QString, QJsonValue> build_index(const QJsonObject& state) {
    QHash<QString, QJsonValue> index;
    index.reserve(state.size());
    for (auto it = state.constBegin(); it != state.constEnd(); ++it) {
        const QString key = normalize_key(it.key());
        if (key.isEmpty() || index.contains(key)) {
            continue;
        }
        index.insert(key, it.value());
    }
    return index;
}

// 取数值：接受数字、布尔（false → 0，true → 1）与数字字符串，其它类型视为缺失
static bool read_number(const QJsonValue& value, double* out) {
    if (value.isDouble()) {
        const double number = value.toDouble();
        if (!std::isfinite(number)) {
            return false;
        }
        *out = number;
        return true;
    }
    if (value.isBool()) {
        *out = value.toBool() ? 1.0 : 0.0;
        return true;
    }
    if (value.isString()) {
        bool ok = false;
        const double number = value.toString().trimmed().toDouble(&ok);
        if (!ok || !std::isfinite(number)) {
            return false;
        }
        *out = number;
        return true;
    }
    return false;
}

// 按主键、后备键顺序取值；主键存在但不是数值时继续尝试后备键
static bool find_number(const QHash<QString, QJsonValue>& index, const char* primary_key,
    const char* fallback_key, double* out) {
    const char* keys[] = {primary_key, fallback_key};
    for (const char* key : keys) {
        if (!key || key[0] == '\0') {
            continue;
        }
        const auto it = index.constFind(normalize_key(QString::fromLatin1(key)));
        if (it == index.constEnd()) {
            continue;
        }
        if (read_number(it.value(), out)) {
            return true;
        }
    }
    return false;
}

// 定标：原值乘倍数四舍五入后钳到写入范围
static int scale_value(double raw, const FieldSpec& spec) {
    if (!std::isfinite(raw)) {
        return 0;
    }
    const double scaled = raw * spec.factor;
    if (scaled <= static_cast<double>(spec.min_value)) {
        return spec.min_value;
    }
    if (scaled >= static_cast<double>(spec.max_value)) {
        return spec.max_value;
    }
    return static_cast<int>(std::lround(scaled));
}

// 按数值表写入一帧遥测：仅更新当帧出现的字段，缺失字段保留上一次的值
static void write_telemetry(IPluginHost* host, const std::string& module_name,
    const QHash<QString, QJsonValue>& index) {
    for (int i = 0; i < k_field_count; ++i) {
        const FieldSpec& spec = k_fields[i];
        double raw = 0.0;
        if (!find_number(index, spec.primary_key, spec.fallback_key, &raw)) {
            continue;
        }
        host->set_value(module_name, spec.value_id, scale_value(raw, spec));
    }
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
    for (int i = 0; i < k_field_count; ++i) {
        const FieldSpec& spec = k_fields[i];
        values.emplace_back(spec.value_id, spec.display_name, QueryPeriod::HALF_SECOND,
            spec.primary_key, spec.min_value, spec.max_value);
    }
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
    // 只写当帧出现的字段：缺失的键不写 0，避免把上一次的值清零
    const QJsonObject state = document.object();
    write_telemetry(host_, name(), build_index(state));
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
