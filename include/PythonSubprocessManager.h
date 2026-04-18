/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QJsonObject>
#include <QMutex>
#include <QObject>
#include <QProcess>
#include <QTcpSocket>
#include <QWaitCondition>

#include <atomic>
#include <functional>
#include <map>

// ============================================
// PythonSubprocessManager - Python 子进程管理与通信
// ============================================
class PythonSubprocessManager : public QObject {
    Q_OBJECT

public:
    // -------------------- 构造/析构 --------------------
    /// @brief 构造函数
    explicit PythonSubprocessManager(QObject* parent = nullptr);
    ~PythonSubprocessManager();

    // -------------------- 公共接口 --------------------
    /// @brief 启动 Python 子进程并尝试 TCP 连接
    /// @param python_executable Python 解释器路径
    /// @param script_path 要运行的脚本路径
    void start_process(const QString& python_executable, const QString& script_path);

    /// @brief 检查是否已与 Python 服务建立 TCP 连接
    inline bool is_connected() const { return socket_ && socket_->state() == QTcpSocket::ConnectedState; }

    /// @brief 异步调用 Python 服务（非阻塞）
    /// @param cmd 要发送的 JSON 命令
    /// @param callback 回调函数，参数为响应 JSON
    /// @param timeout 超时时间（毫秒）
    void call(const QJsonObject& cmd, std::function<void(const QJsonObject&)> callback, int timeout = 5000);

private:
    // -------------------- 成员变量 --------------------
    QProcess* process_; ///< 子进程对象
    QTcpSocket* socket_;    ///< TCP 套接字
    int port_;  ///< Python 服务监听的端口

    // 同步等待相关
    mutable QMutex mutex_;  ///< 保护 wait_cond_、last_response_、response_received_
    QWaitCondition wait_cond_;  ///< 条件变量，用于同步等待响应
    QJsonObject last_response_; ///< 最近收到的响应
    bool response_received_;    ///< 是否已收到响应

    // 异步回调相关
    std::atomic<int> next_token_{ 0 };  ///< 下一个请求 token
    QMap<int, std::function<void(const QJsonObject&)>> pending_callbacks_;  ///< 待处理回调
    QMutex callback_mutex_; ///< 保护 pending_callbacks_

    std::atomic<bool> stopping_{ false };   ///< 析构时停止标志

    // -------------------- 私有辅助函数 --------------------
    /// @brief 处理子进程输出（stdout/stderr）
    void process_output(const QByteArray& data, bool is_error);

    /// @brief 从输出中解析 TCP 端口号
    void parse_port_from_output(const QByteArray& data);

    /// @brief 通过 socket 发送 JSON 对象
    void send_json(const QJsonObject& obj);

    /// @brief 同步发送命令并等待响应（仅供内部 call 的后台任务使用）
    QJsonObject send_command(const QJsonObject& cmd, int timeout);

signals:
    /// @brief 子进程启动并 TCP 连接成功（或失败）时发出
    void started(bool success, const QString& error_string);

    /// @brief 子进程结束时发出
    void finished();

    /// @brief 收到命令响应时发出（用于异步响应）
    void command_response(int token, const QJsonObject& response);

    /// @brief 收到 Python 主动消息时发出（不包含响应）
    void active_message_received(const QJsonObject& message);

private slots:
    // 进程事件
    void on_process_started();
    void on_process_error(QProcess::ProcessError error);
    void on_process_finished(int exit_code, QProcess::ExitStatus status);

    // 套接字事件
    void on_socket_connected();
    void on_socket_error(QTcpSocket::SocketError error);
    void on_socket_ready_read();

    // 输出处理
    void handle_stdout();
    void handle_stderr();
};
