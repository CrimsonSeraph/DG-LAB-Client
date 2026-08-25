/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "DataListener.h"

#include "DebugLog.h"

#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QTcpSocket>
#include <QUdpSocket>

#include <memory>

// ============================================
// HttpJsonParser - HTTP POST JSON 解析器（public）
// ============================================

QJsonObject HttpJsonParser::parse(const QByteArray& raw, bool* ok) const {
    if (ok) {
        *ok = false;
    }
    // 提取请求体：Content-Length 指定 body 长度，body 在空行之后
    QByteArray body;
    int header_end = raw.indexOf("\r\n\r\n");
    if (header_end >= 0) {
        QByteArray header = raw.left(header_end);
        body = raw.mid(header_end + 4);
        // 按 Content-Length 截取（避免粘包）
        QRegularExpression length_re(R"(Content-Length:\s*(\d+))",
            QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch match = length_re.match(QString::fromLatin1(header));
        if (match.hasMatch()) {
            int length = match.captured(1).toInt();
            if (body.size() > length) {
                body = body.left(length);
            }
        }
    }
    else if (!raw.isEmpty()) {
        // 无标准头时直接尝试解析整个内容
        body = raw;
    }
    if (body.isEmpty()) {
        return QJsonObject();
    }
    QJsonParseError parse_error;
    QJsonDocument doc = QJsonDocument::fromJson(body, &parse_error);
    if (parse_error.error == QJsonParseError::NoError && doc.isObject()) {
        if (ok) {
            *ok = true;
        }
        return doc.object();
    }
    return QJsonObject();
}

// ============================================
// JsonBodyParser - 直接 JSON 解析器（public）
// ============================================

QJsonObject JsonBodyParser::parse(const QByteArray& raw, bool* ok) const {
    if (ok) {
        *ok = false;
    }
    if (raw.isEmpty()) {
        return QJsonObject();
    }
    QJsonParseError parse_error;
    QJsonDocument doc = QJsonDocument::fromJson(raw, &parse_error);
    if (parse_error.error == QJsonParseError::NoError && doc.isObject()) {
        if (ok) {
            *ok = true;
        }
        return doc.object();
    }
    return QJsonObject();
}

// ============================================
// 构造/析构（public）
// ============================================

DataListener::DataListener(QObject* parent)
    : QTcpServer(parent) {
}

// ============================================
// 监听控制（public）
// ============================================

bool DataListener::start_listening(int port, Protocol protocol) {
    stop_listening();
    if (port <= 0) {
        return false;
    }
    protocol_ = protocol;
    if (protocol_ == Protocol::UDP) {
        // UDP 协议：创建数据报套接字并绑定本地端口
        udp_socket_ = new QUdpSocket(this);
        connect(udp_socket_, &QUdpSocket::readyRead, this, &DataListener::on_udp_ready_read);
        if (!udp_socket_->bind(QHostAddress::LocalHost, port)) {
            LOG_MODULE("DataListener", "start_listening", LOG_WARN,
                "UDP 端口监听失败（可能被占用）: " << port);
            delete udp_socket_;
            udp_socket_ = nullptr;
            return false;
        }
    }
    else {
        // HTTP 协议：QTcpServer 监听（兼容原 GSI 行为）
        if (!listen(QHostAddress::LocalHost, port)) {
            LOG_MODULE("DataListener", "start_listening", LOG_WARN,
                "TCP 端口监听失败（可能被占用）: " << port);
            return false;
        }
    }
    // 按协议选择解析器
    parser_ = (protocol_ == Protocol::UDP) ? static_cast<IDataParser*>(&json_parser_)
                                           : static_cast<IDataParser*>(&http_parser_);
    port_ = port;
    is_listening_ = true;
    LOG_MODULE("DataListener", "start_listening", LOG_INFO,
        "数据监听已启动，协议: " << (protocol_ == Protocol::UDP ? "UDP" : "HTTP")
                                 << "，端口: " << port);
    return true;
}

void DataListener::stop_listening() {
    if (!is_listening_) {
        return;
    }
    if (protocol_ == Protocol::UDP) {
        // UDP：关闭并释放数据报套接字
        if (udp_socket_) {
            udp_socket_->close();
            udp_socket_->deleteLater();
            udp_socket_ = nullptr;
        }
    }
    else {
        close();
    }
    port_ = 0;
    is_listening_ = false;
    LOG_MODULE("DataListener", "stop_listening", LOG_DEBUG, "数据监听已停止");
}

// ============================================
// 来源默认值（public）
// ============================================

void DataListener::set_default_source(const QString& source) {
    default_source_ = source;
}

void DataListener::set_default_type(const QString& type) {
    default_type_ = type;
}

// ============================================
// 处理器注册（public）
// ============================================

void DataListener::register_handler(const QString& source, const QString& type,
    DataHandler handler) {
    if (!handler) {
        return;
    }
    handlers_[std::make_pair(source, type)] = std::move(handler);
    LOG_MODULE("DataListener", "register_handler", LOG_DEBUG,
        "已注册数据处理器: " << source.toStdString() << " / " << type.toStdString());
}

void DataListener::unregister_handler(const QString& source, const QString& type) {
    auto it = handlers_.find(std::make_pair(source, type));
    if (it != handlers_.end()) {
        handlers_.erase(it);
        LOG_MODULE("DataListener", "unregister_handler", LOG_DEBUG,
            "已注销数据处理器: " << source.toStdString() << " / " << type.toStdString());
    }
}

// ============================================
// 重写事件（protected）
// ============================================

void DataListener::incomingConnection(qintptr socketDescriptor) {
    QTcpSocket* socket = new QTcpSocket(this);
    if (!socket->setSocketDescriptor(socketDescriptor)) {
        delete socket;
        return;
    }
    // 按连接累积请求体：HTTP 头完整后按 Content-Length 判断是否收齐（避免分包半包）
    auto buffer = std::make_shared<QByteArray>();
    connect(socket, &QTcpSocket::readyRead, this, [this, socket, buffer]() {
        buffer->append(socket->readAll());
        // 防御：数据异常过大时直接处理（按无头 JSON 尝试解析，避免无限缓冲）
        if (buffer->size() > 65536) {
            finish_http_request(socket, *buffer);
            return;
        }
        // HTTP 头未完整：继续等待后续数据
        int header_end = buffer->indexOf("\r\n\r\n");
        if (header_end < 0) {
            return;
        }
        // 有完整头：按 Content-Length 判断 body 是否收齐
        const QByteArray header = buffer->left(header_end);
        QRegularExpression length_re(R"(Content-Length:\s*(\d+))",
            QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch match = length_re.match(QString::fromLatin1(header));
        if (match.hasMatch()) {
            int length = match.captured(1).toInt();
            if (buffer->size() < header_end + 4 + length) {
                // body 未收齐：继续等待
                return;
            }
        }
        finish_http_request(socket, *buffer);
    });
    connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
}

void DataListener::finish_http_request(QTcpSocket* socket, const QByteArray& request) {
    // 交给解析器与分发逻辑（HTTP 解析器提取请求体并解析 JSON）
    handle_raw_data(request);
    // 返回 HTTP 200 响应
    const QByteArray response =
        "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    socket->write(response);
    socket->flush();
    socket->disconnectFromHost();
    socket->deleteLater();
}

// ============================================
// private slots 实现
// ============================================

void DataListener::on_udp_ready_read() {
    // 逐包读取 UDP 数据报（每包一个 JSON 对象）
    while (udp_socket_ && udp_socket_->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(static_cast<int>(udp_socket_->pendingDatagramSize()));
        udp_socket_->readDatagram(datagram.data(), datagram.size());
        handle_raw_data(datagram);
    }
}

// ============================================
// 私有辅助函数实现（private）
// ============================================

void DataListener::handle_raw_data(const QByteArray& raw) {
    if (raw.isEmpty()) {
        return;
    }
    bool ok = false;
    QJsonObject obj = parser_ ? parser_->parse(raw, &ok) : QJsonObject();
    if (!ok) {
        LOG_MODULE("DataListener", "handle_raw_data", LOG_WARN,
            "数据解析失败: " << raw.left(200).constData());
        return;
    }
    // 识别数据包来源：优先信封格式 {"source","type","data"}，否则使用默认来源
    QString source = default_source_;
    QString type = default_type_;
    QJsonObject payload = obj;
    if (obj.contains("source") && obj["source"].isString() && obj.contains("type") && obj["type"].isString() && obj.contains("data") && obj["data"].isObject()) {
        source = obj["source"].toString();
        type = obj["type"].toString();
        payload = obj["data"].toObject();
    }
    emit data_received(obj);
    emit packet_received(source, type, payload);
    dispatch(source, type, payload);
}

void DataListener::dispatch(const QString& source, const QString& type,
    const QJsonObject& data) {
    auto it = handlers_.find(std::make_pair(source, type));
    if (it == handlers_.end()) {
        LOG_MODULE("DataListener", "dispatch", LOG_WARN,
            "无匹配的数据处理器，来源: " << source.toStdString()
                                         << "，类型: " << type.toStdString());
        return;
    }
    it->second(data);
}
