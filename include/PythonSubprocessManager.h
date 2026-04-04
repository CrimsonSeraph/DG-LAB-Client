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
    void start_process(const QString& pythonExecutable, const QString& scriptPath);

    // 检查是否已连接
    bool is_connected() const { return m_socket && m_socket->state() == QTcpSocket::ConnectedState; }

    // 发送命令
    void call(const QJsonObject& cmd, std::function<void(const QJsonObject&)> callback, int timeout = 5000);
signals:
    void started(bool success, const QString& errorString); // 子进程启动并TCP连接成功
    void finished(); // 子进程结束
    void command_response(int token, const QJsonObject& response); // 用于异步响应

private slots:
    void on_process_started();
    void on_process_error(QProcess::ProcessError error);
    void on_process_finished(int exitCode, QProcess::ExitStatus status);
    void on_socket_connected();
    void on_socket_error(QTcpSocket::SocketError error);
    void on_socket_ready_read();

private:
    QProcess* m_process;
    QTcpSocket* m_socket;
    int m_port;

    // 用于同步等待响应的成员
    QMutex m_mutex;
    QWaitCondition m_waitCond;
    QJsonObject m_lastResponse;
    bool m_responseReceived;

    std::atomic<int> m_nextToken{ 0 };
    QMap<int, std::function<void(const QJsonObject&)>> m_pendingCallbacks;
    QMutex m_callbackMutex;

    // 停止标志
    std::atomic<bool> m_stopping{ false };

    void parse_port_from_output(const QByteArray& data);
    void send_json(const QJsonObject& obj);
    QJsonObject send_command(const QJsonObject& cmd, int timeout);
};
