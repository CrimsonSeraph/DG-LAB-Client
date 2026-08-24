/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "ModuleValue.h"

#include <QObject>
#include <QString>

#include <string>
#include <vector>

// 前置声明
class ModuleManager;

// ============================================
// CS2GSIModule - CS2 GSI 数值模块（单例）
// 负责 CS2 模块数值定义、通过 PathFinder 查找 CS 目录、
// 生成/更新 GSI 配置文件（gamestate_integration_*.cfg）与周期变更时的重启提示
// ============================================
class CS2GSIModule : public QObject {
    Q_OBJECT

public:
    // -------------------- 单例 --------------------
    /// @brief 获取单例实例
    static CS2GSIModule& instance();

    // -------------------- 初始化 --------------------
    /// @brief 初始化：加载已记录配置、首次查找并生成配置文件、连接周期变化信号
    void init();

    /// @brief 获取模块名称
    /// @return 模块名称
    static const char* module_name();

    /// @brief 获取 CS2 模块的默认数值列表（供 ModuleManager 注册）
    /// @return 数值列表
    static std::vector<ModuleValue> create_default_values();

    // -------------------- 路径与配置 --------------------
    /// @brief 通过 PathFinder.py 查找 CS 游戏目录
    /// @return CS 目录绝对路径，未找到返回空字符串
    QString find_cs_directory();

    /// @brief 获取当前 CS 游戏目录
    /// @return CS 目录绝对路径（可能为空）
    inline QString cs_directory() const { return cs_dir_; }

    /// @brief 生成（或更新）GSI 配置文件（随机端口，buffer/throttle 由最小查询周期决定）
    /// @return 配置文件绝对路径，失败返回空字符串
    QString generate_config();

    /// @brief 获取已生成的 GSI 配置文件路径
    /// @return 配置文件绝对路径（可能为空）
    inline QString config_file_path() const { return config_path_; }

    /// @brief 获取当前 GSI 监听端口
    /// @return 端口号（0 表示未生成）
    inline int port() const { return port_; }

signals:
    /// @brief 配置已更新且游戏正在运行时发出（提示用户重启游戏使配置生效）
    /// @param config_path 更新后的配置文件路径
    void game_restart_required(const QString& config_path);

private slots:
    /// @brief 模块最小查询周期变化时：更新配置文件（throttle 随周期变化）并检查游戏进程
    void on_period_changed();

private:
    // -------------------- 构造/析构（单例私有）--------------------
    CS2GSIModule();
    ~CS2GSIModule() override;

    // -------------------- 私有辅助函数 --------------------
    /// @brief 生成随机端口（20000~60000）
    /// @return 端口号
    static int random_port();

    /// @brief 根据最小查询周期计算 GSI throttle 参数（秒）
    /// @param min_period_ms 最小查询周期（毫秒）
    /// @return throttle 字符串（如 "0.25"）
    static QString compute_throttle(int min_period_ms);

    /// @brief 确定 GSI 配置文件目录（优先 game/csgo/cfg，其次 csgo/cfg，不存在则创建）
    /// @param cs_dir CS 游戏目录
    /// @return cfg 目录绝对路径，无效返回空字符串
    static QString find_cfg_dir(const QString& cs_dir);

    /// @brief 将配置路径/目录/端口记录到 user.json（app.gsi 下）
    void save_config_path() const;

    /// @brief 从 user.json（app.gsi 下）加载已记录配置
    void load_config_path();

    /// @brief 调用 PathFinder.py 执行查找（QProcess 运行 python 脚本）
    /// @param args PathFinder 命令行参数
    /// @return 标准输出文本
    static QString run_path_finder(const QStringList& args);

    // -------------------- 成员变量 --------------------
    QString cs_dir_;          ///< CS 游戏目录
    QString config_path_;     ///< GSI 配置文件路径
    int port_ = 0;            ///< GSI 监听端口
    int last_min_period_ms_ = 0; ///< 上次处理的最小查询周期
};
