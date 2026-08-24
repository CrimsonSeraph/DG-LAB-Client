/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QJsonObject>
#include <QTcpServer>

// ============================================
// GsiServer - CS2 Game State Integration 本地监听服务器
// 监听 GSI 配置文件中的端口，接收 CS2 游戏发来的 HTTP POST 数据并解析为 JSON
// ============================================
class GsiServer : public QTcpServer {
    Q_OBJECT

public:
    // -------------------- 构造/析构 --------------------
    explicit GsiServer(QObject* parent = nullptr);

    /// @brief 开始监听指定端口
    /// @param port 监听端口
    /// @return 成功返回 true（端口被占用返回 false）
    bool start_listening(int port);

    /// @brief 停止监听
    void stop_listening();

    /// @brief 获取当前监听端口
    /// @return 端口号（未监听返回 0）
    inline int port() const { return port_; }

    /// @brief 是否正在监听
    /// @return 监听中返回 true
    inline bool is_listening() const { return is_listening_; }

signals:
    /// @brief 收到并解析出 GSI 数据时发出
    /// @param data 解析后的 GSI 数据（完整 JSON）
    void data_received(const QJsonObject& data);

protected:
    /// @brief 重写新连接处理：读取 HTTP POST 请求体并解析
    /// @param socketDescriptor 新连接 socket 描述符
    void incomingConnection(qintptr socketDescriptor) override;

private:
    // -------------------- 成员变量 --------------------
    int port_ = 0;             ///< 监听端口
    bool is_listening_ = false; ///< 监听标志
};
