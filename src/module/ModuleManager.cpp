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
#include <QDir>
#include <QLibrary>

#include <algorithm>
#include <tuple>
#include <utility>

// 导出函数指针类型（extern "C" 约定）
using PluginApiVersionFn = int (*)();
using PluginCreateFn = IPlugin* (*)();
using PluginDestroyFn = void (*)(IPlugin*);

// ============================================
// 单例（public）
// ============================================

ModuleManager& ModuleManager::instance() {
    static ModuleManager manager;
    return manager;
}

// ============================================
// 构造/析构（private）
// ============================================

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
    // 卸载所有已加载插件（反初始化并销毁实例）
    for (auto& entry : plugins_) {
        if (entry.state == PluginLoadState::Loaded) {
            unload_plugin_entry(entry);
        }
    }
    plugins_.clear();
}

// ============================================
// 初始化（public）
// ============================================

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

// ============================================
// 模块查询（public）
// ============================================

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

// ============================================
// 周期设置（public）
// ============================================

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

// ============================================
// 数据源（public）
// ============================================

void ModuleManager::set_data_source(DataSource source) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_source_ = std::move(source);
}

// ============================================
// 查询（public）
// ============================================

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

// ============================================
// 插件管理（public）
// ============================================

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
        info.display_name = (entry.state == PluginLoadState::Loaded) ? entry.display_name
                                                                     : entry.file_name;
        info.version = entry.version;
        info.state = entry.state;
        info.error = entry.error;
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

// ============================================
// 宿主能力（public，IPluginHost 实现）
// ============================================

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

// ============================================
// private slots 实现
// ============================================

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

// ============================================
// 私有辅助函数实现（private）
// ============================================

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

// -------------------- 插件扫描与加载（private） --------------------

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
    // 动态库文件即插件候选（*.dll / *.so / *.dylib）
    const QStringList filters = {"*.dll", "*.so", "*.dylib"};
    const QStringList files = dir.entryList(filters, QDir::Files, QDir::Name);
    for (const QString& file : files) {
        PluginEntry entry;
        entry.file_name = file.toStdString();
        entry.file_path = dir.filePath(file).toStdString();
        plugins_.push_back(std::move(entry));
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
