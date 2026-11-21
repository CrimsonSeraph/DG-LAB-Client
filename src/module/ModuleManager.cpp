/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "ModuleManager.h"

#include "AppConfig.h"
#include "DataListener.h"
#include "DebugLog.h"
#include "IPlugin.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLibrary>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPair>
#include <QSet>
#include <QStandardPaths>
#include <QStringList>
#include <QUrl>
#include <QtConcurrent>

// miniz 3.0.2（MIT，third_party/miniz）：插件包解压只用内存 zip 读 API
#include <miniz.h>

#include <cstring>

#include <algorithm>
#include <tuple>
#include <utility>

// 导出函数指针类型（extern "C" 约定）
using PluginApiVersionFn = int (*)();
using PluginCreateFn = IPlugin* (*)();
using PluginDestroyFn = void (*)(IPlugin*);

namespace {

    // 解压防护口径与波形 zip（wave_io::parse_zip）一致：单文件 4 MiB、总量 32 MiB、最多 512 个条目
    constexpr qint64 kMaxEntryBytes = 4 * 1024 * 1024;
    constexpr qint64 kMaxTotalBytes = 32 * 1024 * 1024;
    constexpr int kMaxEntries = 512;
    // 下载体积上限（压缩后 64 MiB）：超出立即中止，不落无界临时文件
    constexpr qint64 kMaxDownloadBytes = 64 * 1024 * 1024;
    // 暂存与备份目录：都放在插件目录内，同一文件系统上目录改名才是原子的
    const char* const kStagingDirName = ".staging";
    const char* const kBackupDirName = ".backup";
    // 插件动态库扩展名（比较时统一转小写）
    const char* const kLibrarySuffixes[] = { "dll", "so", "dylib" };

    /// @brief SHA256 文本是否为小写十六进制 64 位
    bool is_sha256_text(const QString& text) {
        if (text.size() != 64) {
            return false;
        }
        for (const QChar ch : text) {
            const bool is_digit = ch >= QLatin1Char('0') && ch <= QLatin1Char('9');
            const bool is_letter = ch >= QLatin1Char('a') && ch <= QLatin1Char('f');
            if (!is_digit && !is_letter) {
                return false;
            }
        }
        return true;
    }

    /// @brief 按 UTF-8 解码 zip 条目名；非法 UTF-8 回退本地 8 位编码（部分压缩工具写 GBK）
    QString decode_entry_name(const char* raw) {
        const QByteArray bytes(raw);
        const QString name = QString::fromUtf8(bytes);
        if (name.contains(QChar::ReplacementCharacter)) {
            return QString::fromLocal8Bit(bytes);
        }
        return name;
    }

    /// @brief 条目名是否含绝对路径或 .. 路径段（路径穿越）
    bool unsafe_entry_name(const QString& name) {
        if (name.startsWith(QLatin1Char('/')) || name.startsWith(QLatin1Char('\\'))) {
            return true;
        }
        if (name.size() >= 2 && name.at(1) == QLatin1Char(':') && name.at(0).isLetter()) {
            return true;
        }
        const QString normalized = QString(name).replace(QLatin1Char('\\'), QLatin1Char('/'));
        for (const QString& part : normalized.split(QLatin1Char('/'))) {
            if (part == QStringLiteral("..")) {
                return true;
            }
        }
        return false;
    }

    /// @brief 是否为插件动态库文件（.dll / .so / .dylib）
    bool is_library_file(const QString& name) {
        const QString suffix = QFileInfo(name).suffix().toLower();
        for (const char* candidate : kLibrarySuffixes) {
            if (suffix == QLatin1String(candidate)) {
                return true;
            }
        }
        return false;
    }

    /// @brief 递归删除目录，目录不存在视为成功
    bool remove_dir_tree(const QString& path) {
        QDir dir(path);
        if (!dir.exists()) {
            return true;
        }
        return dir.removeRecursively();
    }

    /// @brief 读取 JSON 对象文件；缺失、损坏或顶层非对象返回空对象
    QJsonObject read_json_object(const QString& path) {
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

    /// @brief 本地已装插件标识：<插件目录>/<目录>/plugin.json 的 id 与目录名都算（依赖按插件标识声明）
    QSet<QString> installed_plugin_ids(const QString& plugin_dir) {
        QSet<QString> ids;
        QDir root(plugin_dir);
        const QStringList subdirs = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString& name : subdirs) {
            // .staging / .backup 是安装过程的临时目录，不算已安装
            if (name.startsWith(QLatin1Char('.'))) {
                continue;
            }
            ids.insert(name);
            const QString id = json_string(
                read_json_object(root.filePath(name + QStringLiteral("/plugin.json"))), "id").trimmed();
            if (!id.isEmpty()) {
                ids.insert(id);
            }
        }
        return ids;
    }

    /// @brief 依赖校验：暂存目录里 plugin.json 声明的依赖必须都已安装在插件目录
    bool dependencies_satisfied(const QString& staging_top, const QString& plugin_dir, QString& missing) {
        missing.clear();
        const QStringList dependencies = json_string_list(
            read_json_object(QDir(staging_top).filePath(QStringLiteral("plugin.json"))), "dependencies");
        if (dependencies.isEmpty()) {
            return true;
        }
        const QSet<QString> installed = installed_plugin_ids(plugin_dir);
        for (const QString& dependency : dependencies) {
            if (!installed.contains(dependency)) {
                missing = dependency;
                return false;
            }
        }
        return true;
    }

    /// @brief 把暂存目录整体替换到插件目录；失败时把旧版本挪回原位，插件目录保持安装前的样子
    bool replace_plugin_dir(const QString& staging_top, const QString& target_dir, const QString& backup_dir,
        QString& error) {
        error.clear();
        const QString backup_path = QDir(backup_dir).filePath(QFileInfo(target_dir).fileName());
        // 上一轮残留的备份先清掉，避免把本次旧版本挪进非空目录
        remove_dir_tree(backup_path);
        const bool had_old = QFileInfo::exists(target_dir);
        if (had_old) {
            if (!QDir().mkpath(backup_dir)) {
                error = QStringLiteral("无法创建插件备份目录");
                return false;
            }
            if (!QDir().rename(target_dir, backup_path)) {
                error = QStringLiteral("无法备份已安装的旧版本插件，安装已取消");
                return false;
            }
        }
        if (!QDir().rename(staging_top, target_dir)) {
            if (had_old) {
                QDir().rename(backup_path, target_dir);
                error = QStringLiteral("无法替换插件目录，已回滚到安装前的版本");
            }
            else {
                error = QStringLiteral("无法把解压结果移入插件目录");
            }
            return false;
        }
        if (had_old) {
            remove_dir_tree(backup_path);
        }
        return true;
    }

    /// @brief 解压插件包到暂存目录并校验结构；所有条目必须共享同一个顶层目录
    bool extract_plugin_archive(const QByteArray& archive, const QString& staging_root, QString& top_dir,
        QString& error) {
        top_dir.clear();
        error.clear();
        mz_zip_archive zip_archive;
        std::memset(&zip_archive, 0, sizeof(zip_archive));
        if (!mz_zip_reader_init_mem(&zip_archive, archive.constData(),
                static_cast<size_t>(archive.size()), 0)) {
            error = QStringLiteral("不是有效的插件包：无法读取 zip 中央目录");
            return false;
        }
        const mz_uint file_count = mz_zip_reader_get_num_files(&zip_archive);
        if (file_count == 0) {
            mz_zip_reader_end(&zip_archive);
            error = QStringLiteral("插件包里没有任何文件");
            return false;
        }
        if (file_count > static_cast<mz_uint>(kMaxEntries)) {
            mz_zip_reader_end(&zip_archive);
            error = QStringLiteral("插件包条目数 %1 超过上限 %2").arg(file_count).arg(kMaxEntries);
            return false;
        }
        // 第一轮只校验：任一条目不合规就整包拒绝，暂存目录里不留半成品
        QList<QPair<int, QString>> entries;
        QString root;
        qint64 total_bytes = 0;
        for (mz_uint index = 0; index < file_count; ++index) {
            mz_zip_archive_file_stat stat;
            std::memset(&stat, 0, sizeof(stat));
            if (!mz_zip_reader_file_stat(&zip_archive, index, &stat)) {
                mz_zip_reader_end(&zip_archive);
                error = QStringLiteral("插件包第 %1 个条目无法读取头部信息").arg(index + 1);
                return false;
            }
            if (stat.m_is_directory) {
                continue;
            }
            const QString name = decode_entry_name(stat.m_filename);
            if (name.isEmpty()) {
                mz_zip_reader_end(&zip_archive);
                error = QStringLiteral("插件包第 %1 个条目没有名称").arg(index + 1);
                return false;
            }
            if (unsafe_entry_name(name)) {
                mz_zip_reader_end(&zip_archive);
                error = QStringLiteral("插件包里「%1」是绝对路径或含 ..，已拒绝解压").arg(name);
                return false;
            }
            const QString normalized = QString(name).replace(QLatin1Char('\\'), QLatin1Char('/'));
            const qsizetype slash = normalized.indexOf(QLatin1Char('/'));
            if (slash <= 0 || slash == normalized.size() - 1) {
                mz_zip_reader_end(&zip_archive);
                error = QStringLiteral("插件包条目「%1」不在顶层目录下，已拒绝解压").arg(name);
                return false;
            }
            const QString segment = normalized.left(slash);
            if (root.isEmpty()) {
                root = segment;
            }
            else if (root != segment) {
                mz_zip_reader_end(&zip_archive);
                error = QStringLiteral("插件包含多个顶层目录（%1 / %2），已拒绝解压").arg(root, segment);
                return false;
            }
            if (static_cast<qint64>(stat.m_uncomp_size) > kMaxEntryBytes) {
                mz_zip_reader_end(&zip_archive);
                error = QStringLiteral("「%1」解压后 %2 字节，超过单文件上限 %3 字节，已拒绝整个插件包")
                            .arg(name)
                            .arg(static_cast<qulonglong>(stat.m_uncomp_size))
                            .arg(kMaxEntryBytes);
                return false;
            }
            total_bytes += static_cast<qint64>(stat.m_uncomp_size);
            if (total_bytes > kMaxTotalBytes) {
                mz_zip_reader_end(&zip_archive);
                error = QStringLiteral("插件包解压后总量超过上限 %1 字节，已拒绝整个插件包").arg(kMaxTotalBytes);
                return false;
            }
            if (!mz_zip_reader_is_file_supported(&zip_archive, index)
                || mz_zip_reader_is_file_encrypted(&zip_archive, index)) {
                mz_zip_reader_end(&zip_archive);
                error = QStringLiteral("插件包里「%1」的压缩方式不支持或已加密").arg(name);
                return false;
            }
            entries.append(qMakePair(static_cast<int>(index), normalized));
        }
        if (root.isEmpty() || entries.isEmpty()) {
            mz_zip_reader_end(&zip_archive);
            error = QStringLiteral("插件包里没有可解压的文件");
            return false;
        }
        if (root.startsWith(QLatin1Char('.'))) {
            mz_zip_reader_end(&zip_archive);
            error = QStringLiteral("插件包顶层目录是隐藏目录（%1），已拒绝解压").arg(root);
            return false;
        }
        // 第二轮解压并落盘：全部写在暂存目录里，插件目录此时还没有任何改动
        bool has_library = false;
        QString write_error;
        for (const QPair<int, QString>& entry : entries) {
            size_t size = 0;
            void* raw = mz_zip_reader_extract_to_heap(&zip_archive, static_cast<mz_uint>(entry.first), &size, 0);
            if (raw == nullptr) {
                write_error = QStringLiteral("插件包里「%1」解压失败（条目数据损坏）").arg(entry.second);
                break;
            }
            const QString target_path = QDir(staging_root).filePath(entry.second);
            if (!QDir().mkpath(QFileInfo(target_path).absolutePath())) {
                mz_free(raw);
                write_error = QStringLiteral("无法创建插件文件目录：%1").arg(entry.second);
                break;
            }
            QFile out(target_path);
            const bool opened = out.open(QIODevice::WriteOnly | QIODevice::Truncate);
            const bool written = opened
                && out.write(static_cast<const char*>(raw), static_cast<qint64>(size)) == static_cast<qint64>(size);
            out.close();
            mz_free(raw);
            if (!written) {
                write_error = QStringLiteral("无法写入插件文件：%1").arg(entry.second);
                break;
            }
            if (!has_library && is_library_file(entry.second)) {
                has_library = true;
            }
        }
        mz_zip_reader_end(&zip_archive);
        if (!write_error.isEmpty()) {
            remove_dir_tree(staging_root);
            error = write_error;
            return false;
        }
        if (!has_library) {
            remove_dir_tree(staging_root);
            error = QStringLiteral("插件包里没有动态库文件（.dll / .so / .dylib），已拒绝安装");
            return false;
        }
        top_dir = root;
        return true;
    }

} // namespace

// 单例（public）

ModuleManager& ModuleManager::instance() {
    static ModuleManager manager;
    return manager;
}

// 构造/析构（private）

ModuleManager::ModuleManager()
    : QObject(nullptr) {
    // 定时器由主线程驱动，调度查询基于最短周期
    timer_ = new QTimer(this);
    timer_->setTimerType(Qt::PreciseTimer);
    connect(timer_, &QTimer::timeout, this, &ModuleManager::on_timer_tick);
}

ModuleManager::~ModuleManager() {
    if (timer_) {
        timer_->stop();
    }
    // 安装进行中：先等后台校验解压任务收尾，再清掉临时文件
    if (install_watcher_ != nullptr && install_watcher_->isRunning()) {
        install_watcher_->future().waitForFinished();
    }
    reset_install_state();
    // 卸载所有已加载插件（反初始化并销毁实例）
    for (auto& entry : plugins_) {
        if (entry.state == PluginLoadState::Loaded) {
            unload_plugin_entry(entry);
        }
    }
    plugins_.clear();
}

// 初始化（public）

void ModuleManager::init() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // 幂等处理：避免重复初始化
        if (initialized_) {
            return;
        }
        // 数据源由外部通过 set_data_source 提供（真实 GSI 接入前无数据，数值保持"未获取"状态）
        if (!data_source_) {
            LOG_MODULE("ModuleManager", "init", LOG_WARN,
                "未设置数据源，数值模块保持无数据状态（可通过 set_data_source 接入真实数据）");
        }
        // 以最短查询周期为基准启动调度器（模块由插件加载后注册）
        rebuild_scheduler();
        initialized_ = true;
    }
    // 扫描插件目录（config app.module.path，默认 <程序目录>/module）
    scan_plugins();
    // 扫描即加载（config app.module.scan_load，用于调试或特定场景）
    if (scan_load_) {
        LOG_MODULE("ModuleManager", "init", LOG_INFO, "扫描即加载已开启，开始加载全部插件");
        for (auto& entry : plugins_) {
            load_plugin_entry(entry);
        }
        emit plugin_state_changed();
    }
    LOG_MODULE("ModuleManager", "init", LOG_INFO,
        "数值模块初始化完成，基准周期: " << base_period_ms_ << "ms，插件候选: " << plugins_.size());
}

// 模块查询（public）

std::vector<std::string> ModuleManager::get_module_names() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> names;
    names.reserve(modules_.size());
    for (const auto& module : modules_) {
        names.push_back(module.get_name());
    }
    return names;
}

const Module* ModuleManager::get_module(const std::string& module_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& module : modules_) {
        if (module.get_name() == module_name) {
            return &module;
        }
    }
    return nullptr;
}

const ModuleValue* ModuleManager::get_value(const std::string& module_name,
    const std::string& value_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& module : modules_) {
        if (module.get_name() != module_name) {
            continue;
        }
        for (const auto& value : module.get_values()) {
            if (value.get_id() == value_id) {
                return &value;
            }
        }
    }
    return nullptr;
}

int ModuleManager::get_module_min_period_ms(const std::string& module_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& module : modules_) {
        if (module.get_name() == module_name) {
            return module.get_min_period_ms();
        }
    }
    return query_period_to_ms(QueryPeriod::SECOND);
}

std::vector<std::string> ModuleManager::get_modules_for_channel(const std::string& channel) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> names;
    for (const auto& module : modules_) {
        if (module.is_mounted_on_channel(channel)) {
            names.push_back(module.get_name());
        }
    }
    return names;
}

std::string ModuleManager::find_module_by_value_id(const std::string& value_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& module : modules_) {
        for (const auto& value : module.get_values()) {
            if (value.get_id() == value_id) {
                return module.get_name();
            }
        }
    }
    return "";
}

// 周期设置（public）

void ModuleManager::set_value_period(const std::string& module_name, const std::string& value_id,
    QueryPeriod period) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        bool found = false;
        for (auto& module : modules_) {
            if (module.get_name() != module_name) {
                continue;
            }
            for (auto& value : module.get_values()) {
                if (value.get_id() == value_id) {
                    value.set_query_period(period);
                    found = true;
                    break;
                }
            }
            break;
        }
        if (!found) {
            LOG_MODULE("ModuleManager", "set_value_period", LOG_WARN,
                "未找到数值: " << module_name << "/" << value_id);
            return;
        }
        rebuild_scheduler();
    }
    emit_period_changed();
}

void ModuleManager::set_module_period(const std::string& module_name, QueryPeriod period) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        bool found = false;
        for (auto& module : modules_) {
            if (module.get_name() == module_name) {
                module.set_all_values_period(period);
                found = true;
                break;
            }
        }
        if (!found) {
            LOG_MODULE("ModuleManager", "set_module_period", LOG_WARN,
                "未找到模块: " << module_name);
            return;
        }
        rebuild_scheduler();
    }
    emit_period_changed();
}

void ModuleManager::set_all_period(QueryPeriod period) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& module : modules_) {
            module.set_all_values_period(period);
        }
        rebuild_scheduler();
    }
    emit_period_changed();
    LOG_MODULE("ModuleManager", "set_all_period", LOG_INFO,
        "已统一设置所有数值查询周期: " << query_period_to_text(period));
}

// 数据源（public）

void ModuleManager::set_data_source(DataSource source) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_source_ = std::move(source);
}

// 查询（public）

int ModuleManager::query_value(const std::string& module_name, const std::string& value_id) {
    int new_value = 0;
    bool changed = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& module : modules_) {
            if (module.get_name() != module_name) {
                continue;
            }
            for (auto& value : module.get_values()) {
                if (value.get_id() == value_id) {
                    if (!data_source_) {
                        // 无数据源：返回已存储的值（外部写入如 GSI 数据），不触发轮询更新
                        new_value = value.get_last_value();
                        break;
                    }
                    new_value = data_source_(value_id);
                    // 钳制到配置范围后检测变化：无历史值（首次获取）或与钳制后不同均视为变化
                    new_value = value.clamp_value(new_value);
                    changed = !value.get_has_value() || value.get_last_value() != new_value;
                    value.set_last_value(new_value);
                    break;
                }
            }
            break;
        }
    }
    if (changed) {
        emit value_changed(QString::fromStdString(module_name),
            QString::fromStdString(value_id), new_value);
    }
    return new_value;
}

void ModuleManager::set_value(const std::string& module_name, const std::string& value_id,
    int value) {
    bool changed = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& module : modules_) {
            if (module.get_name() != module_name) {
                continue;
            }
            for (auto& v : module.get_values()) {
                if (v.get_id() == value_id) {
                    // 钳制到配置范围后检测变化：无历史值（首次写入）或与钳制后不同均视为变化
                    value = v.clamp_value(value);
                    changed = !v.get_has_value() || v.get_last_value() != value;
                    v.set_last_value(value);
                    break;
                }
            }
            break;
        }
    }
    if (changed) {
        emit value_changed(QString::fromStdString(module_name),
            QString::fromStdString(value_id), value);
    }
}

int ModuleManager::get_base_period_ms() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return base_period_ms_;
}

// 插件管理（public）

std::string ModuleManager::get_plugin_dir() const {
    return plugin_dir_;
}

bool ModuleManager::get_scan_load() const {
    return scan_load_;
}

std::vector<ModuleManager::PluginInfo> ModuleManager::get_plugins() const {
    // plugins_ 仅主线程访问（与模块数据锁分离）
    std::vector<PluginInfo> infos;
    infos.reserve(plugins_.size());
    for (const auto& entry : plugins_) {
        PluginInfo info;
        info.file_name = entry.file_name;
        info.file_path = entry.file_path;
        // 展示名称优先级：侧车 name -> 已加载插件的 name() -> 动态库主干名
        if (!entry.meta.display_name.empty()) {
            info.display_name = entry.meta.display_name;
        } else if (entry.state == PluginLoadState::Loaded) {
            info.display_name = entry.display_name;
        } else {
            const QString stem = QFileInfo(QString::fromStdString(entry.file_name)).completeBaseName();
            info.display_name = stem.toStdString();
        }
        info.version = entry.version;
        info.state = entry.state;
        info.error = entry.error;
        info.banner = entry.meta.banner;
        info.author = entry.meta.author;
        info.description = entry.meta.description;
        info.tags = entry.meta.tags;
        // 模块标识仅加载后有效（load_plugin_entry 已用 plugin->name() 赋值），未加载为空
        info.module_name = (entry.state == PluginLoadState::Loaded) ? entry.display_name : std::string();
        info.events = entry.meta.events;
        infos.push_back(std::move(info));
    }
    return infos;
}

bool ModuleManager::load_plugin(const std::string& file_name) {
    PluginEntry* entry = find_plugin_entry(file_name);
    if (!entry) {
        LOG_MODULE("ModuleManager", "load_plugin", LOG_WARN,
            "插件不存在: " << file_name);
        return false;
    }
    bool ok = load_plugin_entry(*entry);
    emit plugin_state_changed();
    return ok;
}

bool ModuleManager::unload_plugin(const std::string& file_name) {
    PluginEntry* entry = find_plugin_entry(file_name);
    if (!entry) {
        LOG_MODULE("ModuleManager", "unload_plugin", LOG_WARN,
            "插件不存在: " << file_name);
        return false;
    }
    unload_plugin_entry(*entry);
    emit plugin_state_changed();
    // 卸载成功（entry 已回到 NotLoaded）返回 true；插件拒绝卸载时保持 Loaded 返回 false
    return entry->state != PluginLoadState::Loaded;
}

bool ModuleManager::is_plugin_loaded(const std::string& file_name) const {
    const PluginEntry* entry = find_plugin_entry(file_name);
    return entry && entry->state == PluginLoadState::Loaded;
}

void ModuleManager::download_plugin(const std::string& url, const std::string& sha256) {
    const QString url_text = QString::fromStdString(url).trimmed();
    const QString expected_sha256 = QString::fromStdString(sha256).trimmed().toLower();
    // 只接受 http(s)：file:// 之类会把「下载」变成读本机任意路径
    const QUrl target_url(url_text);
    const bool scheme_ok = target_url.isValid()
        && (target_url.scheme() == QStringLiteral("http") || target_url.scheme() == QStringLiteral("https"));
    if (!scheme_ok) {
        finish_install(QString(), false, QStringLiteral("插件包地址无效：只接受 http / https 链接"));
        return;
    }
    if (!is_sha256_text(expected_sha256)) {
        finish_install(QString(), false, QStringLiteral("插件包校验值无效：需要小写十六进制 64 位 SHA256"));
        return;
    }
    if (install_busy_ || install_reply_ != nullptr) {
        finish_install(QString(), false, QStringLiteral("已有插件包正在安装，请稍候再试"));
        return;
    }
    reset_install_state();
    install_expected_sha256_ = expected_sha256;
    install_busy_ = true;
    install_error_.clear();
    // 临时文件落在系统临时目录：文件名带进程号与时间戳，多开实例也不会互相覆盖
    install_temp_path_ = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
        .filePath(QStringLiteral("dglab_plugin_%1_%2.zip")
            .arg(QCoreApplication::applicationPid())
            .arg(QDateTime::currentMSecsSinceEpoch()));
    install_file_ = new QFile(install_temp_path_, this);
    if (!install_file_->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        install_error_ = QStringLiteral("无法写入插件包临时文件: ") + install_file_->errorString();
        install_file_->deleteLater();
        install_file_ = nullptr;
        finish_install(QString(), false, install_error_);
        return;
    }
    if (install_network_ == nullptr) {
        install_network_ = new QNetworkAccessManager(this);
    }
    QNetworkRequest request(target_url);
    // GitHub Releases 会 302 到对象存储，重定向策略必须放开
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    request.setTransferTimeout(60'000);
#endif
    install_reply_ = install_network_->get(request);
    QObject::connect(install_reply_, &QNetworkReply::readyRead, this, &ModuleManager::on_install_ready_read);
    // 下载阶段还不知道顶层目录名，进度里的 target 先留空，由调用方按当前安装项关联
    QObject::connect(install_reply_, &QNetworkReply::downloadProgress, this,
        [this](qint64 received, qint64 total) {
            emit plugin_install_progress(QString(), received, total);
        });
    QObject::connect(install_reply_, &QNetworkReply::finished, this,
        &ModuleManager::on_install_download_finished);
    LOG_MODULE("ModuleManager", "download_plugin", LOG_INFO, "开始下载插件包: " << url);
}

// 宿主能力（public，IPluginHost 实现）

bool ModuleManager::register_module_values(const std::string& module_name,
    const std::vector<ModuleValue>& values,
    const std::vector<std::string>& channels) {
    if (module_name.empty() || values.empty()) {
        return false;
    }
    bool rebuild = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // 已存在同名模块：合并数值（按 ID 去重，不覆盖已有数值）
        auto it = std::find_if(modules_.begin(), modules_.end(),
            [&](const Module& m) { return m.get_name() == module_name; });
        if (it != modules_.end()) {
            for (const auto& value : values) {
                bool exists = false;
                for (const auto& v : it->get_values()) {
                    if (v.get_id() == value.get_id()) {
                        exists = true;
                        break;
                    }
                }
                if (!exists) {
                    it->add_value(value);
                    rebuild = true;
                }
            }
            // 合并通道（挂载）
            for (const auto& channel : channels) {
                it->mount_channel(channel);
            }
        }
        else {
            Module module(module_name, channels);
            for (const auto& value : values) {
                module.add_value(value);
            }
            modules_.push_back(std::move(module));
            rebuild = true;
        }
        if (rebuild) {
            rebuild_scheduler();
        }
    }
    if (rebuild) {
        emit_period_changed();
    }
    LOG_MODULE("ModuleManager", "register_module_values", LOG_INFO,
        "插件注册数值完成: " << module_name << "，数值数量: " << values.size());
    return true;
}

void ModuleManager::unregister_module(const std::string& module_name) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = std::remove_if(modules_.begin(), modules_.end(),
            [&](const Module& m) { return m.get_name() == module_name; });
        if (it != modules_.end()) {
            modules_.erase(it, modules_.end());
            rebuild_scheduler();
        }
        else {
            return;
        }
    }
    // 先广播注销（事件层据此刻丢弃该模块的事件声明与运行期状态），再刷新周期显示
    emit module_unregistered(QString::fromStdString(module_name));
    emit_period_changed();
    LOG_MODULE("ModuleManager", "unregister_module", LOG_INFO,
        "已注销模块: " << module_name);
}

bool ModuleManager::listen_data(int port) {
    // 宿主共享数据接收器（懒创建），HTTP 协议监听
    if (!shared_listener_) {
        shared_listener_ = new DataListener(this);
        LOG_MODULE("ModuleManager", "listen_data", LOG_DEBUG,
            "创建宿主共享数据接收器");
    }
    return shared_listener_->start_listening(port);
}

void ModuleManager::stop_listening_data() {
    if (shared_listener_) {
        shared_listener_->stop_listening();
    }
}

bool ModuleManager::register_data_handler(const std::string& source, const std::string& type,
    const std::function<void(const QJsonObject&)>& handler) {
    if (!shared_listener_) {
        shared_listener_ = new DataListener(this);
    }
    // 无信封数据（如 CS2 GSI）回退默认来源：首次注册的来源+类型作为默认值
    if (shared_listener_->default_source().isEmpty() && shared_listener_->default_type().isEmpty()) {
        shared_listener_->set_default_source(QString::fromStdString(source));
        shared_listener_->set_default_type(QString::fromStdString(type));
    }
    shared_listener_->register_handler(QString::fromStdString(source),
        QString::fromStdString(type), handler);
    return true;
}

void ModuleManager::unregister_data_handler(const std::string& source,
    const std::string& type) {
    if (shared_listener_) {
        shared_listener_->unregister_handler(QString::fromStdString(source),
            QString::fromStdString(type));
    }
}

std::string ModuleManager::get_config_value(const std::string& key,
    const std::string& default_value) {
    return AppConfig::instance().get_value<std::string>(key, default_value);
}

void ModuleManager::set_config_value(const std::string& key, const std::string& value) {
    AppConfig::instance().set_value_with_name<std::string>(key, value, "user");
}

int ModuleManager::base_period_ms() {
    std::lock_guard<std::mutex> lock(mutex_);
    return base_period_ms_;
}

void ModuleManager::notify(const std::string& title, const std::string& message) {
    emit plugin_notification(QString::fromStdString(title), QString::fromStdString(message));
}

// private slots 实现

void ModuleManager::on_timer_tick() {
    ++tick_count_;
    // 收集本周期内发生变化的数值（先查后发，避免持锁发信号）
    std::vector<std::tuple<std::string, std::string, int>> changes;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& module : modules_) {
            for (auto& value : module.get_values()) {
                int period_ms = query_period_to_ms(value.get_query_period());
                int interval = period_ms / base_period_ms_;
                if (interval < 1) {
                    interval = 1;
                }
                // 按基准周期取模：周期越短的数值被查询的次数越多
                if (tick_count_ % interval != 0) {
                    continue;
                }
                if (query_value_locked(module, value)) {
                    changes.emplace_back(module.get_name(), value.get_id(),
                        value.get_last_value());
                }
            }
        }
    }
    // 数值变化时推送（触发规则计算与界面刷新）
    for (const auto& [module_name, value_id, new_value] : changes) {
        emit value_changed(QString::fromStdString(module_name),
            QString::fromStdString(value_id), new_value);
    }
}

void ModuleManager::on_install_ready_read() {
    if (install_reply_ == nullptr) {
        return;
    }
    if (install_file_ == nullptr || !install_file_->isOpen()) {
        install_write_error_ = true;
        install_error_ = QStringLiteral("无法写入插件包临时文件: 文件句柄不可用");
        install_reply_->abort();
        return;
    }
    const QByteArray chunk = install_reply_->readAll();
    if (install_file_->size() + chunk.size() > kMaxDownloadBytes) {
        install_write_error_ = true;
        install_error_ = QStringLiteral("插件包超过压缩后上限 %1 MiB，已中止下载")
                             .arg(kMaxDownloadBytes / (1024 * 1024));
        install_reply_->abort();
        return;
    }
    if (install_file_->write(chunk) != static_cast<qint64>(chunk.size())) {
        install_write_error_ = true;
        install_error_ = QStringLiteral("无法写入插件包临时文件: ") + install_file_->errorString();
        install_reply_->abort();
    }
}

void ModuleManager::on_install_download_finished() {
    QNetworkReply* reply = install_reply_;
    install_reply_ = nullptr;
    if (reply == nullptr) {
        return;
    }
    reply->deleteLater();
    // 尾部残余数据写进同一句柄后再统一落盘：校验读到的是磁盘上的完整文件
    if (!install_write_error_ && reply->error() == QNetworkReply::NoError
        && install_file_ != nullptr && install_file_->isOpen()) {
        const QByteArray tail = reply->readAll();
        if (!tail.isEmpty() && install_file_->write(tail) != static_cast<qint64>(tail.size())) {
            install_write_error_ = true;
            install_error_ = QStringLiteral("无法写入插件包临时文件: ") + install_file_->errorString();
        }
    }
    if (install_file_ != nullptr) {
        if (!install_file_->flush()) {
            install_write_error_ = true;
            install_error_ = QStringLiteral("无法写入插件包临时文件: 刷新到磁盘失败");
        }
        install_file_->close();
        install_file_->deleteLater();
        install_file_ = nullptr;
    }
    if (install_write_error_) {
        finish_install(QString(), false, install_error_);
        return;
    }
    if (reply->error() != QNetworkReply::NoError) {
        finish_install(QString(), false, QStringLiteral("插件包下载失败: ") + reply->errorString());
        return;
    }
    // 校验、解压、依赖校验与目录替换都在后台线程完成：这些是文件与 CPU 操作，不占界面线程
    const QString temp_path = install_temp_path_;
    const QString expected_sha256 = install_expected_sha256_;
    const QString plugin_dir = plugin_dir_.empty()
        ? QString::fromStdString(resolve_plugin_dir())
        : QString::fromStdString(plugin_dir_);
    if (install_watcher_ == nullptr) {
        install_watcher_ = new QFutureWatcher<InstallOutcome>(this);
        QObject::connect(install_watcher_, &QFutureWatcher<InstallOutcome>::finished, this,
            &ModuleManager::on_install_task_finished);
    }
    install_watcher_->setFuture(QtConcurrent::run([temp_path, expected_sha256, plugin_dir]() {
        InstallOutcome outcome;
        QFile archive_file(temp_path);
        if (!archive_file.open(QIODevice::ReadOnly)) {
            outcome.error = QStringLiteral("无法读取下载的插件包");
            return outcome;
        }
        const QByteArray archive = archive_file.readAll();
        archive_file.close();
        QFile::remove(temp_path);
        if (archive.isEmpty()) {
            outcome.error = QStringLiteral("下载的插件包为空");
            return outcome;
        }
        // 摘要不符时插件目录还没有任何改动，直接丢弃下载内容
        const QString actual_sha256 = QString::fromLatin1(
            QCryptographicHash::hash(archive, QCryptographicHash::Sha256).toHex());
        if (actual_sha256 != expected_sha256) {
            outcome.error = QStringLiteral("插件包校验失败（SHA256 不匹配），已丢弃下载内容");
            return outcome;
        }
        const QString staging_root = QDir(plugin_dir).filePath(QString::fromLatin1(kStagingDirName));
        // 暂存目录每次安装前清空，避免上一轮半成品混进本次校验
        if (!remove_dir_tree(staging_root)) {
            outcome.error = QStringLiteral("无法清理插件暂存目录: ") + staging_root;
            return outcome;
        }
        QString top_dir;
        QString extract_error;
        if (!extract_plugin_archive(archive, staging_root, top_dir, extract_error)) {
            outcome.error = extract_error;
            return outcome;
        }
        outcome.target = top_dir;
        const QString staging_top = QDir(staging_root).filePath(top_dir);
        // 依赖校验放在替换之前：缺依赖时插件目录保持原状
        QString missing;
        if (!dependencies_satisfied(staging_top, plugin_dir, missing)) {
            remove_dir_tree(staging_root);
            outcome.error = QStringLiteral("缺少依赖插件 %1").arg(missing);
            return outcome;
        }
        QString replace_error;
        if (!replace_plugin_dir(staging_top, QDir(plugin_dir).filePath(top_dir),
                QDir(plugin_dir).filePath(QString::fromLatin1(kBackupDirName)), replace_error)) {
            remove_dir_tree(staging_root);
            outcome.error = replace_error;
            return outcome;
        }
        remove_dir_tree(staging_root);
        outcome.ok = true;
        return outcome;
    }));
}

void ModuleManager::on_install_task_finished() {
    if (install_watcher_ == nullptr) {
        return;
    }
    const InstallOutcome outcome = install_watcher_->result();
    if (!outcome.ok) {
        LOG_MODULE("ModuleManager", "on_install_task_finished", LOG_WARN,
            "插件包安装失败: " << outcome.error.toStdString());
        finish_install(outcome.target, false, outcome.error);
        return;
    }
    // 重新扫描：新装的插件按插件目录里的实际内容重新登记为候选，插件中心与事件层一起刷新
    scan_plugins();
    emit plugin_state_changed();
    LOG_MODULE("ModuleManager", "on_install_task_finished", LOG_INFO,
        "插件包安装完成: " << outcome.target.toStdString());
    finish_install(outcome.target, true, QString());
}

// 私有辅助函数实现（private）

void ModuleManager::rebuild_scheduler() {
    base_period_ms_ = query_period_to_ms(QueryPeriod::SECOND);
    for (const auto& module : modules_) {
        base_period_ms_ = std::min(base_period_ms_, module.get_min_period_ms());
    }
    tick_count_ = 0;
    if (timer_) {
        timer_->start(base_period_ms_);
    }
    LOG_MODULE("ModuleManager", "rebuild_scheduler", LOG_DEBUG,
        "调度器已重建，基准周期: " << base_period_ms_ << "ms");
}

bool ModuleManager::query_value_locked(Module& module, ModuleValue& value) {
    if (!data_source_) {
        // 无数据源：保持"未获取"状态，不产生变化
        return false;
    }
    int new_value = data_source_(value.get_id());
    // 钳制到配置范围后检测变化：无历史值（首次获取）或与钳制后不同均返回 true（触发推送）
    new_value = value.clamp_value(new_value);
    bool changed = !value.get_has_value() || value.get_last_value() != new_value;
    value.set_last_value(new_value);
    return changed;
}

void ModuleManager::emit_period_changed() {
    // 周期变化通知：先通知已加载插件（如 GSI 插件据此更新配置文件 throttle），再发出信号
    for (const auto& entry : plugins_) {
        if (entry.state == PluginLoadState::Loaded && entry.instance) {
            entry.instance->on_host_period_changed();
        }
    }
    emit period_changed();
}

// 插件扫描与加载（private）

std::string ModuleManager::resolve_plugin_dir() const {
    auto& config = AppConfig::instance();
    std::string configured = config.get_value<std::string>("app.module.path", "./module");
    QDir dir(QString::fromStdString(configured));
    if (dir.isRelative()) {
        // 相对路径相对于程序目录解析（与 config/python 复制行为一致）
        dir = QDir(QCoreApplication::applicationDirPath() + "/" + QString::fromStdString(configured));
    }
    return QDir::cleanPath(dir.absolutePath()).toStdString();
}

void ModuleManager::scan_plugins() {
    plugin_dir_ = resolve_plugin_dir();
    scan_load_ = AppConfig::instance().get_value<bool>("app.module.scan_load", false);
    plugins_.clear();
    QDir dir(QString::fromStdString(plugin_dir_));
    if (!dir.exists()) {
        LOG_MODULE("ModuleManager", "scan_plugins", LOG_WARN,
            "插件目录不存在，跳过扫描: " << plugin_dir_);
        return;
    }
    // 动态库文件即插件候选（*.dll / *.so / *.dylib）：先扫插件目录顶层（随程序部署的内置插件在这里），
    // 再扫一层子目录（插件市场安装的包解压为 <插件目录>/<顶层目录>/<动态库>）
    const QStringList filters = {"*.dll", "*.so", "*.dylib"};
    QList<QPair<QString, QString>> candidates;
    const QStringList top_files = dir.entryList(filters, QDir::Files, QDir::Name);
    for (const QString& file : top_files) {
        candidates.append(qMakePair(file, dir.filePath(file)));
    }
    const QStringList subdirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& subdir : subdirs) {
        // .staging / .backup 是安装过程的临时目录，不参与扫描
        if (subdir.startsWith(QLatin1Char('.'))) {
            continue;
        }
        const QDir sub_dir(dir.filePath(subdir));
        const QStringList files = sub_dir.entryList(filters, QDir::Files, QDir::Name);
        for (const QString& file : files) {
            bool duplicated = false;
            for (const auto& candidate : candidates) {
                if (candidate.first == file) {
                    duplicated = true;
                    break;
                }
            }
            // 同名动态库以插件目录顶层为准，避免侧车目录解析到另一份
            if (duplicated) {
                continue;
            }
            candidates.append(qMakePair(file, sub_dir.filePath(file)));
        }
    }
    int missing_meta = 0;
    for (const auto& candidate : candidates) {
        PluginEntry entry;
        entry.file_name = candidate.first.toStdString();
        entry.file_path = candidate.second.toStdString();
        // 侧车目录名与动态库主干名一致，按主干名定位 <插件目录>/<主干名>/plugin.json
        const QString stem = QFileInfo(candidate.first).completeBaseName();
        entry.meta = read_plugin_meta(plugin_dir_, stem.toStdString());
        if (!entry.meta.found) {
            ++missing_meta;
        }
        plugins_.push_back(std::move(entry));
    }
    // 侧车缺失或解析失败只作单次调试记录，避免逐个插件刷屏
    if (missing_meta > 0) {
        LOG_MODULE("ModuleManager", "scan_plugins", LOG_DEBUG,
            "未读取到展示元数据侧车的插件数: " << missing_meta);
    }
    LOG_MODULE("ModuleManager", "scan_plugins", LOG_INFO,
        "插件目录扫描完成: " << plugin_dir_ << "，候选: " << plugins_.size());
}

bool ModuleManager::load_plugin_entry(PluginEntry& entry) {
    // 已加载直接返回成功
    if (entry.state == PluginLoadState::Loaded) {
        return true;
    }
    // 上次加载失败：重置后重试
    entry.error.clear();
    LOG_MODULE("ModuleManager", "load_plugin_entry", LOG_INFO,
        "开始加载插件: " << entry.file_name);

    // 动态加载动态库
    QLibrary* library = new QLibrary(QString::fromStdString(entry.file_path), this);
    if (!library->load()) {
        entry.state = PluginLoadState::Failed;
        entry.error = library->errorString().toStdString();
        LOG_MODULE("ModuleManager", "load_plugin_entry", LOG_ERROR,
            "插件加载失败: " << entry.file_name << "，原因: " << entry.error);
        delete library;
        return false;
    }
    // 版本校验：get_plugin_api_version 必须等于 PLUGIN_API_VERSION
    auto api_version_fn = reinterpret_cast<PluginApiVersionFn>(library->resolve("get_plugin_api_version"));
    if (!api_version_fn) {
        entry.state = PluginLoadState::Failed;
        entry.error = "缺少 get_plugin_api_version 导出，非插件动态库";
        LOG_MODULE("ModuleManager", "load_plugin_entry", LOG_WARN, entry.error << ": " << entry.file_name);
        library->unload();
        delete library;
        return false;
    }
    if (api_version_fn() != PLUGIN_API_VERSION) {
        entry.state = PluginLoadState::Failed;
        entry.error = "插件 API 版本不匹配（期望 " + std::to_string(PLUGIN_API_VERSION) + "，实际 " + std::to_string(api_version_fn()) + "）";
        LOG_MODULE("ModuleManager", "load_plugin_entry", LOG_ERROR,
            entry.error << ": " << entry.file_name);
        library->unload();
        delete library;
        return false;
    }
    // 解析工厂函数
    auto create_fn = reinterpret_cast<PluginCreateFn>(library->resolve("create_plugin"));
    auto destroy_fn = reinterpret_cast<PluginDestroyFn>(library->resolve("destroy_plugin"));
    if (!create_fn || !destroy_fn) {
        entry.state = PluginLoadState::Failed;
        entry.error = "缺少 create_plugin / destroy_plugin 导出";
        LOG_MODULE("ModuleManager", "load_plugin_entry", LOG_WARN, entry.error << ": " << entry.file_name);
        library->unload();
        delete library;
        return false;
    }
    // 创建插件实例（内存隔离：插件内部 new，宿主仅通过 destroy_plugin 销毁）
    IPlugin* plugin = create_fn();
    plugin->attach_host(this);
    // 日志转发：插件回调 -> 宿主统一 LOG_MODULE（类名为插件名称、方法名为函数名）
    plugin->set_log_callback([](PluginLogLevel level, const std::string& plugin_name,
                                 const std::string& function, const std::string& message) {
        LogLevel host_level = LOG_DEBUG;
        switch (level) {
        case PluginLogLevel::Debug: host_level = LOG_DEBUG; break;
        case PluginLogLevel::Info: host_level = LOG_INFO; break;
        case PluginLogLevel::Warn: host_level = LOG_WARN; break;
        case PluginLogLevel::Error: host_level = LOG_ERROR; break;
        case PluginLogLevel::None: host_level = LOG_NONE; break;
        }
        LOG_MODULE(plugin_name, function, host_level, message);
    });
    // 依赖校验：依赖的插件必须已加载
    bool dependency_missing = false;
    std::string missing_dependency;
    for (const auto& dep : plugin->dependencies()) {
        bool found = false;
        for (const auto& other : plugins_) {
            if (other.state == PluginLoadState::Loaded && other.instance && other.instance->name() == dep) {
                found = true;
                break;
            }
        }
        if (!found) {
            dependency_missing = true;
            missing_dependency = dep;
            break;
        }
    }
    if (dependency_missing) {
        entry.state = PluginLoadState::Failed;
        entry.error = "依赖插件未加载: " + missing_dependency;
        LOG_MODULE("ModuleManager", "load_plugin_entry", LOG_ERROR,
            entry.error << ": " << entry.file_name);
        destroy_fn(plugin);
        library->unload();
        delete library;
        return false;
    }
    // 初始化（插件在此通过 host_ 注册数值与数据处理器）
    PluginError init_error = plugin->initialize();
    if (init_error != PluginError::Ok) {
        entry.state = PluginLoadState::Failed;
        entry.error = "初始化失败，错误码: " + std::to_string(static_cast<int>(init_error));
        LOG_MODULE("ModuleManager", "load_plugin_entry", LOG_ERROR,
            entry.error << ": " << entry.file_name);
        plugin->uninitialize();
        destroy_fn(plugin);
        library->unload();
        delete library;
        return false;
    }
    // 加载成功
    entry.library = library;
    entry.instance = plugin;
    entry.state = PluginLoadState::Loaded;
    entry.display_name = plugin->name();
    entry.version = plugin->version();
    LOG_MODULE("ModuleManager", "load_plugin_entry", LOG_INFO,
        "插件加载成功: " << entry.display_name << " v" << entry.version
                         << "（" << entry.file_name << "）");
    return true;
}

void ModuleManager::unload_plugin_entry(PluginEntry& entry) {
    if (entry.state != PluginLoadState::Loaded || !entry.instance) {
        return;
    }
    IPlugin* plugin = entry.instance;
    QLibrary* library = entry.library;
    // 卸载前检查：插件拒绝卸载时保持已加载状态
    if (!plugin->can_unload()) {
        entry.error = "插件拒绝卸载（can_unload 返回 false）";
        LOG_MODULE("ModuleManager", "unload_plugin_entry", LOG_WARN,
            entry.error << ": " << entry.file_name);
        return;
    }
    // 反初始化（插件在此通过 host_ 注销数值）与资源清理
    plugin->uninitialize();
    plugin->cleanup();
    // 兜底：确保插件模块已从模块列表移除
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = std::remove_if(modules_.begin(), modules_.end(),
            [&](const Module& m) { return m.get_name() == plugin->name(); });
        if (it != modules_.end()) {
            modules_.erase(it, modules_.end());
            rebuild_scheduler();
        }
    }
    // 销毁实例（插件内部 delete）并卸载动态库
    auto destroy_fn = reinterpret_cast<PluginDestroyFn>(library->resolve("destroy_plugin"));
    if (destroy_fn) {
        destroy_fn(plugin);
    }
    library->unload();
    library->deleteLater();
    // 重置条目为未加载状态
    entry.library = nullptr;
    entry.instance = nullptr;
    entry.state = PluginLoadState::NotLoaded;
    entry.display_name.clear();
    entry.version.clear();
    entry.error.clear();
    LOG_MODULE("ModuleManager", "unload_plugin_entry", LOG_INFO,
        "插件已卸载: " << entry.file_name);
}

ModuleManager::PluginEntry* ModuleManager::find_plugin_entry(const std::string& file_name) {
    for (auto& entry : plugins_) {
        if (entry.file_name == file_name) {
            return &entry;
        }
    }
    return nullptr;
}

const ModuleManager::PluginEntry* ModuleManager::find_plugin_entry(const std::string& file_name) const {
    for (const auto& entry : plugins_) {
        if (entry.file_name == file_name) {
            return &entry;
        }
    }
    return nullptr;
}

void ModuleManager::reset_install_state() {
    if (install_reply_ != nullptr) {
        QObject::disconnect(install_reply_, nullptr, this, nullptr);
        install_reply_->abort();
        install_reply_->deleteLater();
        install_reply_ = nullptr;
    }
    if (install_file_ != nullptr) {
        install_file_->close();
        install_file_->deleteLater();
        install_file_ = nullptr;
    }
    if (!install_temp_path_.isEmpty()) {
        QFile::remove(install_temp_path_);
        install_temp_path_.clear();
    }
    install_write_error_ = false;
    install_error_.clear();
    install_expected_sha256_.clear();
}

void ModuleManager::finish_install(const QString& target, bool ok, const QString& error) {
    install_busy_ = false;
    // 失败路径也清掉下载的临时文件，系统临时目录里不留半成品
    if (!install_temp_path_.isEmpty()) {
        QFile::remove(install_temp_path_);
        install_temp_path_.clear();
    }
    emit plugin_install_finished(target, ok, error);
}
