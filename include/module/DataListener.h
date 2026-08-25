/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QJsonObject>
#include <QString>
#include <QTcpServer>

#include <functional>
#include <map>
#include <utility>

class QUdpSocket;

// ============================================
// IDataParser - 数据解析器抽象接口
// 将收到的原始字节解析为 JSON 对象（传输协议与 JSON 解析解耦点）
// ============================================
class IDataParser {
public:
    virtual ~IDataParser() = default;

    /// @brief 解析原始数据为 JSON 对象
    /// @param raw 原始字节
    /// @param ok 输出：解析是否成功
    /// @return 解析结果（失败时返回空对象）
    virtual QJsonObject parse(const QByteArray& raw, bool* ok) const = 0;
};

// ============================================
// HttpJsonParser - HTTP POST JSON 解析器（默认，兼容原 GSI 逻辑）
// 提取 Content-Length 指定的请求体并解析为 JSON
// ============================================
class HttpJsonParser : public IDataParser {
public:
    /// @brief 解析 HTTP 请求：提取请求体并解析 JSON
    /// @param raw HTTP 请求原始字节
    /// @param ok 输出：解析是否成功
    /// @return 解析结果（失败时返回空对象）
    QJsonObject parse(const QByteArray& raw, bool* ok) const override;
};

// ============================================
// JsonBodyParser - 直接 JSON 解析器（用于 UDP 数据报等无 HTTP 头场景）
// ============================================
class JsonBodyParser : public IDataParser {
public:
    /// @brief 直接解析 JSON 对象
    /// @param raw 原始字节（应为完整 JSON 文本）
    /// @param ok 输出：解析是否成功
    /// @return 解析结果（失败时返回空对象）
    QJsonObject parse(const QByteArray& raw, bool* ok) const override;
};

// ============================================
// DataListener - 通用数据接收器
// 单实例监听单个端口（TCP HTTP POST 或 UDP 数据报），解析为 JSON 后
// 按数据包携带的来源标识（source + type）分发给已注册的处理器。
// 数据包信封格式: {"source": "<模块名>", "type": "<信息类型>", "data": {...}}；
// 无信封字段时回退到默认来源（set_default_source / set_default_type）。
// 注意：本类面向单线程事件循环使用，处理器注册表不做线程安全保护。
// ============================================
class DataListener : public QTcpServer {
    Q_OBJECT

public:
    // -------------------- 监听协议枚举 --------------------
    enum class Protocol {
        HTTP, ///< TCP HTTP POST 请求体（默认，兼容原 GSI）
        UDP   ///< UDP 数据报（每包一个 JSON）
    };

    /// @brief 数据处理器类型：收到并识别来源后的回调
    using DataHandler = std::function<void(const QJsonObject&)>;

    // -------------------- 构造/析构 --------------------
    explicit DataListener(QObject* parent = nullptr);

    // -------------------- 监听控制 --------------------
    /// @brief 开始监听指定端口
    /// @param port 监听端口
    /// @param protocol 监听协议（HTTP 或 UDP，默认 HTTP）
    /// @return 成功返回 true（端口被占用返回 false）
    bool start_listening(int port, Protocol protocol = Protocol::HTTP);

    /// @brief 停止监听
    void stop_listening();

    /// @brief 获取当前监听端口
    /// @return 端口号（未监听返回 0）
    inline int port() const { return port_; }

    /// @brief 是否正在监听
    /// @return 监听中返回 true
    inline bool is_listening() const { return is_listening_; }

    // -------------------- 来源默认值 --------------------
    /// @brief 设置默认来源标识（数据包无信封字段时使用）
    /// @param source 模块名（如 "CS2 GSI"）
    void set_default_source(const QString& source);

    /// @brief 设置默认信息类型（数据包无信封字段时使用）
    /// @param type 信息类型（如 "gsi"）
    void set_default_type(const QString& type);

    /// @brief 获取默认来源标识
    /// @return 默认来源（未设置返回空字符串）
    inline const QString& default_source() const { return default_source_; }

    /// @brief 获取默认信息类型
    /// @return 默认类型（未设置返回空字符串）
    inline const QString& default_type() const { return default_type_; }

    // -------------------- 处理器注册 --------------------
    /// @brief 注册数据处理处理器（同一来源+类型重复注册时覆盖）
    /// @param source 来源标识（模块名）
    /// @param type 信息类型
    /// @param handler 数据处理器（收到匹配数据时调用）
    void register_handler(const QString& source, const QString& type, DataHandler handler);

    /// @brief 注销数据处理处理器
    /// @param source 来源标识（模块名）
    /// @param type 信息类型
    void unregister_handler(const QString& source, const QString& type);

signals:
    /// @brief 收到并解析出数据时发出（兼容原 GsiServer 信号）
    /// @param data 解析后的完整数据
    void data_received(const QJsonObject& data);

    /// @brief 数据包来源识别完成时发出
    /// @param source 来源标识（模块名）
    /// @param type 信息类型
    /// @param data 分发给处理器的数据对象
    void packet_received(const QString& source, const QString& type, const QJsonObject& data);

protected:
    /// @brief 重写新连接处理：读取请求体并交给解析器与分发逻辑
    /// @param socketDescriptor 新连接 socket 描述符
    void incomingConnection(qintptr socketDescriptor) override;

private slots:
    /// @brief UDP 数据报到达时解析并分发
    void on_udp_ready_read();

private:
    // -------------------- 私有辅助函数 --------------------
    /// @brief 解析原始数据、识别来源并分发（HTTP 与 UDP 共用入口）
    /// @param raw 原始字节
    void handle_raw_data(const QByteArray& raw);

    /// @brief 完成单个 HTTP 请求：解析分发并返回 200 响应（收齐请求体后调用）
    /// @param socket 请求 socket
    /// @param request 完整请求数据
    void finish_http_request(QTcpSocket* socket, const QByteArray& request);

    /// @brief 按来源与类型分发数据给已注册的处理器
    /// @param source 来源标识
    /// @param type 信息类型
    /// @param data 数据对象
    void dispatch(const QString& source, const QString& type, const QJsonObject& data);

    // -------------------- 成员变量 --------------------
    int port_ = 0;                                                ///< 监听端口
    bool is_listening_ = false;                                   ///< 监听标志
    Protocol protocol_ = Protocol::HTTP;                          ///< 当前监听协议
    QUdpSocket* udp_socket_ = nullptr;                            ///< UDP 套接字（UDP 协议时使用）
    QString default_source_;                                      ///< 默认来源标识
    QString default_type_;                                        ///< 默认信息类型
    IDataParser* parser_ = nullptr;                               ///< 当前数据解析器
    HttpJsonParser http_parser_;                                  ///< HTTP 解析器（HTTP 协议使用）
    JsonBodyParser json_parser_;                                  ///< 直接 JSON 解析器（UDP 协议使用）
    std::map<std::pair<QString, QString>, DataHandler> handlers_; ///< 来源+类型 -> 处理器
};
