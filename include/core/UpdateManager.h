#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

class QNetworkReply;
class QJsonObject;

class UpdateManager : public QObject {
    Q_OBJECT

public:
    /// @brief 构造更新管理器
    /// @param parent 父对象
    explicit UpdateManager(QObject* parent = nullptr);

    /// @brief 析构更新管理器，中止未完成的下载
    ~UpdateManager() override;

    /// @brief 触发检查更新
    void check_for_updates();

    /// @brief 开始下载更新包
    void start_download();

    /// @brief 执行更新脚本并退出程序
    void apply_update_and_restart();

    /// @brief 是否存在可用更新
    /// @return 有可用更新返回 true
    bool has_update() const;

    /// @brief 获取远程版本号
    /// @return 远程版本号字符串
    QString remote_version() const;

    /// @brief 获取更新说明
    /// @return 更新说明文本
    QString release_notes() const;

    /// @brief 获取当前错误信息
    /// @return 错误信息字符串
    QString error_message() const;

    /// @brief 获取更新健康标记文件路径（脚本与主程序共用）
    /// @return 标记文件的绝对路径
    static QString update_flag_path();

private:
    QNetworkAccessManager* network_manager_; // 网络访问管理器
    QNetworkReply* current_download_reply_;  // 当前下载请求回复

    QString manifest_url_;       // 更新清单 URL
    QString remote_version_;     // 远程版本号
    QString release_notes_;      // 更新说明
    QString download_url_;       // 更新包下载 URL
    QString expected_sha256_;    // 更新包期望 SHA256
    QString download_file_path_; // 更新包本地临时路径
    QString current_error_;      // 当前错误信息

    /// @brief 处理更新清单请求响应
    /// @param reply 网络回复对象
    void on_manifest_reply(QNetworkReply* reply);

    /// @brief 下载数据可读时写入文件
    void on_download_ready_read();

    /// @brief 下载完成时校验并发出结果
    void on_download_finished();

    /// @brief 判断 remote 版本是否比 current 新
    /// @param current 当前版本
    /// @param remote 远程版本
    /// @return remote 更新时返回 true
    bool is_newer_version(const QString& current, const QString& remote) const;

    /// @brief 从平台信息对象中提取文件名与 SHA256
    /// @param platforms_obj 平台信息 JSON 对象
    /// @param out_file 输出文件名
    /// @param out_sha 输出 SHA256
    /// @return 提取成功返回 true
    bool extract_platform_info(const QJsonObject& platforms_obj, QString& out_file, QString& out_sha);

    /// @brief 校验文件 SHA256
    /// @param file_path 文件路径
    /// @param expected_sha 期望 SHA256
    /// @return 校验通过返回 true
    bool verify_sha256(const QString& file_path, const QString& expected_sha);

    /// @brief 生成 Windows 更新脚本
    /// @param installer_path 安装包路径
    /// @return 脚本路径
    QString generate_windows_script(const QString& installer_path);

    /// @brief 生成 macOS 更新脚本
    /// @param dmg_path DMG 路径
    /// @return 脚本路径
    QString generate_macos_script(const QString& dmg_path);

    /// @brief 生成 Linux 更新脚本
    /// @param appimage_path AppImage 路径
    /// @return 脚本路径
    QString generate_linux_script(const QString& appimage_path);

    /// @brief 执行脚本并退出程序
    /// @param script_path 脚本路径
    void execute_script_and_quit(const QString& script_path);

signals:
    /// @brief 检查更新完成
    /// @param available 是否有新版本
    void update_available(bool available);

    /// @brief 下载进度变化
    /// @param received 已接收字节数
    /// @param total 总字节数
    void download_progress(qint64 received, qint64 total);

    /// @brief 下载完成
    /// @param success 是否成功
    /// @param error 错误信息
    void download_finished(bool success, const QString& error);

    /// @brief 发生错误
    /// @param error 错误信息
    void error_occurred(const QString& error);
};
