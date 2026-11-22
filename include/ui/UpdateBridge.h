// Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QObject>
#include <QString>

class UpdateManager;

class UpdateBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool checking READ checking NOTIFY state_changed)                 // 是否正在检查更新
    Q_PROPERTY(bool downloading READ downloading NOTIFY state_changed)           // 是否正在下载
    Q_PROPERTY(bool ready_to_install READ ready_to_install NOTIFY state_changed) // 是否已准备好安装
    Q_PROPERTY(int progress READ progress NOTIFY progress_changed)               // 下载进度（0-100）
    Q_PROPERTY(QString remote_version READ remote_version NOTIFY state_changed)  // 远程版本号
    Q_PROPERTY(QString release_notes READ release_notes NOTIFY state_changed)    // 更新说明
    Q_PROPERTY(QString error_message READ error_message NOTIFY state_changed)    // 当前错误信息
    Q_PROPERTY(bool hasUpdate READ has_update NOTIFY state_changed)              // 是否存在可用更新
    Q_PROPERTY(QString release_url READ release_url NOTIFY state_changed)        // 最新版本的下载地址

public:
    /// @brief 构造更新桥接对象
    /// @param manager 更新管理器
    /// @param parent 父对象
    explicit UpdateBridge(UpdateManager* manager, QObject* parent = nullptr);

    /// @brief 是否正在检查更新
    /// @return 正在检查返回 true
    bool checking() const;

    /// @brief 是否正在下载
    /// @return 正在下载返回 true
    bool downloading() const;

    /// @brief 是否已准备好安装
    /// @return 准备好安装返回 true
    bool ready_to_install() const;

    /// @brief 获取下载进度
    /// @return 进度值（0-100）
    int progress() const;

    /// @brief 获取远程版本号
    /// @return 远程版本号字符串
    QString remote_version() const;

    /// @brief 获取更新说明
    /// @return 更新说明文本
    QString release_notes() const;

    /// @brief 获取当前错误信息
    /// @return 错误信息字符串
    QString error_message() const;

    /// @brief 获取最新 release 下载 url
    /// @return 最新 release 下载 url
    QString release_url() const;

    /// @brief 获取是否存在可用更新
    /// @return 是否存在可用更新
    bool has_update() const;

    /// @brief 在系统浏览器打开最新 Release 页面
    /// @note release_url 为空时静默返回，不弹提示
    Q_INVOKABLE void open_release_url();

    /// @brief 安排启动后的静默检查更新
    /// @note 距上次自动检查不足一天时直接跳过，只排一个延迟回调，不阻塞启动
    void schedule_startup_check();

public slots:
    /// @brief 用户点击检查更新
    void on_check_update_clicked();

    /// @brief 用户点击开始下载
    void on_start_download_clicked();

    /// @brief 用户点击应用更新
    void on_apply_update_clicked();

private:
    UpdateManager* manager_; // 更新管理器
    bool checking_;          // 是否正在检查更新
    bool downloading_;       // 是否正在下载
    bool silent_check_;      // 当前检查是否为启动静默检查（不弹失败提示）
    bool ready_to_install_;  // 是否已准备好安装
    int progress_;           // 下载进度（0-100）
    QString error_message_;  // 当前错误信息

signals:
    /// @brief 状态变化（checking / downloading / ready_to_install 等）
    void state_changed();

    /// @brief 进度变化
    void progress_changed();

    /// @brief 请求发送 Toast 提示
    /// @param message 提示内容
    /// @param level 提示级别
    void toast_requested(const QString& message, int level);

private slots:
    /// @brief 检查更新完成
    /// @param available 是否有新版本
    void on_update_available(bool available);

    /// @brief 下载进度变化
    /// @param received 已接收字节数
    /// @param total 总字节数
    void on_download_progress(qint64 received, qint64 total);

    /// @brief 下载完成
    /// @param success 是否成功
    /// @param error 错误信息
    void on_download_finished(bool success, const QString& error);

    /// @brief 发生错误
    /// @param error 错误信息
    void on_error_occurred(const QString& error);

    /// @brief 静默检查的延迟触发点
    void on_startup_check_timer();
};
