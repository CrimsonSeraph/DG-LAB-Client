#include "PythonSubprocessManager.h"

#include "DebugLog.h"

#include <QCoreApplication>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTcpSocket>
#include <QThreadPool>
#include <QTimer>

#include <iostream>

PythonSubprocessManager::PythonSubprocessManager(QObject* parent)
    : QObject(parent)
    , process_(new QProcess(this))
    , socket_(new QTcpSocket(this))
    , port_(0)
    , response_received_(false) {
    LOG_MODULE("PythonSubprocessManager", "PythonSubprocessManager", LOG_INFO, "创建 PythonSubprocessManager 对象");
    connect(process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        this, &PythonSubprocessManager::on_process_finished);
    connect(process_, &QProcess::errorOccurred, this, &PythonSubprocessManager::on_process_error);
    connect(process_, &QProcess::started, this, &PythonSubprocessManager::on_process_started);
    connect(socket_, &QTcpSocket::connected, this, &PythonSubprocessManager::on_socket_connected);
    connect(socket_, &QTcpSocket::errorOccurred, this, &PythonSubprocessManager::on_socket_error);
    connect(socket_, &QTcpSocket::readyRead, this, &PythonSubprocessManager::on_socket_ready_read);
    connect(process_, &QProcess::readyReadStandardOutput, this, &PythonSubprocessManager::handle_stdout);
    connect(process_, &QProcess::readyReadStandardError, this, &PythonSubprocessManager::handle_stderr);
}

PythonSubprocessManager::~PythonSubprocessManager() {
    LOG_MODULE("PythonSubprocessManager", "~PythonSubprocessManager", LOG_INFO, "析构，清理资源");

    // 通知所有等待线程退出
    stopping_ = true;
    {
        QMutexLocker locker(&mutex_);
        wait_cond_.wakeAll();   // 唤醒所有等待 send_command 的线程
    }

    // 断开所有来自子对象的信号连接，避免在析构过程中触发槽函数
    LOG_MODULE("PythonSubprocessManager", "~PythonSubprocessManager", LOG_DEBUG, "开始断开信号连接");
    if (process_) {
        process_->disconnect(this);
    }
    if (socket_) {
        socket_->disconnect(this);
    }

    // 等待线程池中所有后台任务完成（避免访问已销毁的成员）
    LOG_MODULE("PythonSubprocessManager", "~PythonSubprocessManager", LOG_DEBUG, "等待线程池任务结束...");
    QThreadPool::globalInstance()->waitForDone(3000);

    // 终止子进程
    if (process_->state() != QProcess::NotRunning) {
        LOG_MODULE("PythonSubprocessManager", "~PythonSubprocessManager", LOG_DEBUG, "终止 Python 进程");
        process_->terminate();
        if (!process_->waitForFinished(2000)) {
            process_->kill();
            process_->waitForFinished(1000);
        }
    }

    // 断开 socket 连接
    if (socket_->state() == QTcpSocket::ConnectedState) {
        socket_->disconnectFromHost();
        socket_->waitForDisconnected(1000);
    }
}

void PythonSubprocessManager::start_process(const QString& python_executable, const QString& script_path) {
    port_ = 0;

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PYTHONIOENCODING", "utf-8");
    process_->setProcessEnvironment(env);

    QStringList args;
    args << script_path;
    QString correct_path = python_executable;
    process_->start(correct_path, args);
    if (!process_->waitForStarted(5000)) {
        LOG_MODULE("PythonSubprocessManager", "start_process", LOG_ERROR, "启动 Python 进程失败（超时）");
        emit started(false, "无法启动Python进程");
    }
    else {
        LOG_MODULE("PythonSubprocessManager", "start_process", LOG_DEBUG, "waitForStarted 成功，等待进程真正启动信号");
    }
}

void PythonSubprocessManager::call(const QJsonObject& cmd, std::function<void(const QJsonObject&)> callback, int timeout) {
    if (stopping_) {
        LOG_MODULE("PythonSubprocessManager", "call", LOG_WARN, "对象正在销毁，忽略新调用");
        if (callback) {
            callback({ {"status", "error"}, {"message", "对象正在销毁"} });
        }
        return;
    }

    int token = ++next_token_;

    {
        QMutexLocker locker(&callback_mutex_);
        pending_callbacks_[token] = callback;
    }

    QJsonObject cmd_with_token = cmd;
    cmd_with_token["req_id"] = token;

    std::string cmd_str = DebugLogUtil::remove_newline(QJsonDocument(cmd_with_token).toJson().toStdString());
    LOG_MODULE("PythonSubprocessManager", "call", LOG_DEBUG,
        "异步调用，token=" << token << "，命令: " << cmd_str);

    QThreadPool::globalInstance()->start([this, cmd_with_token, timeout, token]() {
        if (stopping_) {
            LOG_MODULE("PythonSubprocessManager", "call_lambda", LOG_DEBUG,
                "对象正在销毁，后台任务提前退出，token=" << token);
            return;
        }

        LOG_MODULE("PythonSubprocessManager", "call_lambda", LOG_DEBUG,
            "后台线程开始执行 send_command，token=" << token);
        QJsonObject response = send_command(cmd_with_token, timeout);

        QMetaObject::invokeMethod(this, [this, token, response]() {
            LOG_MODULE("PythonSubprocessManager", "call_lambda_callback", LOG_DEBUG,
                "回到主线程，处理 token=" << token << " 的回调");
            std::function<void(const QJsonObject&)> cb;
            {
                QMutexLocker locker(&callback_mutex_);
                auto it = pending_callbacks_.find(token);
                if (it != pending_callbacks_.end()) {
                    cb = it.value();
                    pending_callbacks_.erase(it);
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
}

void PythonSubprocessManager::on_process_error(QProcess::ProcessError error) {
    QString msg = QString("Python进程错误: %1").arg(error);
    LOG_MODULE("PythonSubprocessManager", "on_process_error", LOG_ERROR, "进程错误: " << error);
    emit started(false, msg);
}

void PythonSubprocessManager::on_process_finished(int exit_code, QProcess::ExitStatus status) {
    LOG_MODULE("PythonSubprocessManager", "on_process_finished", LOG_INFO,
        "Python 进程结束，exitCode=" << exit_code << "，status=" << (status == QProcess::NormalExit ? "Normal" : "Crash"));
    emit finished();
}

void PythonSubprocessManager::on_socket_connected() {
    LOG_MODULE("PythonSubprocessManager", "on_socket_connected", LOG_INFO,
        "TCP socket 已连接到端口 " << port_);
    emit started(true, QString());
}

void PythonSubprocessManager::on_socket_error(QTcpSocket::SocketError error) {
    LOG_MODULE("PythonSubprocessManager", "on_socket_error", LOG_ERROR,
        "Socket 错误: " << socket_->errorString().toStdString() << " (error=" << error << ")");
    emit started(false, socket_->errorString());
}

void PythonSubprocessManager::on_socket_ready_read() {
    while (socket_->canReadLine()) {
        QByteArray line = socket_->readLine();
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
            QMutexLocker locker(&mutex_);
            last_response_ = resp;
            response_received_ = true;
            wait_cond_.wakeAll();
        }

        emit command_response(token, resp);
    }
}

void PythonSubprocessManager::handle_stdout() {
    QByteArray data = process_->readAllStandardOutput();
    parse_port_from_output(data);
    process_output(data, false);
}

void PythonSubprocessManager::handle_stderr() {
    QByteArray data = process_->readAllStandardError();
    process_output(data, true);
}

void PythonSubprocessManager::process_output(const QByteArray& data, bool is_error) {
    QList<QByteArray> lines = data.split('\n');
    for (const QByteArray& line : lines) {
        if (line.isEmpty()) continue;
        QString line_str = QString::fromUtf8(line);

        LogLevel log_level = is_error ? LOG_ERROR : LOG_INFO;
        QString message = line_str;

        QRegularExpression re(R"(^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2},\d{3} - (DEBUG|INFO|WARNING|ERROR) - )");
        QRegularExpressionMatch match = re.match(line_str);
        if (match.hasMatch()) {
            QString level_str = match.captured(1);
            if (level_str == "DEBUG") log_level = LOG_DEBUG;
            else if (level_str == "INFO") log_level = LOG_INFO;
            else if (level_str == "WARNING") log_level = LOG_WARN;
            else if (level_str == "ERROR") log_level = LOG_ERROR;

            message = line_str.mid(match.capturedLength());
        }
        else {
            message = "[Raw] " + message;
        }

        LOG_MODULE("Python", is_error ? "stderr" : "stdout", log_level, message.toUtf8().constData());
    }
}

void PythonSubprocessManager::parse_port_from_output(const QByteArray& data) {
    if (port_ != 0) {
        return;
    }

    QString output = QString::fromUtf8(data).trimmed();
    bool ok;
    int port = output.toInt(&ok);
    if (ok && port > 0 && port < 65536) {
        port_ = port;
        LOG_MODULE("PythonSubprocessManager", "parse_port_from_output", LOG_INFO,
            "从输出解析到 TCP 端口: " << port);
        socket_->connectToHost(QHostAddress::LocalHost, port_);
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
    socket_->write(data);
    socket_->flush();
}

QJsonObject PythonSubprocessManager::send_command(const QJsonObject& cmd, int timeout) {
    QMutexLocker locker(&mutex_);
    response_received_ = false;
    last_response_ = QJsonObject();

    std::string message = DebugLogUtil::remove_newline(QJsonDocument(cmd).toJson().toStdString());
    LOG_MODULE("PythonSubprocessManager", "send_command", LOG_DEBUG,
        "发送命令，等待响应，超时=" << timeout << "ms，命令: " << message);

    bool sent = false;
    QMetaObject::invokeMethod(this, [this, cmd, &sent]() {
        send_json(cmd);
        sent = true;
        }, Qt::BlockingQueuedConnection);
    if (!sent) {
        return { {"status", "error"}, {"message", "发送命令失败"} };
    }
    // 等待响应或超时，同时响应停止标志
    while (!response_received_ && !stopping_) {
        if (!wait_cond_.wait(&mutex_, timeout)) {
            // 超时或停止
            if (stopping_) {
                LOG_MODULE("PythonSubprocessManager", "send_command", LOG_DEBUG,
                    "对象正在销毁，停止等待");
                return { {"status", "error"}, {"message", "对象正在销毁"} };
            }
            LOG_MODULE("PythonSubprocessManager", "send_command", LOG_WARN,
                "等待响应超时 (" << timeout << "ms)");
            return { {"status", "error"}, {"message", "响应超时"} };
        }
    }

    if (stopping_) {
        LOG_MODULE("PythonSubprocessManager", "send_command", LOG_DEBUG, "对象正在销毁，返回错误");
        return { {"status", "error"}, {"message", "对象正在销毁"} };
    }

    std::string respond_str = DebugLogUtil::remove_newline(QJsonDocument(last_response_).toJson().toStdString());
    LOG_MODULE("PythonSubprocessManager", "send_command", LOG_DEBUG,
        "收到响应: " << respond_str);
    return last_response_;
}
