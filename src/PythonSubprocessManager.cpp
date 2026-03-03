#include "PythonSubprocessManager.h"
#include "DebugLog.h"

#include <QDebug>
#include <QTimer>
#include <QCoreApplication>
#include <QThreadPool>
#include <iostream>
#include <QJsonObject>
#include <QJsonDocument>

PythonSubprocessManager::PythonSubprocessManager(QObject* parent)
    : QObject(parent)
    , m_process(new QProcess(this))
    , m_socket(new QTcpSocket(this))
    , m_port(0)
    , m_responseReceived(false) {
    LOG_MODULE("PythonSubprocessManager", "PythonSubprocessManager", LOG_INFO, "创建 PythonSubprocessManager 对象");
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        this, &PythonSubprocessManager::on_process_finished);
    connect(m_process, &QProcess::errorOccurred, this, &PythonSubprocessManager::on_process_error);
    connect(m_process, &QProcess::started, this, &PythonSubprocessManager::on_process_started);
    connect(m_socket, &QTcpSocket::connected, this, &PythonSubprocessManager::on_socket_connected);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &PythonSubprocessManager::on_socket_error);
    connect(m_socket, &QTcpSocket::readyRead, this, &PythonSubprocessManager::on_socket_ready_read);
}

PythonSubprocessManager::~PythonSubprocessManager() {
    LOG_MODULE("PythonSubprocessManager", "~PythonSubprocessManager", LOG_INFO, "析构，清理资源");

    // 通知所有等待线程退出
    m_stopping = true;
    {
        QMutexLocker locker(&m_mutex);
        m_waitCond.wakeAll(); // 唤醒所有等待 send_command 的线程
    }

    // 断开所有来自子对象的信号连接，避免在析构过程中触发槽函数
    LOG_MODULE("PythonSubprocessManager", "~PythonSubprocessManager", LOG_DEBUG, "开始断开信号连接");
    if (m_process) {
        m_process->disconnect(this);
    }
    if (m_socket) {
        m_socket->disconnect(this);
    }

    // 等待线程池中所有后台任务完成（避免访问已销毁的成员）
    LOG_MODULE("PythonSubprocessManager", "~PythonSubprocessManager", LOG_DEBUG, "等待线程池任务结束...");
    QThreadPool::globalInstance()->waitForDone(3000);

    // 终止子进程
    if (m_process->state() != QProcess::NotRunning) {
        LOG_MODULE("PythonSubprocessManager", "~PythonSubprocessManager", LOG_DEBUG, "终止 Python 进程");
        m_process->terminate();
        if (!m_process->waitForFinished(2000)) {
            m_process->kill();
            m_process->waitForFinished(1000);
        }
    }

    // 断开 socket 连接
    if (m_socket->state() == QTcpSocket::ConnectedState) {
        m_socket->disconnectFromHost();
        m_socket->waitForDisconnected(1000);
    }
}

void PythonSubprocessManager::start_process(const QString& pythonExecutable, const QString& scriptPath) {
    m_port = 0;

    QStringList args;
    args << scriptPath;
    m_process->start(pythonExecutable, args);
    if (!m_process->waitForStarted(5000)) {
        LOG_MODULE("PythonSubprocessManager", "start_process", LOG_ERROR, "启动 Python 进程失败（超时）");
        emit started(false, "无法启动Python进程");
    }
    else {
        LOG_MODULE("PythonSubprocessManager", "start_process", LOG_DEBUG, "waitForStarted 成功，等待进程真正启动信号");
    }
}

void PythonSubprocessManager::call(const QJsonObject& cmd, std::function<void(const QJsonObject&)> callback, int timeout) {
    if (m_stopping) {
        LOG_MODULE("PythonSubprocessManager", "call", LOG_WARN, "对象正在销毁，忽略新调用");
        if (callback) {
            callback({ {"status", "error"}, {"message", "对象正在销毁"} });
        }
        return;
    }

    int token = ++m_nextToken;

    {
        QMutexLocker locker(&m_callbackMutex);
        m_pendingCallbacks[token] = callback;
    }

    QJsonObject cmdWithToken = cmd;
    cmdWithToken["req_id"] = token;

    std::string cmd_str = DebugLogUtil::remove_newline(QJsonDocument(cmdWithToken).toJson().toStdString());
    LOG_MODULE("PythonSubprocessManager", "call", LOG_DEBUG,
        "异步调用，token=" << token << "，命令: " << cmd_str);

    QThreadPool::globalInstance()->start([this, cmdWithToken, timeout, token]() {
        if (m_stopping) {
            LOG_MODULE("PythonSubprocessManager", "call_lambda", LOG_DEBUG,
                "对象正在销毁，后台任务提前退出，token=" << token);
            return;
        }

        LOG_MODULE("PythonSubprocessManager", "call_lambda", LOG_DEBUG,
            "后台线程开始执行 send_command，token=" << token);
        QJsonObject response = send_command(cmdWithToken, timeout);

        QMetaObject::invokeMethod(this, [this, token, response]() {
            LOG_MODULE("PythonSubprocessManager", "call_lambda_callback", LOG_DEBUG,
                "回到主线程，处理 token=" << token << " 的回调");
            std::function<void(const QJsonObject&)> cb;
            {
                QMutexLocker locker(&m_callbackMutex);
                auto it = m_pendingCallbacks.find(token);
                if (it != m_pendingCallbacks.end()) {
                    cb = it.value();
                    m_pendingCallbacks.erase(it);
                }
            }
            if (cb) {
                cb(response);
            }
            }, Qt::QueuedConnection);
        });
}

void PythonSubprocessManager::on_process_started() {
    LOG_MODULE("PythonSubprocessManager", "on_process_started", LOG_INFO, "Python 进程已启动");
    connect(m_process, &QProcess::readyReadStandardOutput, [this]() {
        QByteArray data = m_process->readAllStandardOutput();
        parse_port_from_output(data);
        });
}

void PythonSubprocessManager::on_process_error(QProcess::ProcessError error) {
    QString msg = QString("Python进程错误: %1").arg(error);
    LOG_MODULE("PythonSubprocessManager", "on_process_error", LOG_ERROR, "进程错误: " << error);
    emit started(false, msg);
}

void PythonSubprocessManager::on_process_finished(int exitCode, QProcess::ExitStatus status) {
    LOG_MODULE("PythonSubprocessManager", "on_process_finished", LOG_INFO,
        "Python 进程结束，exitCode=" << exitCode << "，status=" << (status == QProcess::NormalExit ? "Normal" : "Crash"));
    emit finished();
}

void PythonSubprocessManager::on_socket_connected() {
    LOG_MODULE("PythonSubprocessManager", "on_socket_connected", LOG_INFO,
        "TCP socket 已连接到端口 " << m_port);
    emit started(true, QString());
}

void PythonSubprocessManager::on_socket_error(QTcpSocket::SocketError error) {
    LOG_MODULE("PythonSubprocessManager", "on_socket_error", LOG_ERROR,
        "Socket 错误: " << m_socket->errorString().toStdString() << " (error=" << error << ")");
    emit started(false, m_socket->errorString());
}

void PythonSubprocessManager::on_socket_ready_read() {
    while (m_socket->canReadLine()) {
        QByteArray line = m_socket->readLine();
        std::string line_str = DebugLogUtil::remove_newline(line.toStdString());
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(line, &err);
        if (err.error != QJsonParseError::NoError) {
            LOG_MODULE("PythonSubprocessManager", "on_socket_ready_read", LOG_WARN,
                "JSON 解析错误: " << err.errorString().toStdString() << "，原始数据: " << line_str);
            continue;
        }
        QJsonObject resp = doc.object();

        int token = resp.value("req_id").toInt(0);
        LOG_MODULE("PythonSubprocessManager", "on_socket_ready_read", LOG_DEBUG,
            "收到响应，token=" << token << "，内容: " << line_str);

        {
            QMutexLocker locker(&m_mutex);
            m_lastResponse = resp;
            m_responseReceived = true;
            m_waitCond.wakeAll();
        }

        emit command_response(token, resp);
    }
}

void PythonSubprocessManager::parse_port_from_output(const QByteArray& data) {
    if (m_port != 0) {
        return;
    }

    QString output = QString::fromUtf8(data).trimmed();
    bool ok;
    int port = output.toInt(&ok);
    if (ok && port > 0 && port < 65536) {
        m_port = port;
        LOG_MODULE("PythonSubprocessManager", "parse_port_from_output", LOG_INFO,
            "从输出解析到 TCP 端口: " << port);
        m_socket->connectToHost(QHostAddress::LocalHost, m_port);
    }
    else {
        LOG_MODULE("PythonSubprocessManager", "parse_port_from_output", LOG_DEBUG,
            "输出内容不是有效端口号，忽略: " << output.toStdString());
    }
}

void PythonSubprocessManager::send_json(const QJsonObject& obj) {
    QByteArray data = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    data.append('\n');
    LOG_MODULE("PythonSubprocessManager", "send_json", LOG_DEBUG,
        "发送 JSON: " << DebugLogUtil::remove_newline(data.toStdString()));
    m_socket->write(data);
    m_socket->flush();
}

QJsonObject PythonSubprocessManager::send_command(const QJsonObject& cmd, int timeout) {
    QMutexLocker locker(&m_mutex);
    m_responseReceived = false;
    m_lastResponse = QJsonObject();

    std::string message = DebugLogUtil::remove_newline(QJsonDocument(cmd).toJson().toStdString());
    LOG_MODULE("PythonSubprocessManager", "send_command", LOG_DEBUG,
        "发送命令，等待响应，超时=" << timeout << "ms，命令: " << message);

    bool sent = false;
    QMetaObject::invokeMethod(this, [this, cmd, &sent]() {
        send_json(cmd);
        sent = true;
        }, Qt::BlockingQueuedConnection);

    // 等待响应或超时，同时响应停止标志
    while (!m_responseReceived && !m_stopping) {
        if (!m_waitCond.wait(&m_mutex, timeout)) {
            // 超时或停止
            if (m_stopping) {
                LOG_MODULE("PythonSubprocessManager", "send_command", LOG_DEBUG,
                    "对象正在销毁，停止等待");
                return { {"status", "error"}, {"message", "对象正在销毁"} };
            }
            LOG_MODULE("PythonSubprocessManager", "send_command", LOG_WARN,
                "等待响应超时 (" << timeout << "ms)");
            return { {"status", "error"}, {"message", "响应超时"} };
        }
    }

    if (m_stopping) {
        LOG_MODULE("PythonSubprocessManager", "send_command", LOG_DEBUG, "对象正在销毁，返回错误");
        return { {"status", "error"}, {"message", "对象正在销毁"} };
    }

    std::string respond_str = DebugLogUtil::remove_newline(QJsonDocument(m_lastResponse).toJson().toStdString());
    LOG_MODULE("PythonSubprocessManager", "send_command", LOG_DEBUG,
        "收到响应: " << respond_str);
    return m_lastResponse;
}
