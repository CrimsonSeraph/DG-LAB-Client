#pragma once

#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QObject>
#include <QProcess>
#include <QTcpSocket>
#include <QThread>
#include <QWaitCondition>

#include <atomic>
#include <functional>
#include <map>

class PythonSubprocessManager : public QObject
{
    Q_OBJECT

public:
    explicit PythonSubprocessManager(QObject* parent = nullptr);
    ~PythonSubprocessManager();

    // 启动Python子进程
    void start_process(const QString& python_executable, const QString& script_path);

    // 检查是否已连接
    bool is_connected() const { return socket_ && socket_->state() == QTcpSocket::ConnectedState; }

    // 发送命令
    void call(const QJsonObject& cmd, std::function<void(const QJsonObject&)> callback, int timeout = 5000);

signals:
    void started(bool success, const QString& error_string);    // 子进程启动并TCP连接成功
    void finished();    // 子进程结束
    void command_response(int token, const QJsonObject& response);  // 用于异步响应

private slots:
    void on_process_started();
    void on_process_error(QProcess::ProcessError error);
    void on_process_finished(int exit_code, QProcess::ExitStatus status);
    void on_socket_connected();
    void on_socket_error(QTcpSocket::SocketError error);
    void on_socket_ready_read();
    void handle_stdout();
    void handle_stderr();

private:
    QProcess* process_;
    QTcpSocket* socket_;
    int port_;

    // 用于同步等待响应的成员
    QMutex mutex_;
    QWaitCondition wait_cond_;
    QJsonObject last_response_;
    bool response_received_;

    std::atomic<int> next_token_{ 0 };
    QMap<int, std::function<void(const QJsonObject&)>> pending_callbacks_;
    QMutex callback_mutex_;

    // 停止标志
    std::atomic<bool> stopping_{ false };

    void process_output(const QByteArray& data, bool is_error);
    void parse_port_from_output(const QByteArray& data);
    void send_json(const QJsonObject& obj);
    QJsonObject send_command(const QJsonObject& cmd, int timeout);
};
