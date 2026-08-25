/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "PluginHost.h"

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>

#include <string>
#include <vector>

// ============================================
// CS2GsiPlugin - CS2 Game State Integration 插件
// 通过 PathFinder 查找 CS 游戏目录、生成 GSI 配置文件（gamestate_integration_dglab.cfg）、
// 监听 GSI 数据并区分自身/队友数据归属后写入数值模块（完整数值列表参照官方 GSI 规范）
// ============================================
class CS2GsiPlugin : public IPlugin {
public:
    // -------------------- 自描述 --------------------
    /// @brief 获取插件名称（模块名，规则引用沿用原名称）
    /// @return 插件名称
    std::string name() const override { return "CS2 GSI 模块"; }

    /// @brief 获取插件版本号
    /// @return 版本号
    std::string version() const override { return "0.6.0"; }

    /// @brief 获取插件 API 版本
    /// @return API 版本号
    int api_version() const override { return PLUGIN_API_VERSION; }

    /// @brief 获取插件能力标志（提供数值 + 消费外部数据）
    /// @return 能力标志
    PluginCapability capabilities() const override {
        return PluginCapability::ProvidesValues | PluginCapability::ConsumesData;
    }

    // -------------------- 生命周期 --------------------
    /// @brief 初始化：注册数值、加载/生成 GSI 配置、注册数据处理器并启动监听
    /// @return 成功返回 PluginError::Ok
    PluginError initialize() override;

    /// @brief 反初始化：注销数据处理器、停止监听、注销模块
    void uninitialize() override;

    /// @brief 卸载前检查：反初始化后无活动资源，允许卸载
    /// @return 允许卸载返回 true
    bool can_unload() const override { return true; }

    /// @brief 宿主周期变化：更新 GSI 配置（throttle 随周期变化）并检查游戏进程
    void on_host_period_changed() override;

private:
    // -------------------- 数值定义 --------------------
    /// @brief 完整数值列表（参照官方 GSI 规范；个人状态类 + 团队/地图类）
    /// @return 数值列表
    static std::vector<ModuleValue> create_default_values();

    // -------------------- GSI 数据处理 --------------------
    /// @brief 收到 GSI 数据：判断自身/队友归属并写入数值
    /// @param data GSI 完整数据
    void on_gsi_data(const QJsonObject& data);

    /// @brief 更新个人状态类数值（仅自身数据时调用）
    /// @param data GSI 完整数据
    void update_self_values(const QJsonObject& data);

    /// @brief 更新团队/地图类数值（无论数据归属均更新）
    /// @param data GSI 完整数据
    void update_team_values(const QJsonObject& data);

    // -------------------- 路径与配置 --------------------
    /// @brief 通过 PathFinder.py 查找 CS 游戏目录
    /// @return CS 目录绝对路径，未找到返回空字符串
    QString find_cs_directory();

    /// @brief 生成（或更新）GSI 配置文件（随机端口，buffer/throttle 由最小查询周期决定）
    /// @return 配置文件绝对路径，失败返回空字符串
    QString generate_config();

    /// @brief 确定 GSI 配置文件目录（优先 game/csgo/cfg，其次 csgo/cfg，不存在则创建）
    /// @param cs_dir CS 游戏目录
    /// @return cfg 目录绝对路径，无效返回空字符串
    static QString find_cfg_dir(const QString& cs_dir);

    /// @brief 根据最小查询周期计算 GSI throttle 参数（秒）
    /// @param min_period_ms 最小查询周期（毫秒）
    /// @return throttle 字符串（如 "0.25"）
    static QString compute_throttle(int min_period_ms);

    /// @brief 生成随机端口（20000~60000）
    /// @return 端口号
    static int random_port();

    /// @brief 启动 GSI 端口监听（端口被占用时重新随机端口并重新生成配置，最多重试 5 次）
    void start_gsi_listener();

    /// @brief 将配置路径/目录/端口记录到宿主 user 配置（app.gsi 下）
    void save_config_path() const;

    /// @brief 从宿主 user 配置（app.gsi 下）加载已记录配置
    void load_config_path();

    /// @brief 调用 PathFinder.py 执行查找（QProcess 运行 python 脚本）
    /// @param args PathFinder 命令行参数
    /// @return 标准输出文本
    QString run_path_finder(const QStringList& args);

    /// @brief 检查指定名称的进程是否在运行（Windows tasklist / Unix ps）
    /// @param process_name 进程名
    /// @return 运行中返回 true
    static bool is_process_running(const QString& process_name);

    // -------------------- 解析工具 --------------------
    /// @brief 从 GSI JSON 中按候选结构提取字段值（支持标准/变体/直接三种结构）
    /// @param data GSI 完整数据
    /// @param group 分组名（如 "player"）
    /// @param sub 子分组名（如 "state"，可为空）
    /// @param field 目标字段名
    /// @return 字段值，未找到返回 Undefined
    static QJsonValue extract_gsi_value(const QJsonObject& data, const QString& group,
        const QString& sub, const QString& field);

    /// @brief 提取整数值（布尔转 1/0，未找到返回 fallback）
    /// @param data GSI 完整数据
    /// @param group 分组名
    /// @param sub 子分组名
    /// @param field 目标字段名
    /// @param fallback 默认值
    /// @return 提取的整数值
    static int extract_int(const QJsonObject& data, const QString& group, const QString& sub,
        const QString& field, int fallback);

    /// @brief 提取字符串值（未找到返回空字符串）
    /// @param data GSI 完整数据
    /// @param group 分组名
    /// @param sub 子分组名
    /// @param field 目标字段名
    /// @return 字符串值
    static QString extract_string(const QJsonObject& data, const QString& group,
        const QString& sub, const QString& field);

    // -------------------- 成员变量 --------------------
    bool initialized_ = false;        ///< 初始化标志
    std::string cs_dir_;              ///< CS 游戏目录
    std::string config_path_;         ///< GSI 配置文件路径
    int port_ = 0;                    ///< GSI 监听端口
    int last_min_period_ms_ = 0;      ///< 上次处理的最小查询周期
    std::string local_steam_id_;      ///< 本地玩家 SteamID 基准（首次有效数据记录）
    bool has_local_steam_id_ = false; ///< 是否已记录 SteamID 基准
};
