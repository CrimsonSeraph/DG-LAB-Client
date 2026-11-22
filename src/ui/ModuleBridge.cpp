// Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
// SPDX-License-Identifier: GPL-3.0-only

#include "ModuleBridge.h"

#include "DebugLog.h"
#include "ModuleManager.h"

#include <QDir>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QSet>
#include <QStringList>
#include <QUrl>
#include <QVariantMap>

#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace {

    // 周期选项（文本 -> 毫秒），与 QueryPeriod 对应；文本直接用毫秒数值，避免口语量词
    const std::pair<const char*, int> kPeriodOptions[] = {
        { "250 ms", 250 },
        { "500 ms", 500 },
        { "1000 ms", 1000 },
        { "2000 ms", 2000 },
        { "4000 ms", 4000 }
    };

    // 头图缓存的并发下载上限：一屏卡片再多也只排这么多，其余等下一次界面刷新
    constexpr int kMaxBannerDownloads = 4;
    // 头图下载超时（毫秒）：卡片头图不值得等太久
    constexpr int kBannerRequestTimeoutMs = 15000;

    // 头图字段是否绝对 http(s) URL：只有这两种 scheme 直接当 Image.source 用
    bool is_remote_banner(const QString& banner) {
        return banner.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive)
            || banner.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive);
    }

    // 文件名要能直接进 image://modulebanner 的 id：非空、非当前/上级目录、不含分隔符与盘符
    bool is_safe_banner_name(const QString& name) {
        if (name.isEmpty() || name == QStringLiteral(".") || name == QStringLiteral("..")) {
            return false;
        }
        return !name.contains(QLatin1Char('/')) && !name.contains(QLatin1Char('\\'))
            && !name.contains(QLatin1Char(':'));
    }

    // 能直接拼进 image:// 的缓存文件名：只收 ASCII 字母数字与 . _ -，其它写法不进缓存
    // 这样拼出来的 id 不需要再转义，也不会被 URL 解析吃掉字符
    bool is_cacheable_banner_name(const QString& name) {
        if (!is_safe_banner_name(name)) {
            return false;
        }
        for (const QChar ch : name) {
            const bool known = (ch >= QLatin1Char('a') && ch <= QLatin1Char('z'))
                || (ch >= QLatin1Char('A') && ch <= QLatin1Char('Z'))
                || (ch >= QLatin1Char('0') && ch <= QLatin1Char('9'))
                || ch == QLatin1Char('.') || ch == QLatin1Char('_') || ch == QLatin1Char('-');
            if (!known) {
                return false;
            }
        }
        return true;
    }

    // 远程地址里的文件名（QUrl::fileName 已去掉查询串与末级分隔符）
    QString remote_banner_name(const QString& banner) {
        const QString name = QUrl(banner).fileName();
        return is_cacheable_banner_name(name) ? name : QString();
    }

    // 写临时文件再原子改名（QSaveFile 自带暂存与提交），失败时不留半截文件
    bool write_file_atomically(const QString& path, const QByteArray& payload) {
        const QDir dir = QFileInfo(path).absoluteDir();
        if (!dir.exists() && !QDir().mkpath(dir.absolutePath())) {
            return false;
        }
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            return false;
        }
        if (file.write(payload) != payload.size()) {
            file.cancelWriting();
            return false;
        }
        return file.commit();
    }

    // 标签列表转换为 QStringList（QML 直接消费）
    QStringList to_qstring_list(const std::vector<std::string>& values) {
        QStringList list;
        for (const auto& value : values) {
            list.append(QString::fromStdString(value));
        }
        return list;
    }

} // namespace

ModuleBridge::ModuleBridge(QObject* parent)
    : QObject(parent) {
}

void ModuleBridge::initialize() {
    auto& manager = ModuleManager::instance();
    connect(&manager, &ModuleManager::value_changed, this, [this](const QString&, const QString&, int) {
        emit selectedChanged();
        emit dataChanged();
    });
    connect(&manager, &ModuleManager::period_changed, this, &ModuleBridge::dataChanged);
    connect(&manager, &ModuleManager::plugin_state_changed, this, &ModuleBridge::dataChanged);
    LOG_MODULE("ModuleBridge", "initialize", LOG_INFO, "模块桥接初始化完成");
}

QVariantList ModuleBridge::modules() const {
    QVariantList list;
    auto& manager = ModuleManager::instance();
    QSet<QString> loaded_names;

    for (const auto& plugin : manager.get_plugins()) {
        const QString name = QString::fromStdString(plugin.display_name);
        const QString module_name = QString::fromStdString(plugin.module_name);
        QVariantMap item;
        item.insert(QStringLiteral("kind"), QStringLiteral("plugin"));
        item.insert(QStringLiteral("fileName"), QString::fromStdString(plugin.file_name));
        item.insert(QStringLiteral("name"), name);
        item.insert(QStringLiteral("version"), QString::fromStdString(plugin.version));
        item.insert(QStringLiteral("error"), QString::fromStdString(plugin.error));
        item.insert(QStringLiteral("banner"), QString::fromStdString(plugin.banner));
        item.insert(QStringLiteral("author"), QString::fromStdString(plugin.author));
        item.insert(QStringLiteral("description"), QString::fromStdString(plugin.description));
        item.insert(QStringLiteral("tags"), to_qstring_list(plugin.tags));
        item.insert(QStringLiteral("moduleName"), module_name);

        QString state_text;
        QString state;
        bool enabled = false;
        switch (plugin.state) {
        case ModuleManager::PluginLoadState::Loaded:
            state_text = QStringLiteral("已加载");
            state = QStringLiteral("loaded");
            enabled = true;
            // 去重按模块标识，displayName 只用于展示
            if (!module_name.isEmpty()) {
                loaded_names.insert(module_name);
            }
            break;
        case ModuleManager::PluginLoadState::Failed:
            state_text = QStringLiteral("加载失败");
            state = QStringLiteral("failed");
            break;
        default:
            state_text = QStringLiteral("暂未加载");
            state = QStringLiteral("notLoaded");
            break;
        }
        item.insert(QStringLiteral("state"), state);
        item.insert(QStringLiteral("stateText"), state_text);
        item.insert(QStringLiteral("enabled"), enabled);
        int period_ms = 0;
        if (enabled && !module_name.isEmpty()) {
            period_ms = manager.get_module_min_period_ms(plugin.module_name);
        }
        item.insert(QStringLiteral("periodMs"), period_ms);
        list.append(item);
    }

    for (const auto& raw_name : manager.get_module_names()) {
        const QString name = QString::fromStdString(raw_name);
        if (loaded_names.contains(name)) {
            continue;
        }
        QVariantMap item;
        item.insert(QStringLiteral("kind"), QStringLiteral("module"));
        item.insert(QStringLiteral("fileName"), QString());
        item.insert(QStringLiteral("name"), name);
        item.insert(QStringLiteral("moduleName"), name);
        item.insert(QStringLiteral("version"), QString());
        item.insert(QStringLiteral("error"), QString());
        item.insert(QStringLiteral("state"), QStringLiteral("builtin"));
        item.insert(QStringLiteral("stateText"), QStringLiteral("内置模块"));
        item.insert(QStringLiteral("enabled"), true);
        item.insert(QStringLiteral("periodMs"), manager.get_module_min_period_ms(raw_name));
        list.append(item);
    }
    return list;
}

QVariantList ModuleBridge::period_options() const {
    QVariantList list;
    for (const auto& option : kPeriodOptions) {
        QVariantMap item;
        item.insert(QStringLiteral("text"), QString::fromUtf8(option.first));
        item.insert(QStringLiteral("ms"), option.second);
        list.append(item);
    }
    return list;
}

int ModuleBridge::base_period_ms() const {
    return ModuleManager::instance().get_base_period_ms();
}

int ModuleBridge::uniform_period_index() const {
    auto& manager = ModuleManager::instance();
    int uniform_ms = -1;
    for (const auto& name : manager.get_module_names()) {
        const Module* module = manager.get_module(name);
        if (module == nullptr) {
            continue;
        }
        for (const auto& value : module->get_values()) {
            const int ms = query_period_to_ms(value.get_query_period());
            if (uniform_ms < 0) {
                uniform_ms = ms;
            }
            else if (ms != uniform_ms) {
                return -1;
            }
        }
    }
    if (uniform_ms < 0) {
        // 未注册任何数值时按基准周期回显，避免下拉空置
        uniform_ms = manager.get_base_period_ms();
    }
    for (int index = 0; index < static_cast<int>(std::size(kPeriodOptions)); ++index) {
        if (kPeriodOptions[index].second == uniform_ms) {
            return index;
        }
    }
    return -1;
}

QString ModuleBridge::selected_display_name() const {
    if (selected_.isEmpty()) {
        return selected_;
    }
    const std::string selected = selected_.toStdString();
    for (const auto& plugin : ModuleManager::instance().get_plugins()) {
        if (plugin.module_name != selected || plugin.display_name.empty()) {
            continue;
        }
        return QString::fromStdString(plugin.display_name);
    }
    // 内置模块与未匹配场景回退模块标识，行为与旧版一致
    return selected_;
}

QVariantList ModuleBridge::selected_values() const {
    QVariantList list;
    if (selected_.isEmpty()) {
        return list;
    }
    const Module* module = ModuleManager::instance().get_module(selected_.toStdString());
    if (module == nullptr) {
        return list;
    }
    for (const auto& value : module->get_values()) {
        QVariantMap item;
        item.insert(QStringLiteral("id"), QString::fromStdString(value.get_id()));
        item.insert(QStringLiteral("name"), QString::fromStdString(value.get_name()));
        item.insert(QStringLiteral("value"), value.get_has_value() ? QVariant(value.get_last_value()) : QVariant());
        item.insert(QStringLiteral("min"), value.get_min().has_value() ? QVariant(value.get_min().value()) : QVariant());
        item.insert(QStringLiteral("max"), value.get_max().has_value() ? QVariant(value.get_max().value()) : QVariant());
        item.insert(QStringLiteral("periodText"), QString::fromUtf8(query_period_to_text(value.get_query_period())));
        list.append(item);
    }
    return list;
}

void ModuleBridge::applyAllPeriod(int index) {
    if (index < 0 || index >= static_cast<int>(std::size(kPeriodOptions))) {
        return;
    }
    const int ms = kPeriodOptions[index].second;
    ModuleManager::instance().set_all_period(query_period_from_ms(ms));
    LOG_MODULE("ModuleBridge", "applyAllPeriod", LOG_INFO, "统一查询周期已设置");
    emit dataChanged();
}

void ModuleBridge::togglePlugin(const QString& file_name) {
    if (file_name.isEmpty()) {
        return;
    }
    auto& manager = ModuleManager::instance();
    bool found = false;
    bool was_loaded = false;
    for (const auto& plugin : manager.get_plugins()) {
        if (QString::fromStdString(plugin.file_name) != file_name) {
            continue;
        }
        found = true;
        was_loaded = plugin.state == ModuleManager::PluginLoadState::Loaded;
        break;
    }
    if (!found) {
        emit pluginToggled(file_name, false, QStringLiteral("插件不存在：") + file_name);
        emit dataChanged();
        return;
    }

    const bool ok = was_loaded ? manager.unload_plugin(file_name.toStdString())
                               : manager.load_plugin(file_name.toStdString());
    QString error;
    if (!ok) {
        // 失败原因以管理器记录的加载错误为准；卸载被插件拒绝时没有 error 字段
        for (const auto& plugin : manager.get_plugins()) {
            if (QString::fromStdString(plugin.file_name) == file_name) {
                error = QString::fromStdString(plugin.error);
                break;
            }
        }
        if (error.isEmpty()) {
            error = was_loaded ? QStringLiteral("插件拒绝卸载，仍处于启用状态")
                               : QStringLiteral("插件加载失败");
        }
    }
    emit pluginToggled(file_name, ok, error);
    emit dataChanged();
}

void ModuleBridge::selectModule(const QString& module_name) {
    selected_ = module_name;
    emit selectedChanged();
}

QString ModuleBridge::bannerSource(const QString& file_name, const QString& banner) {
    if (file_name.isEmpty() || banner.isEmpty()) {
        return QString();
    }
    // 在线头图先查本地缓存：命中就走提供器，未命中仍用远程地址显示并异步落盘
    if (is_remote_banner(banner)) {
        const QString cached = cached_banner_source(file_name, banner);
        return cached.isEmpty() ? banner : cached;
    }
    // 纯文件名才走 image://modulebanner/<插件文件名>/<头图文件名>，带路径分隔符或盘符的按无图处理
    if (banner.contains(QLatin1Char('/')) || banner.contains(QLatin1Char('\\'))
        || banner.contains(QLatin1Char(':'))) {
        return QString();
    }
    return QStringLiteral("image://modulebanner/") + file_name + QLatin1Char('/') + banner;
}

QString ModuleBridge::banner_cache_path(const QString& file_name, const QString& banner) {
    const QString name = remote_banner_name(banner);
    if (name.isEmpty()) {
        return QString();
    }
    // 缓存目录与提供器的解析口径一致：插件扫描目录 + 动态库主干名
    const QString plugin_dir = QString::fromStdString(ModuleManager::instance().get_plugin_dir());
    const QString stem = QFileInfo(file_name).completeBaseName();
    if (plugin_dir.isEmpty() || !is_safe_banner_name(stem)) {
        return QString();
    }
    return QDir(plugin_dir).filePath(stem + QLatin1Char('/') + name);
}

QString ModuleBridge::cached_banner_source(const QString& file_name, const QString& banner) {
    const QString path = banner_cache_path(file_name, banner);
    if (path.isEmpty()) {
        // 地址取不出可用文件名：保持原逻辑，直接联网显示，不落盘
        return QString();
    }
    const QString source = QStringLiteral("image://modulebanner/") + file_name
        + QLatin1Char('/') + QFileInfo(path).fileName();
    if (banner_cached_.contains(path)) {
        return source;
    }
    // 失败过或正在下载的目标不再重复发起，避免卡片重建时反复联网
    if (banner_pending_.contains(path) || banner_failed_.contains(path)) {
        return QString();
    }
    const QFileInfo info(path);
    if (!info.isFile() || info.size() <= 0) {
        start_banner_download(banner, path);
        return QString();
    }
    banner_cached_.insert(path);
    LOG_MODULE("ModuleBridge", "cached_banner_source", LOG_DEBUG,
        "头图缓存命中: " << path.toStdString());
    return source;
}

void ModuleBridge::start_banner_download(const QString& banner, const QString& path) {
    if (banner_pending_.size() >= kMaxBannerDownloads) {
        // 超出并发上限：不记失败，下一次界面刷新会再试
        return;
    }
    if (banner_network_ == nullptr) {
        banner_network_ = new QNetworkAccessManager(this);
    }
    QNetworkRequest request{QUrl(banner)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    request.setTransferTimeout(kBannerRequestTimeoutMs);
#endif
    QNetworkReply* reply = banner_network_->get(request);
    reply->setProperty("cachePath", path);
    reply->setProperty("bannerUrl", banner);
    banner_pending_.insert(path);
    QObject::connect(reply, &QNetworkReply::finished, this, &ModuleBridge::on_banner_finished);
    LOG_MODULE("ModuleBridge", "start_banner_download", LOG_DEBUG,
        "开始下载头图缓存: " << banner.toStdString() << " → " << path.toStdString());
}

void ModuleBridge::on_banner_finished() {
    auto* reply = qobject_cast<QNetworkReply*>(sender());
    if (reply == nullptr) {
        return;
    }
    reply->deleteLater();
    const QString path = reply->property("cachePath").toString();
    const QString url = reply->property("bannerUrl").toString();
    if (path.isEmpty()) {
        return;
    }
    banner_pending_.remove(path);
    if (reply->error() != QNetworkReply::NoError) {
        banner_failed_.insert(path);
        LOG_MODULE("ModuleBridge", "on_banner_finished", LOG_WARN,
            "头图缓存下载失败: " << url.toStdString() << "，原因: " << reply->errorString().toStdString());
        return;
    }
    const QByteArray payload = reply->readAll();
    if (payload.isEmpty() || !write_file_atomically(path, payload)) {
        banner_failed_.insert(path);
        LOG_MODULE("ModuleBridge", "on_banner_finished", LOG_WARN,
            "头图缓存写入失败: " << path.toStdString());
        return;
    }
    banner_cached_.insert(path);
    LOG_MODULE("ModuleBridge", "on_banner_finished", LOG_INFO,
        "头图已写入缓存: " << path.toStdString() << "，字节数: " << payload.size());
    // 卡片与详情重新取源：bannerSource 这次会返回 image://modulebanner 的本地缓存
    emit dataChanged();
}
