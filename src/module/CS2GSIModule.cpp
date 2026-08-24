/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "CS2GSIModule.h"

#include "AppConfig.h"
#include "DebugLog.h"
#include "GsiServer.h"
#include "ModuleManager.h"
#include "ProcessChecker.h"

#include <QJsonObject>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QRandomGenerator>
#include <QRegularExpression>

// ============================================
// 单例（public）
// ============================================

CS2GSIModule& CS2GSIModule::instance() {
    static CS2GSIModule module;
    return module;
}

// ============================================
// 构造/析构（private）
// ============================================

CS2GSIModule::CS2GSIModule()
    : QObject(nullptr) {
    // 监听模块周期变化：最小查询周期改变时更新 GSI 配置并检查游戏进程
    connect(&ModuleManager::instance(), &ModuleManager::period_changed,
        this, &CS2GSIModule::on_period_changed);
    // 创建 GSI 监听服务器，接收 CS2 游戏推送的数据
    gsi_server_ = new GsiServer(this);
    connect(gsi_server_, &GsiServer::data_received,
        this, &CS2GSIModule::on_gsi_data_received);
}

CS2GSIModule::~CS2GSIModule() = default;

// ============================================
// 初始化（public）
// ============================================

void CS2GSIModule::init() {
    // 加载已记录的配置路径/目录/端口
    load_config_path();
    // 首次运行（无记录）或目录失效时：查找 CS 目录并生成配置文件
    if (config_path_.isEmpty() || !QFile::exists(config_path_)) {
        QString cs_dir = find_cs_directory();
        if (cs_dir.isEmpty()) {
            LOG_MODULE("CS2GSIModule", "init", LOG_WARN,
                "未找到 CS 游戏目录，暂不生成 GSI 配置文件");
        }
        else {
            cs_dir_ = cs_dir;
            generate_config();
        }
    }
    last_min_period_ms_ = ModuleManager::instance().get_base_period_ms();
    // 启动 GSI 端口监听（接收 CS2 游戏数据）
    start_gsi_listener();
    LOG_MODULE("CS2GSIModule", "init", LOG_INFO,
        "CS2 GSI 模块初始化完成，配置文件: " << config_path_.toStdString());
}

const char* CS2GSIModule::module_name() {
    return "CS2 GSI 模块";
}

std::vector<ModuleValue> CS2GSIModule::create_default_values() {
    // 参照 CS2 官方 GSI 规范注册常用数值（id 用于规则引用，field 为底层字段名）
    std::vector<ModuleValue> values;
    values.emplace_back("health", "当前血量", QueryPeriod::QUARTER_SECOND, "m_iHealth");
    values.emplace_back("armor", "当前护甲", QueryPeriod::HALF_SECOND, "m_ArmorValue");
    values.emplace_back("team_num", "队伍编号", QueryPeriod::SECOND, "m_iTeamNum");
    values.emplace_back("money", "金钱", QueryPeriod::TWO_SECONDS, "m_iMoney");
    values.emplace_back("has_helmet", "是否有头盔", QueryPeriod::FOUR_SECONDS, "m_bHasHelmet");
    values.emplace_back("has_defuser", "是否有拆弹器", QueryPeriod::FOUR_SECONDS, "m_bHasDefuser");
    return values;
}

// ============================================
// 路径与配置（public）
// ============================================

QString CS2GSIModule::find_cs_directory() {
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
        cs_dir_ = QDir::cleanPath(result);
        LOG_MODULE("CS2GSIModule", "find_cs_directory", LOG_INFO,
            "找到 CS 游戏目录: " << cs_dir_.toStdString());
        return cs_dir_;
    }
    LOG_MODULE("CS2GSIModule", "find_cs_directory", LOG_WARN,
        "未能通过 PathFinder 找到 CS 游戏目录");
    return QString();
}

QString CS2GSIModule::generate_config() {
    if (cs_dir_.isEmpty()) {
        cs_dir_ = find_cs_directory();
        if (cs_dir_.isEmpty()) {
            LOG_MODULE("CS2GSIModule", "generate_config", LOG_WARN,
                "未找到 CS 目录，无法生成配置文件");
            return QString();
        }
    }
    QString cfg_dir = find_cfg_dir(cs_dir_);
    if (cfg_dir.isEmpty()) {
        LOG_MODULE("CS2GSIModule", "generate_config", LOG_ERROR,
            "无法确定 GSI 配置文件目录");
        return QString();
    }
    // 随机端口（首次生成时确定，之后保持不变）
    if (port_ <= 0) {
        port_ = random_port();
    }
    // buffer/throttle 由模块最小查询周期决定
    int min_period_ms = ModuleManager::instance().get_base_period_ms();
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
        LOG_MODULE("CS2GSIModule", "generate_config", LOG_ERROR,
            "配置文件写入失败: " << path.toStdString());
        return QString();
    }
    file.write(config.toUtf8());
    file.close();
    config_path_ = path;
    save_config_path();
    LOG_MODULE("CS2GSIModule", "generate_config", LOG_INFO,
        "GSI 配置文件已生成: " << path.toStdString()
                               << "，端口: " << port_ << "，throttle: " << throttle.toStdString());
    return path;
}

// ============================================
// private slots 实现
// ============================================

void CS2GSIModule::on_period_changed() {
    int new_min_period = ModuleManager::instance().get_base_period_ms();
    if (new_min_period == last_min_period_ms_) {
        return;
    }
    last_min_period_ms_ = new_min_period;
    LOG_MODULE("CS2GSIModule", "on_period_changed", LOG_DEBUG,
        "最小查询周期变化: " << new_min_period << "ms");
    // 已有配置文件时重新生成（throttle 随周期变化）
    if (config_path_.isEmpty()) {
        return;
    }
    QString new_path = generate_config();
    if (new_path.isEmpty()) {
        return;
    }
    // 配置仅当游戏启动时加载、不支持热重载：游戏运行中需提示重启
    if (ProcessChecker::isRunning("cs2.exe")) {
        LOG_MODULE("CS2GSIModule", "on_period_changed", LOG_INFO,
            "CS2 正在运行，配置已更新，需重启游戏生效");
        emit game_restart_required(new_path);
    }
    else {
        LOG_MODULE("CS2GSIModule", "on_period_changed", LOG_INFO,
            "CS2 未运行，配置已更新（下次启动游戏自动加载）");
    }
}

void CS2GSIModule::on_gsi_data_received(const QJsonObject& data) {
    // 从 GSI 数据中解析玩家数值并写入数值模块（兼容 player.state 与 player_state 两种结构）
    auto& manager = ModuleManager::instance();
    const QString module = QString::fromUtf8(module_name());
    // 血量 / 护甲 / 金钱 / 头盔 / 拆弹器（字段缺失（-1）时不写入）
    int health = extract_gsi_field(data, "player", "state", "health", -1);
    if (health >= 0) {
        manager.set_value(module.toStdString(), "health", health);
    }
    int armor = extract_gsi_field(data, "player", "state", "armor", -1);
    if (armor >= 0) {
        manager.set_value(module.toStdString(), "armor", armor);
    }
    int money = extract_gsi_field(data, "player", "state", "money", -1);
    if (money >= 0) {
        manager.set_value(module.toStdString(), "money", money);
    }
    int helmet = extract_gsi_field(data, "player", "state", "helmet", -1);
    if (helmet >= 0) {
        manager.set_value(module.toStdString(), "has_helmet", helmet);
    }
    int defuser = extract_gsi_field(data, "player", "state", "defusekit", -1);
    if (defuser >= 0) {
        manager.set_value(module.toStdString(), "has_defuser", defuser);
    }
    // 队伍编号（team 为 "CT"/"T" 字符串，映射 3/2；为数字时直接使用）
    QJsonValue team_value;
    if (data.contains("player") && data["player"].isObject()) {
        team_value = data["player"].toObject().value("team");
    }
    if (team_value.isString()) {
        QString team = team_value.toString().toUpper();
        manager.set_value(module.toStdString(), "team_num",
            (team == "CT") ? 3 : 2);
    }
    else if (team_value.isDouble()) {
        manager.set_value(module.toStdString(), "team_num", team_value.toInt());
    }
    // LOG_MODULE("CS2GSIModule", "on_gsi_data_received", LOG_DEBUG,
    //     "GSI 数据写入模块: health=" << health << ", armor=" << armor
    //     << ", money=" << money << ", helmet=" << helmet << ", defuser=" << defuser);
}

// ============================================
// 私有辅助函数实现（private）
// ============================================

void CS2GSIModule::start_gsi_listener() {
    if (!gsi_server_ || port_ <= 0) {
        LOG_MODULE("CS2GSIModule", "start_gsi_listener", LOG_WARN,
            "端口无效，无法启动监听（端口: " << port_ << "）");
        return;
    }
    // 端口被占用时重新随机端口并重新生成配置，最多重试 5 次
    for (int attempt = 0; attempt < 5; ++attempt) {
        if (gsi_server_->start_listening(port_)) {
            LOG_MODULE("CS2GSIModule", "start_gsi_listener", LOG_INFO,
                "GSI 端口监听已启动: " << port_);
            return;
        }
        LOG_MODULE("CS2GSIModule", "start_gsi_listener", LOG_WARN,
            "端口 " << port_ << " 监听失败，重新随机端口并更新配置");
        port_ = random_port();
        if (config_path_.isEmpty() || generate_config().isEmpty()) {
            return;
        }
    }
    LOG_MODULE("CS2GSIModule", "start_gsi_listener", LOG_ERROR,
        "连续 5 次端口监听失败，GSI 数据接收不可用");
}

int CS2GSIModule::extract_gsi_field(const QJsonObject& data, const QString& player_key,
    const QString& state_key, const QString& field, int fallback) {
    QJsonValue result = fallback;
    // 支持 player.state.xxx 与 player_state.xxx 两种 GSI 结构
    const QStringList player_candidates = {player_key, "player_state"};
    for (const QString& pk : player_candidates) {
        if (!data.contains(pk) || !data[pk].isObject()) {
            continue;
        }
        const QJsonObject player_obj = data[pk].toObject();
        // state 字段：state_key 或直接 player 下
        if (player_obj.contains(state_key) && player_obj[state_key].isObject()) {
            const QJsonObject state_obj = player_obj[state_key].toObject();
            if (state_obj.contains(field)) {
                result = state_obj.value(field);
                break;
            }
        }
        else if (player_obj.contains(field)) {
            result = player_obj.value(field);
            break;
        }
    }
    if (result.isBool()) {
        return result.toBool() ? 1 : 0;
    }
    if (result.isDouble()) {
        return result.toInt();
    }
    return fallback;
}

int CS2GSIModule::random_port() {
    // 随机端口区间避开常见服务端口
    return QRandomGenerator::global()->bounded(20000, 60000);
}

QString CS2GSIModule::compute_throttle(int min_period_ms) {
    // throttle（最小发送间隔，秒）取模块最小查询周期
    double seconds = static_cast<double>(qMax(50, min_period_ms)) / 1000.0;
    return QString::number(seconds, 'f', 2);
}

QString CS2GSIModule::find_cfg_dir(const QString& cs_dir) {
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

void CS2GSIModule::save_config_path() const {
    auto& config = AppConfig::instance();
    config.set_value_with_name<std::string>("app.gsi.cs_dir", cs_dir_.toStdString(), "user");
    config.set_value_with_name<std::string>("app.gsi.config_path", config_path_.toStdString(), "user");
    config.set_value_with_name<int>("app.gsi.port", port_, "user");
}

void CS2GSIModule::load_config_path() {
    auto& config = AppConfig::instance();
    cs_dir_ = QString::fromStdString(config.get_value<std::string>("app.gsi.cs_dir", ""));
    config_path_ = QString::fromStdString(config.get_value<std::string>("app.gsi.config_path", ""));
    port_ = config.get_value<int>("app.gsi.port", 0);
    if (!cs_dir_.isEmpty() && !QDir(cs_dir_).exists()) {
        // 目录已失效，重新查找
        cs_dir_.clear();
        config_path_.clear();
        port_ = 0;
    }
}

QString CS2GSIModule::run_path_finder(const QStringList& args) {
    // 使用配置中的 Python 解释器运行 PathFinder.py
    auto& config = AppConfig::instance();
    QString python = QString::fromStdString(config.get_value<std::string>("python.path", "python"));
    QString script = QCoreApplication::applicationDirPath() + "/python/PathFinder.py";
    QProcess process;
    QStringList full_args;
    full_args << script << args;
    process.start(python, full_args);
    if (!process.waitForFinished(20000)) {
        LOG_MODULE("CS2GSIModule", "run_path_finder", LOG_WARN, "PathFinder 执行超时");
        process.kill();
        return QString();
    }
    QString output = QString::fromUtf8(process.readAllStandardOutput());
    if (process.exitCode() != 0 && output.trimmed().isEmpty()) {
        LOG_MODULE("CS2GSIModule", "run_path_finder", LOG_WARN,
            "PathFinder 执行失败: " << QString::fromUtf8(process.readAllStandardError()).trimmed().toStdString());
        return QString();
    }
    return output;
}
