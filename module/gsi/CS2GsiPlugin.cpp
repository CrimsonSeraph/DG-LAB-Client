/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "CS2GsiPlugin.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QRandomGenerator>
#include <QRegularExpression>

#include <cstdlib>
#include <map>

// ============================================
// 数值定义（public）
// ============================================

std::vector<ModuleValue> CS2GsiPlugin::create_default_values() {
    // 完整数值列表（参照官方 GSI 规范）：个人状态类数值队友数据不更新，团队/地图类始终更新
    std::vector<ModuleValue> values;
    // 个人状态类（最值参照官方 GSI 取值，写入时自动钳制到范围）
    values.emplace_back("health", "当前血量", QueryPeriod::QUARTER_SECOND, "m_iHealth", 0, 100);
    values.emplace_back("armor", "当前护甲", QueryPeriod::HALF_SECOND, "m_ArmorValue", 0, 100);
    values.emplace_back("money", "金钱", QueryPeriod::TWO_SECONDS, "m_iMoney", 0, 16000);
    values.emplace_back("team_num", "队伍编号", QueryPeriod::SECOND, "team", 2, 3);
    values.emplace_back("has_helmet", "是否有头盔", QueryPeriod::FOUR_SECONDS, "m_bHasHelmet", 0, 1);
    values.emplace_back("has_defuser", "是否有拆弹器", QueryPeriod::FOUR_SECONDS, "m_bHasDefuser", 0, 1);
    values.emplace_back("flashed", "闪光致盲程度", QueryPeriod::SECOND, "flashed", 0, 255);
    values.emplace_back("smoked", "烟雾遮蔽程度", QueryPeriod::SECOND, "smoked", 0, 255);
    values.emplace_back("burning", "燃烧灼烧程度", QueryPeriod::SECOND, "burning", 0, 255);
    values.emplace_back("round_kills", "当前回合击杀数", QueryPeriod::SECOND, "round_kills", 0, 255);
    values.emplace_back("round_killhs", "当前回合爆头击杀数", QueryPeriod::SECOND, "round_killhs", 0, 255);
    values.emplace_back("round_totaldmg", "当前回合总伤害", QueryPeriod::SECOND, "round_totaldmg", 0, 10000);
    values.emplace_back("equip_value", "装备总价值", QueryPeriod::SECOND, "equip_value", 0, 65535);
    values.emplace_back("kills", "总击杀数", QueryPeriod::SECOND, "kills", 0, 99999);
    values.emplace_back("assists", "总助攻数", QueryPeriod::SECOND, "assists", 0, 99999);
    values.emplace_back("deaths", "总死亡数", QueryPeriod::SECOND, "deaths", 0, 99999);
    values.emplace_back("mvps", "MVP 次数", QueryPeriod::SECOND, "mvps", 0, 99999);
    // 团队/地图类
    values.emplace_back("ct_score", "CT 队伍得分", QueryPeriod::SECOND, "map.team_ct.score", 0, 999);
    values.emplace_back("t_score", "T 队伍得分", QueryPeriod::SECOND, "map.team_t.score", 0, 999);
    values.emplace_back("ct_consecutive_round_losses", "CT 连续失利次数", QueryPeriod::SECOND,
        "map.team_ct.consecutive_round_losses", 0, 99);
    values.emplace_back("t_consecutive_round_losses", "T 连续失利次数", QueryPeriod::SECOND,
        "map.team_t.consecutive_round_losses", 0, 99);
    values.emplace_back("bomb_state", "炸弹状态", QueryPeriod::SECOND, "bomb.state", 0, 4);
    values.emplace_back("map_phase", "地图阶段", QueryPeriod::SECOND, "map.phase", 0, 6);
    return values;
}

// ============================================
// 生命周期（public）
// ============================================

PluginError CS2GsiPlugin::initialize() {
    if (initialized_) {
        return PluginError::AlreadyInitialized;
    }
    if (!host_) {
        return PluginError::NotInitialized;
    }
    // 1) 注册完整数值列表到模块管理器（模块名 = 插件名，挂载到 A/B 通道）
    host_->register_module_values(name(), create_default_values(), {"A", "B"});
    // 2) 加载已记录的配置路径/目录/端口（user 配置 app.gsi 下）
    load_config_path();
    // 3) 首次运行（无记录）或目录失效时：查找 CS 目录并生成配置文件
    if (config_path_.empty() || !QFile::exists(QString::fromStdString(config_path_))) {
        QString cs_dir = find_cs_directory();
        if (cs_dir.isEmpty()) {
            PLUGIN_LOG(this, PluginLogLevel::Warn, "未找到 CS 游戏目录，暂不生成 GSI 配置文件");
        }
        else {
            cs_dir_ = cs_dir.toStdString();
            generate_config();
        }
    }
    last_min_period_ms_ = host_->base_period_ms();
    // 4) 注册 GSI 数据处理器（来源 CS2 GSI / 类型 gsi）
    host_->register_data_handler(name(), "gsi",
        [this](const QJsonObject& data) { on_gsi_data(data); });
    // 5) 启动 GSI 端口监听（接收 CS2 游戏数据）
    start_gsi_listener();
    initialized_ = true;
    PLUGIN_LOG(this, PluginLogLevel::Info, "CS2 GSI 插件初始化完成，配置文件: " << config_path_);
    return PluginError::Ok;
}

void CS2GsiPlugin::uninitialize() {
    if (!initialized_) {
        return;
    }
    // 注销数据处理器、停止监听并注销插件模块（与 initialize 对称）
    if (host_) {
        host_->unregister_data_handler(name(), "gsi");
        host_->stop_listening_data();
        host_->unregister_module(name());
    }
    initialized_ = false;
    PLUGIN_LOG(this, PluginLogLevel::Info, "CS2 GSI 插件已反初始化");
}

void CS2GsiPlugin::on_host_period_changed() {
    int new_min_period = host_ ? host_->base_period_ms() : 1000;
    if (new_min_period == last_min_period_ms_) {
        return;
    }
    last_min_period_ms_ = new_min_period;
    PLUGIN_LOG(this, PluginLogLevel::Debug, "最小查询周期变化: " << new_min_period << "ms");
    // 已有配置文件时重新生成（throttle 随周期变化）
    if (config_path_.empty()) {
        return;
    }
    QString new_path = generate_config();
    if (new_path.isEmpty()) {
        return;
    }
    // 配置仅当游戏启动时加载、不支持热重载：游戏运行中需提示重启
    if (is_process_running("cs2.exe")) {
        PLUGIN_LOG(this, PluginLogLevel::Info, "CS2 正在运行，配置已更新，需重启游戏生效");
        if (host_) {
            host_->notify("需要重启游戏",
                "已更新 GSI 配置文件（最小查询周期变化，throttle 已调整）:\n" + new_path.toStdString() + "\n\nGSI 配置仅在游戏启动时加载，请重启 CS2 游戏使新配置生效。");
        }
    }
    else {
        PLUGIN_LOG(this, PluginLogLevel::Info, "CS2 未运行，配置已更新（下次启动游戏自动加载）");
    }
}

// ============================================
// GSI 数据处理（private）
// ============================================

void CS2GsiPlugin::on_gsi_data(const QJsonObject& data) {
    if (!host_) {
        return;
    }
    // 归属判断：首次收到有效数据时记录 player.steamid 为基准（本地玩家），
    // 之后每次比较：一致为自身（含观战自己），不一致为队友（正在观战该队友）
    const QString steam_id = extract_string(data, "player", "", "steamid");
    bool is_self = true;
    if (!steam_id.isEmpty()) {
        if (!has_local_steam_id_) {
            has_local_steam_id_ = true;
            local_steam_id_ = steam_id.toStdString();
            PLUGIN_LOG(this, PluginLogLevel::Info, "记录本地玩家 SteamID 基准: " << local_steam_id_);
        }
        is_self = (steam_id.toStdString() == local_steam_id_);
    }
    if (is_self) {
        update_self_values(data);
    }
    else {
        PLUGIN_LOG(this, PluginLogLevel::Debug, "当前数据属于队友（观战中），个人数值不更新");
    }
    // 团队/地图类数值无论归属均更新（全局信息）
    update_team_values(data);
}

void CS2GsiPlugin::update_self_values(const QJsonObject& data) {
    // 个人状态类数值（字段缺失（-1）时不写入）
    const auto write_int = [this, &data](const char* id, const QString& group,
                               const QString& sub, const QString& field) {
        int value = extract_int(data, group, sub, field, -1);
        if (value >= 0) {
            host_->set_value(name(), id, value);
        }
    };
    write_int("health", "player", "state", "health");
    write_int("armor", "player", "state", "armor");
    write_int("money", "player", "state", "money");
    write_int("has_helmet", "player", "state", "helmet");
    write_int("has_defuser", "player", "state", "defusekit");
    write_int("flashed", "player", "state", "flashed");
    write_int("smoked", "player", "state", "smoked");
    write_int("burning", "player", "state", "burning");
    write_int("round_kills", "player", "state", "round_kills");
    write_int("round_killhs", "player", "state", "round_killhs");
    write_int("round_totaldmg", "player", "state", "round_totaldmg");
    write_int("equip_value", "player", "state", "equip_value");
    write_int("kills", "player", "match_stats", "kills");
    write_int("assists", "player", "match_stats", "assists");
    write_int("deaths", "player", "match_stats", "deaths");
    write_int("mvps", "player", "match_stats", "mvps");
    // 队伍编号（team 为 "CT"/"T" 字符串映射 3/2；为数字时直接使用）
    QString team = extract_string(data, "player", "", "team");
    if (!team.isEmpty()) {
        host_->set_value(name(), "team_num", (team.toUpper() == "CT") ? 3 : 2);
    }
    else {
        int team_num = extract_int(data, "player", "", "team", -1);
        if (team_num >= 0) {
            host_->set_value(name(), "team_num", team_num);
        }
    }
}

void CS2GsiPlugin::update_team_values(const QJsonObject& data) {
    // 团队/地图类数值（无论数据归属均更新）
    const auto write_int = [this, &data](const char* id, const QString& group,
                               const QString& sub, const QString& field) {
        int value = extract_int(data, group, sub, field, -1);
        if (value >= 0) {
            host_->set_value(name(), id, value);
        }
    };
    write_int("ct_score", "map", "team_ct", "score");
    write_int("t_score", "map", "team_t", "score");
    write_int("ct_consecutive_round_losses", "map", "team_ct", "consecutive_round_losses");
    write_int("t_consecutive_round_losses", "map", "team_t", "consecutive_round_losses");
    // 炸弹状态（bomb.state 固定字符串集合映射为数字；未知状态不更新）
    static const std::map<QString, int> bomb_map = {
        {"planted", 1}, {"exploding", 2}, {"exploded", 3}, {"defused", 4}};
    QString bomb_state = extract_string(data, "bomb", "", "state");
    auto bomb_it = bomb_map.find(bomb_state.toLower());
    if (bomb_it != bomb_map.end()) {
        host_->set_value(name(), "bomb_state", bomb_it->second);
    }
    // 地图阶段（map.phase 固定字符串集合映射为数字；未知阶段不更新）
    static const std::map<QString, int> phase_map = {
        {"warmup", 0}, {"calibration", 1}, {"teamchange", 2},
        {"firsthalf", 3}, {"halftime", 4}, {"secondhalf", 5}, {"gameover", 6}};
    QString phase = extract_string(data, "map", "", "phase");
    auto phase_it = phase_map.find(phase.toLower());
    if (phase_it != phase_map.end()) {
        host_->set_value(name(), "map_phase", phase_it->second);
    }
}

// ============================================
// 路径与配置（private）
// ============================================

QString CS2GsiPlugin::find_cs_directory() {
    // 通过 PathFinder.py 查找 CS 目录（Steam 模式，非交互，返回第一个匹配）
    QStringList args;
    args << "--mode" << "steam"
         << "--steam-game" << "Counter-Strike Global Offensive"
         << "--type" << "directory"
         << "--no-interactive";
    QString output = run_path_finder(args);
    QString result = output.trimmed();
    // PathFinder 结果输出为完整路径（可能含换行，取第一行）
    int newline = result.indexOf('\n');
    if (newline > 0) {
        result = result.left(newline);
    }
    if (!result.isEmpty() && QDir(result).exists()) {
        cs_dir_ = QDir::cleanPath(result).toStdString();
        PLUGIN_LOG(this, PluginLogLevel::Info, "找到 CS 游戏目录: " << cs_dir_);
        return QString::fromStdString(cs_dir_);
    }
    PLUGIN_LOG(this, PluginLogLevel::Warn, "未能通过 PathFinder 找到 CS 游戏目录");
    return QString();
}

QString CS2GsiPlugin::generate_config() {
    if (cs_dir_.empty()) {
        QString cs_dir = find_cs_directory();
        if (cs_dir.isEmpty()) {
            PLUGIN_LOG(this, PluginLogLevel::Warn, "未找到 CS 目录，无法生成配置文件");
            return QString();
        }
        cs_dir_ = cs_dir.toStdString();
    }
    QString cfg_dir = find_cfg_dir(QString::fromStdString(cs_dir_));
    if (cfg_dir.isEmpty()) {
        PLUGIN_LOG(this, PluginLogLevel::Error, "无法确定 GSI 配置文件目录");
        return QString();
    }
    // 随机端口（首次生成时确定，之后保持不变）
    if (port_ <= 0) {
        port_ = random_port();
    }
    // buffer/throttle 由模块最小查询周期决定
    int min_period_ms = host_ ? host_->base_period_ms() : 1000;
    QString buffer = "0.1";
    QString throttle = compute_throttle(min_period_ms);

    // 生成 GSI 配置文件内容
    QString config;
    config += "\"gamestate_integration_dglab\"\n";
    config += "{\n";
    config += QString("    \"uri\"           \"http://127.0.0.1:%1\"\n").arg(port_);
    config += "    \"timeout\"       \"5.0\"\n";
    config += QString("    \"buffer\"        \"%1\"\n").arg(buffer);
    config += QString("    \"throttle\"      \"%1\"\n").arg(throttle);
    config += "    \"heartbeat\"     \"10.0\"\n";
    config += "    \"data\"\n";
    config += "    {\n";
    config += "        \"provider\"           \"1\"\n";
    config += "        \"map\"                \"1\"\n";
    config += "        \"round\"              \"1\"\n";
    config += "        \"player_id\"          \"1\"\n";
    config += "        \"player_state\"       \"1\"\n";
    config += "        \"player_weapons\"     \"1\"\n";
    config += "        \"player_match_stats\" \"1\"\n";
    config += "    }\n";
    config += "}\n";

    QString path = QDir(cfg_dir).filePath("gamestate_integration_dglab.cfg");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        PLUGIN_LOG(this, PluginLogLevel::Error, "配置文件写入失败: " << path.toStdString());
        return QString();
    }
    file.write(config.toUtf8());
    file.close();
    config_path_ = path.toStdString();
    save_config_path();
    PLUGIN_LOG(this, PluginLogLevel::Info,
        "GSI 配置文件已生成: " << config_path_
                               << "，端口: " << port_ << "，throttle: " << throttle.toStdString());
    return path;
}

QString CS2GsiPlugin::find_cfg_dir(const QString& cs_dir) {
    // CS2 的 GSI 配置位于 game/csgo/cfg 或 csgo/cfg
    QStringList candidates;
    candidates << QDir(cs_dir).filePath("game/csgo/cfg")
               << QDir(cs_dir).filePath("csgo/cfg");
    for (const QString& candidate : candidates) {
        if (QDir(candidate).exists()) {
            return QDir::cleanPath(candidate);
        }
    }
    // 都不存在：尝试创建 game/csgo/cfg（CS2 标准结构）
    QString fallback = QDir(cs_dir).filePath("game/csgo/cfg");
    if (QDir().mkpath(fallback)) {
        return QDir::cleanPath(fallback);
    }
    return QString();
}

QString CS2GsiPlugin::compute_throttle(int min_period_ms) {
    // throttle（最小发送间隔，秒）取模块最小查询周期
    double seconds = static_cast<double>(qMax(50, min_period_ms)) / 1000.0;
    return QString::number(seconds, 'f', 2);
}

int CS2GsiPlugin::random_port() {
    // 随机端口区间避开常见服务端口
    return QRandomGenerator::global()->bounded(20000, 60000);
}

void CS2GsiPlugin::start_gsi_listener() {
    if (port_ <= 0) {
        PLUGIN_LOG(this, PluginLogLevel::Warn,
            "端口无效，无法启动监听（端口: " << port_ << "）");
        return;
    }
    // 端口被占用时重新随机端口并重新生成配置，最多重试 5 次
    for (int attempt = 0; attempt < 5; ++attempt) {
        if (host_ && host_->listen_data(port_)) {
            PLUGIN_LOG(this, PluginLogLevel::Info, "GSI 端口监听已启动: " << port_);
            return;
        }
        PLUGIN_LOG(this, PluginLogLevel::Warn,
            "端口 " << port_ << " 监听失败，重新随机端口并更新配置");
        port_ = random_port();
        if (config_path_.empty() || generate_config().isEmpty()) {
            return;
        }
    }
    PLUGIN_LOG(this, PluginLogLevel::Error, "连续 5 次端口监听失败，GSI 数据接收不可用");
}

void CS2GsiPlugin::save_config_path() const {
    if (!host_) {
        return;
    }
    host_->set_config_value("app.gsi.cs_dir", cs_dir_);
    host_->set_config_value("app.gsi.config_path", config_path_);
    host_->set_config_value("app.gsi.port", std::to_string(port_));
}

void CS2GsiPlugin::load_config_path() {
    if (!host_) {
        return;
    }
    cs_dir_ = host_->get_config_value("app.gsi.cs_dir", "");
    config_path_ = host_->get_config_value("app.gsi.config_path", "");
    port_ = std::atoi(host_->get_config_value("app.gsi.port", "0").c_str());
    if (!cs_dir_.empty() && !QDir(QString::fromStdString(cs_dir_)).exists()) {
        // 目录已失效，重新查找
        cs_dir_.clear();
        config_path_.clear();
        port_ = 0;
    }
}

QString CS2GsiPlugin::run_path_finder(const QStringList& args) {
    // 使用宿主配置中的 Python 解释器运行 PathFinder.py
    std::string python = host_ ? host_->get_config_value("python.path", "python") : "python";
    QString script = QCoreApplication::applicationDirPath() + "/python/PathFinder.py";
    QProcess process;
    QStringList full_args;
    full_args << script << args;
    process.start(QString::fromStdString(python), full_args);
    if (!process.waitForFinished(20000)) {
        PLUGIN_LOG(this, PluginLogLevel::Warn, "PathFinder 执行超时");
        process.kill();
        return QString();
    }
    QString output = QString::fromUtf8(process.readAllStandardOutput());
    if (process.exitCode() != 0 && output.trimmed().isEmpty()) {
        PLUGIN_LOG(this, PluginLogLevel::Warn,
            "PathFinder 执行失败: "
                << QString::fromUtf8(process.readAllStandardError()).trimmed().toStdString());
        return QString();
    }
    return output;
}

bool CS2GsiPlugin::is_process_running(const QString& process_name) {
    if (process_name.isEmpty()) {
        return false;
    }
    QProcess process;
#ifdef Q_OS_WIN
    // Windows：tasklist 输出 CSV 格式，按进程名过滤
    process.start("tasklist",
        {"/FO", "CSV", "/NH", "/FI", QString("IMAGENAME eq %1").arg(process_name)});
#else
    // Unix-like：ps 输出全部进程命令名
    process.start("ps", {"-e", "-o", "comm="});
#endif
    if (!process.waitForFinished(3000) || process.exitCode() != 0) {
        return false;
    }
    const QString output = QString::fromUtf8(process.readAllStandardOutput());
    const QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        QString name;
#ifdef Q_OS_WIN
        // 简单 CSV 解析：取第一个字段（进程名），去掉首尾引号
        if (line.startsWith('"')) {
            int end = line.indexOf('"', 1);
            if (end > 0) {
                name = line.mid(1, end - 1);
            }
        }
#else
        name = line.trimmed();
#endif
        if (name.compare(process_name, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

// ============================================
// 解析工具（private，静态）
// ============================================

QJsonValue CS2GsiPlugin::extract_gsi_value(const QJsonObject& data, const QString& group,
    const QString& sub, const QString& field) {
    // 1) 标准结构 data[group][sub][field]（如 player.state.health / map.team_ct.score）
    if (!sub.isEmpty() && data.contains(group) && data[group].isObject()) {
        const QJsonObject group_obj = data[group].toObject();
        if (group_obj.contains(sub) && group_obj[sub].isObject()) {
            const QJsonObject sub_obj = group_obj[sub].toObject();
            if (sub_obj.contains(field)) {
                return sub_obj.value(field);
            }
        }
    }
    // 2) 变体结构 data[group_sub][field]（如 player_state.health / player_match_stats.kills）
    if (!sub.isEmpty()) {
        const QString flat = group + "_" + sub;
        if (data.contains(flat) && data[flat].isObject()) {
            const QJsonObject flat_obj = data[flat].toObject();
            if (flat_obj.contains(field)) {
                return flat_obj.value(field);
            }
        }
    }
    // 3) 直接结构 data[group][field]（如 player.steamid / player.team / map.phase / bomb.state）
    if (data.contains(group) && data[group].isObject()) {
        const QJsonObject group_obj = data[group].toObject();
        if (group_obj.contains(field)) {
            return group_obj.value(field);
        }
    }
    return QJsonValue(QJsonValue::Undefined);
}

int CS2GsiPlugin::extract_int(const QJsonObject& data, const QString& group, const QString& sub,
    const QString& field, int fallback) {
    QJsonValue value = extract_gsi_value(data, group, sub, field);
    if (value.isBool()) {
        return value.toBool() ? 1 : 0;
    }
    if (value.isDouble()) {
        return value.toInt();
    }
    return fallback;
}

QString CS2GsiPlugin::extract_string(const QJsonObject& data, const QString& group,
    const QString& sub, const QString& field) {
    QJsonValue value = extract_gsi_value(data, group, sub, field);
    return value.isString() ? value.toString() : QString();
}

// ============================================
// 动态库导出（extern "C" 工厂函数）
// ============================================
extern "C" {
PLUGIN_EXPORT int get_plugin_api_version() {
    return PLUGIN_API_VERSION;
}

PLUGIN_EXPORT IPlugin* create_plugin() {
    // 内存隔离：实例在插件内部 new，由 destroy_plugin 在插件内部 delete
    return new CS2GsiPlugin();
}

PLUGIN_EXPORT void destroy_plugin(IPlugin* plugin) {
    delete plugin;
}
}
