/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "GsiServer.h"

#include "DebugLog.h"

#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QTcpSocket>

// ============================================
// 构造/析构（public）
// ============================================

GsiServer::GsiServer(QObject* parent)
    : QTcpServer(parent) {
}

// ============================================
// 公共接口（public）
// ============================================

bool GsiServer::start_listening(int port) {
    stop_listening();
    if (port <= 0) {
        return false;
    }
    if (!listen(QHostAddress::LocalHost, port)) {
        LOG_MODULE("GsiServer", "start_listening", LOG_WARN,
            "端口监听失败（可能被占用）: " << port);
        return false;
    }
    port_ = port;
    is_listening_ = true;
    LOG_MODULE("GsiServer", "start_listening", LOG_INFO,
        "GSI 监听服务器已启动，端口: " << port);
    return true;
}

void GsiServer::stop_listening() {
    if (is_listening_) {
        close();
        port_ = 0;
        is_listening_ = false;
        LOG_MODULE("GsiServer", "stop_listening", LOG_DEBUG, "GSI 监听服务器已停止");
    }
}

// ============================================
// 重写事件（protected）
// ============================================

void GsiServer::incomingConnection(qintptr socketDescriptor) {
    QTcpSocket* socket = new QTcpSocket(this);
    if (!socket->setSocketDescriptor(socketDescriptor)) {
        delete socket;
        return;
    }
    // 接收 HTTP POST 请求体（CS2 GSI 每次发送一个 JSON 数据块）
    connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
        QByteArray request = socket->readAll();
        // 提取请求体：Content-Length 指定 body 长度，body 在空行之后
        QByteArray body;
        int headerEnd = request.indexOf("\r\n\r\n");
        if (headerEnd >= 0) {
            QByteArray header = request.left(headerEnd);
            body = request.mid(headerEnd + 4);
            // 按 Content-Length 截取（避免粘包）
            QRegularExpression length_re("Content-Length:\s*(\d+)",
                QRegularExpression::CaseInsensitiveOption);
            QRegularExpressionMatch match = length_re.match(QString::fromLatin1(header));
            if (match.hasMatch()) {
                int length = match.captured(1).toInt();
                if (body.size() > length) {
                    body = body.left(length);
                }
            }
        }
        else if (!request.isEmpty()) {
            // 无标准头时直接尝试解析整个内容
            body = request;
        }

        if (!body.isEmpty()) {
            QJsonParseError parse_error;
            QJsonDocument doc = QJsonDocument::fromJson(body, &parse_error);
            if (parse_error.error == QJsonParseError::NoError && doc.isObject()) {
                emit data_received(doc.object());
                LOG_MODULE("GsiServer", "incomingConnection", LOG_DEBUG,
                    "收到 GSI 数据，大小: " << body.size());
            }
            else {
                LOG_MODULE("GsiServer", "incomingConnection", LOG_WARN,
                    "GSI 数据解析失败: " << parse_error.errorString().toStdString()
                    << "，请求体: " << body.left(200).constData());
            }
        }
        // 返回 HTTP 200 响应
        const QByteArray response =
            "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        socket->write(response);
        socket->flush();
        socket->disconnectFromHost();
        socket->deleteLater();
    });
    connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
}
