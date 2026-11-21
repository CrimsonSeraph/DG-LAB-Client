/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "MarketBridge.h"

#include "AppConfig.h"
#include "DebugLog.h"
#include "ModuleManager.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSysInfo>
#include <QUrl>
#include <QVariantMap>

#include <string>

namespace {

    // 清单源地址的配置键与占位值（首次部署后由协调者回填真实地址）
    const char* const kRegistryUrlKey = "app.market.registry_url";
    const char* const kDefaultRegistryUrl = "https://dglab-registry.example.workers.dev/registry.json";
    // 清单 schema 版本（阶段二按同一份契约生成）
    constexpr int kSchemaVersion = 1;
    // 清单请求超时（毫秒）
    constexpr int kRequestTimeoutMs = 30000;

    /// @brief 是否为绝对 http(s) 地址（与 ModuleBridge::bannerSource 同一套头图判定）
    bool is_remote_url(const QString& text) {
        if (text.isEmpty()) {
            return false;
        }
        const QUrl url(text);
        return url.isValid()
            && (url.scheme() == QStringLiteral("http") || url.scheme() == QStringLiteral("https"));
    }

    /// @brief 读取 JSON 字符串字段（缺失或类型不符返回空串）
    QString json_string(const QJsonObject& object, const char* key) {
        const QJsonValue value = object.value(QString::fromUtf8(key));
        return value.isString() ? value.toString() : QString();
    }

    /// @brief 读取 JSON 字符串数组字段（跳过非字符串与空串）
    QStringList json_string_list(const QJsonObject& object, const char* key) {
        QStringList list;
        const QJsonValue value = object.value(QString::fromUtf8(key));
        if (!value.isArray()) {
            return list;
        }
        const QJsonArray array = value.toArray();
        for (const auto& item : array) {
            if (!item.isString()) {
                continue;
            }
            const QString text = item.toString().trimmed();
            if (!text.isEmpty()) {
                list.append(text);
            }
        }
        return list;
    }

    /// @brief 读取侧车 plugin.json 的指定字段；文件缺失或损坏返回空对象
    QJsonObject read_sidecar(const QString& path) {
        QFile file(path);
        if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
            return QJsonObject();
        }
        QJsonParseError parse_error;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parse_error);
        file.close();
        if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
            return QJsonObject();
        }
        return document.object();
    }

    /// @brief 把版本串拆成数字段（去掉前导 v，任一段不是数字则返回空列表）
    QList<int> version_segments(const QString& version) {
        QString text = version.trimmed();
        if (text.startsWith(QLatin1Char('v')) || text.startsWith(QLatin1Char('V'))) {
            text.remove(0, 1);
        }
        QList<int> segments;
        if (text.isEmpty()) {
            return segments;
        }
        const QStringList parts = text.split(QLatin1Char('.'));
        for (const QString& part : parts) {
            bool ok = false;
            const int value = part.toInt(&ok);
            if (!ok) {
                segments.clear();
                return segments;
            }
            segments.append(value);
        }
        return segments;
    }

} // namespace

MarketBridge::MarketBridge(QObject* parent)
    : QObject(parent)
    , source_url_(QString::fromStdString(
          AppConfig::instance().get_value<std::string>(kRegistryUrlKey, kDefaultRegistryUrl))) {
}

void MarketBridge::initialize() {
    ModuleManager& manager = ModuleManager::instance();
    // 安装动作只有一条下载链路：市场页只负责把进度与结果回写到清单项
    QObject::connect(&manager, &ModuleManager::plugin_install_progress, this,
        &MarketBridge::on_install_progress);
    QObject::connect(&manager, &ModuleManager::plugin_install_finished, this,
        &MarketBridge::on_install_finished);
}

QVariantList MarketBridge::plugins() const {
    QVariantList list;
    list.reserve(items_.size());
    for (const MarketItem& item : items_) {
        QVariantMap entry;
        entry.insert(QStringLiteral("id"), item.id);
        entry.insert(QStringLiteral("name"), item.name);
        entry.insert(QStringLiteral("version"), item.version);
        entry.insert(QStringLiteral("description"), item.description);
        entry.insert(QStringLiteral("tags"), item.tags);
        entry.insert(QStringLiteral("banner"), item.banner);
        entry.insert(QStringLiteral("localVersion"), item.local_version);
        entry.insert(QStringLiteral("status"), item.status);
        entry.insert(QStringLiteral("statusText"), item.status_text);
        entry.insert(QStringLiteral("actionText"), item.action_text);
        entry.insert(QStringLiteral("actionEnabled"), item.action_enabled);
        entry.insert(QStringLiteral("progress"), item.progress);
        entry.insert(QStringLiteral("error"), item.error);
        list.append(entry);
    }
    return list;
}

void MarketBridge::refresh() {
    if (manifest_reply_ != nullptr) {
        return;
    }
    last_error_.clear();
    loading_ = true;
    emit stateChanged();
    if (network_ == nullptr) {
        network_ = new QNetworkAccessManager(this);
    }
    QNetworkRequest request{QUrl(source_url_)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    request.setTransferTimeout(kRequestTimeoutMs);
#endif
    manifest_reply_ = network_->get(request);
    QObject::connect(manifest_reply_, &QNetworkReply::finished, this, &MarketBridge::on_manifest_reply);
    LOG_MODULE("MarketBridge", "refresh", LOG_INFO, "开始获取插件清单: " << source_url_.toStdString());
}

void MarketBridge::install(const QString& id) {
    if (!installing_id_.isEmpty()) {
        emit installFinished(id, false, QStringLiteral("已有插件包正在安装，请稍候再试"));
        return;
    }
    MarketItem* item = find_item(id);
    if (item == nullptr) {
        emit installFinished(id, false, QStringLiteral("插件 %1 不在市场清单里").arg(id));
        return;
    }
    if (!item->has_variant || item->download_url.isEmpty()) {
        item->error = QStringLiteral("当前平台无可用包");
        refresh_item_state(*item, false);
        emit pluginsChanged();
        emit installFinished(id, false, item->error);
        return;
    }
    if (item->status == QStringLiteral("installed")) {
        emit installFinished(id, false, QStringLiteral("插件 %1 已是最新版本").arg(item->name));
        return;
    }
    // 依赖校验放在下载之前：缺依赖时连下载都不发起
    const QHash<QString, QString> versions = installed_versions();
    for (const QString& dependency : item->dependencies) {
        if (versions.contains(dependency)) {
            continue;
        }
        item->error = QStringLiteral("缺少依赖插件 %1").arg(dependency);
        refresh_item_state(*item, false);
        emit pluginsChanged();
        emit installFinished(id, false, item->error);
        return;
    }
    item->error.clear();
    item->progress = 0.0;
    installing_id_ = id;
    refresh_item_state(*item, true);
    emit pluginsChanged();
    LOG_MODULE("MarketBridge", "install", LOG_INFO, "开始安装插件: " << id.toStdString());
    ModuleManager::instance().download_plugin(item->download_url.toStdString(), item->sha256.toStdString());
}

void MarketBridge::on_manifest_reply() {
    QNetworkReply* reply = manifest_reply_;
    manifest_reply_ = nullptr;
    if (reply == nullptr) {
        return;
    }
    reply->deleteLater();
    loading_ = false;
    if (reply->error() != QNetworkReply::NoError) {
        last_error_ = QStringLiteral("获取插件清单失败: ") + reply->errorString();
        emit stateChanged();
        emit refreshFinished(false, last_error_);
        return;
    }
    QJsonParseError parse_error;
    const QJsonDocument document = QJsonDocument::fromJson(reply->readAll(), &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
        last_error_ = QStringLiteral("插件清单格式错误：不是合法的 JSON 对象");
        emit stateChanged();
        emit refreshFinished(false, last_error_);
        return;
    }
    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("schemaVersion")).toInt(-1) != kSchemaVersion) {
        last_error_ = QStringLiteral("插件清单版本不受支持（期望 schemaVersion %1）").arg(kSchemaVersion);
        emit stateChanged();
        emit refreshFinished(false, last_error_);
        return;
    }
    if (!root.value(QStringLiteral("plugins")).isArray()) {
        last_error_ = QStringLiteral("插件清单缺少 plugins 数组");
        emit stateChanged();
        emit refreshFinished(false, last_error_);
        return;
    }
    const QString generated_at = root.value(QStringLiteral("generatedAt")).toString().trimmed();
    updated_at_ = generated_at.isEmpty()
        ? QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"))
        : generated_at;
    apply_manifest(root);
    emit stateChanged();
    LOG_MODULE("MarketBridge", "on_manifest_reply", LOG_INFO,
        "插件清单已更新，条目数: " << items_.size());
    emit refreshFinished(true, QString());
}

void MarketBridge::on_install_progress(const QString& target, qint64 received, qint64 total) {
    // 目标目录名要解压后才知道，进度按当前安装项回写
    Q_UNUSED(target)
    if (installing_id_.isEmpty()) {
        return;
    }
    MarketItem* item = find_item(installing_id_);
    if (item == nullptr) {
        return;
    }
    const double value = total > 0 ? static_cast<double>(received) / static_cast<double>(total) : 0.0;
    const double progress = qBound(0.0, value, 1.0);
    // 只在整数百分比变化时重算列表：整表重建的成本不该按数据块次数付
    if (qRound(progress * 100.0) == qRound(item->progress * 100.0)) {
        return;
    }
    item->progress = progress;
    refresh_item_state(*item, true);
    emit pluginsChanged();
}

void MarketBridge::on_install_finished(const QString& target, bool ok, const QString& error) {
    Q_UNUSED(target)
    const QString id = installing_id_;
    installing_id_.clear();
    MarketItem* item = id.isEmpty() ? nullptr : find_item(id);
    if (item != nullptr) {
        item->progress = ok ? 1.0 : 0.0;
        item->error = ok ? QString() : error;
        // 成功后重读本地版本：三态从 install / update 变成 installed
        item->local_version = installed_versions().value(item->id);
        refresh_item_state(*item, false);
        emit pluginsChanged();
    }
    if (ok) {
        LOG_MODULE("MarketBridge", "on_install_finished", LOG_INFO,
            "插件安装完成: " << id.toStdString());
    }
    else {
        LOG_MODULE("MarketBridge", "on_install_finished", LOG_WARN,
            "插件安装失败: " << error.toStdString());
    }
    emit installFinished(id, ok, error);
}

QHash<QString, QString> MarketBridge::installed_versions() const {
    QHash<QString, QString> versions;
    const QDir root(QString::fromStdString(ModuleManager::instance().get_plugin_dir()));
    if (!root.exists()) {
        return versions;
    }
    const QStringList subdirs = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& name : subdirs) {
        // .staging / .backup 是安装过程的临时目录，不算已安装
        if (name.startsWith(QLatin1Char('.'))) {
            continue;
        }
        const QJsonObject meta = read_sidecar(root.filePath(name + QStringLiteral("/plugin.json")));
        const QString version = json_string(meta, "version").trimmed();
        versions.insert(name, version);
        const QString id = json_string(meta, "id").trimmed();
        if (!id.isEmpty()) {
            versions.insert(id, version);
        }
    }
    return versions;
}

QString MarketBridge::platform_key() const {
    const QString kernel = QSysInfo::kernelType().toLower();
    QString os_name;
    if (kernel == QStringLiteral("winnt")) {
        os_name = QStringLiteral("windows");
    }
    else if (kernel == QStringLiteral("linux")) {
        os_name = QStringLiteral("linux");
    }
    else if (kernel == QStringLiteral("darwin")) {
        os_name = QStringLiteral("macos");
    }
    else {
        return QString();
    }
    const QString architecture = QSysInfo::currentCpuArchitecture().toLower();
    QString arch_name;
    if (architecture == QStringLiteral("x86_64") || architecture == QStringLiteral("amd64")) {
        arch_name = QStringLiteral("x64");
    }
    else if (architecture == QStringLiteral("arm64") || architecture == QStringLiteral("aarch64")) {
        arch_name = QStringLiteral("arm64");
    }
    else {
        return QString();
    }
    return os_name + QLatin1Char('-') + arch_name;
}

void MarketBridge::apply_manifest(const QJsonObject& root) {
    // 正在安装的项在一次刷新后要保留进度，不能因为清单重排而清零
    double installing_progress = 0.0;
    if (!installing_id_.isEmpty()) {
        for (const MarketItem& item : items_) {
            if (item.id == installing_id_) {
                installing_progress = item.progress;
                break;
            }
        }
    }
    const QHash<QString, QString> versions = installed_versions();
    const QString platform = platform_key();
    QList<MarketItem> rebuilt;
    const QJsonArray plugins = root.value(QStringLiteral("plugins")).toArray();
    for (const auto& element : plugins) {
        if (!element.isObject()) {
            continue;
        }
        const QJsonObject object = element.toObject();
        MarketItem item;
        item.id = json_string(object, "id").trimmed();
        if (item.id.isEmpty()) {
            continue;
        }
        item.name = json_string(object, "name").trimmed();
        if (item.name.isEmpty()) {
            item.name = item.id;
        }
        item.version = json_string(object, "version").trimmed();
        item.description = json_string(object, "description");
        // 头图只认绝对 http(s)：与 ModuleBridge::bannerSource 同一套判定，其它写法在这里就判空
        const QString banner = json_string(object, "banner").trimmed();
        item.banner = is_remote_url(banner) ? banner : QString();
        item.tags = json_string_list(object, "tags");
        item.dependencies = json_string_list(object, "dependencies");
        item.local_version = versions.value(item.id);
        // 变体只取当前平台那一条，地址必须是 http(s)
        if (!platform.isEmpty()) {
            const QJsonArray variants = object.value(QStringLiteral("variants")).toArray();
            for (const auto& variant_value : variants) {
                if (!variant_value.isObject()) {
                    continue;
                }
                const QJsonObject variant = variant_value.toObject();
                if (json_string(variant, "platform").trimmed() != platform) {
                    continue;
                }
                const QString url = json_string(variant, "url").trimmed();
                if (!is_remote_url(url)) {
                    continue;
                }
                item.download_url = url;
                item.sha256 = json_string(variant, "sha256").trimmed().toLower();
                item.has_variant = true;
                break;
            }
        }
        const bool installing = !installing_id_.isEmpty() && item.id == installing_id_;
        item.progress = installing ? installing_progress : 0.0;
        refresh_item_state(item, installing);
        rebuilt.append(item);
    }
    items_ = rebuilt;
    emit pluginsChanged();
}

void MarketBridge::refresh_item_state(MarketItem& item, bool installing) const {
    if (installing) {
        item.status = QStringLiteral("installing");
        item.status_text = item.progress > 0.0
            ? QStringLiteral("正在安装 %1%").arg(qRound(item.progress * 100.0))
            : QStringLiteral("正在安装…");
        item.action_text = QStringLiteral("安装中…");
        item.action_enabled = false;
        return;
    }
    if (item.local_version.isEmpty()) {
        item.status = QStringLiteral("install");
        item.status_text = QStringLiteral("未安装");
    }
    else if (!item.version.isEmpty() && is_newer(item.version, item.local_version)) {
        item.status = QStringLiteral("update");
        item.status_text = QStringLiteral("可更新 v%1 → v%2").arg(item.local_version, item.version);
    }
    else {
        item.status = QStringLiteral("installed");
        item.status_text = QStringLiteral("已安装 v%1").arg(item.local_version);
    }
    if (!item.has_variant) {
        item.status_text = QStringLiteral("当前平台无可用包");
        item.action_text = QStringLiteral("不可用");
        item.action_enabled = false;
        return;
    }
    if (item.status == QStringLiteral("install")) {
        item.action_text = QStringLiteral("安装");
    }
    else if (item.status == QStringLiteral("update")) {
        item.action_text = QStringLiteral("更新");
    }
    else {
        item.action_text = QStringLiteral("已安装");
    }
    item.action_enabled = item.status != QStringLiteral("installed");
}

MarketBridge::MarketItem* MarketBridge::find_item(const QString& id) {
    if (id.isEmpty()) {
        return nullptr;
    }
    for (MarketItem& item : items_) {
        if (item.id == id) {
            return &item;
        }
    }
    return nullptr;
}

bool MarketBridge::is_newer(const QString& remote, const QString& local) {
    const QList<int> remote_segments = version_segments(remote);
    const QList<int> local_segments = version_segments(local);
    if (remote_segments.isEmpty() || local_segments.isEmpty()) {
        // 拆不出数字段时只按是否不同判断，避免把无法比较的版本当成「已是最新」
        return remote.trimmed() != local.trimmed();
    }
    const qsizetype count = remote_segments.size() > local_segments.size()
        ? remote_segments.size()
        : local_segments.size();
    for (qsizetype index = 0; index < count; ++index) {
        const int remote_part = index < remote_segments.size() ? remote_segments.at(index) : 0;
        const int local_part = index < local_segments.size() ? local_segments.at(index) : 0;
        if (remote_part != local_part) {
            return remote_part > local_part;
        }
    }
    return false;
}
