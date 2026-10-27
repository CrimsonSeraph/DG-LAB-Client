#include "UpdateManager.h"

#include "AppVersion.h"
#include "DebugLog.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QProcess>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <QVersionNumber>
#include <QtConcurrent>

UpdateManager::UpdateManager(QObject* parent)
    : QObject(parent)
    , network_manager_(new QNetworkAccessManager(this))
    , current_download_reply_(nullptr)
    , current_check_reply_(nullptr)
    , manifest_url_("https://github.com/CrimsonSeraph/DG-LAB-Client/releases/latest/download/manifest.json") {
}

UpdateManager::~UpdateManager() {
    if (current_check_reply_) {
        disconnect(current_check_reply_, nullptr, this, nullptr);
        current_check_reply_->abort();
        current_check_reply_ = nullptr;
    }
    if (current_download_reply_) {
        disconnect(current_download_reply_, nullptr, this, nullptr);
        current_download_reply_->abort();
        current_download_reply_ = nullptr;
    }
}

void UpdateManager::check_for_updates() {
    if (current_check_reply_ || current_download_reply_) return;
    if (!is_self_updatable()) {
        current_error_ = unsupported_update_hint();
        LOG_MODULE("UpdateManager", "check_for_updates", LOG_INFO, current_error_);
        emit error_occurred(current_error_);
        return;
    }
    LOG_MODULE("UpdateManager", "check_for_updates", LOG_INFO, "开始检查更新...");
    current_error_.clear();

    // 重置上一轮结果
    remote_version_.clear();
    release_notes_.clear();
    download_url_.clear();
    expected_sha256_.clear();
    download_file_path_.clear();
    release_url_.clear();

    QNetworkRequest req{QUrl(manifest_url_)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    req.setTransferTimeout(30'000);
#endif

    current_check_reply_ = network_manager_->get(req);
    connect(current_check_reply_, &QNetworkReply::finished, this, [this]() {
        QNetworkReply* reply = current_check_reply_;
        current_check_reply_ = nullptr;
        on_manifest_reply(reply);
    });
}

void UpdateManager::on_manifest_reply(QNetworkReply* reply) {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        current_error_ = reply->errorString();
        LOG_MODULE("UpdateManager", "on_manifest_reply", LOG_WARN, "获取更新清单失败: " + current_error_);
        emit error_occurred(current_error_);
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (!doc.isObject()) {
        current_error_ = "更新清单格式错误";
        emit error_occurred(current_error_);
        return;
    }

    QJsonObject obj = doc.object();
    remote_version_ = obj["version"].toString();
    release_notes_ = obj["release_notes"].toString();
    release_url_ = obj["release_url"].toString();

    if (remote_version_.isEmpty()) {
        current_error_ = "更新清单缺少 version 字段";
        emit error_occurred(current_error_);
        return;
    }
    if (!obj["platforms"].isObject()) {
        current_error_ = "更新清单缺少 platforms 字段";
        emit error_occurred(current_error_);
        return;
    }
    if (release_url_.isEmpty() &&
        (release_notes_.startsWith("http://") || release_notes_.startsWith("https://"))) {
        release_url_ = release_notes_;
        release_notes_ = "详细更新说明请点击下方链接查看";
    }

    if (!is_newer_version(APP_VERSION, remote_version_)) {
        LOG_MODULE("UpdateManager", "on_manifest_reply", LOG_INFO, "当前已是最新版本: " + remote_version_);
        emit update_available(false);
        return;
    }

    QString file_name, file_url;
    if (extract_platform_info(obj["platforms"].toObject(),
            file_name, file_url, expected_sha256_)) {
        if (!file_url.isEmpty()) {
            download_url_ = file_url;
        }
        else {
            QString base_url = manifest_url_.left(manifest_url_.lastIndexOf('/'));
            download_url_ = base_url + "/" + file_name;
        }
        emit update_available(true);
        LOG_MODULE("UpdateManager", "on_manifest_reply", LOG_INFO, "发现新版本: " + remote_version_);
    }
    else {
        current_error_ = "当前平台没有可用的更新包";
        emit error_occurred(current_error_);
    }
}

void UpdateManager::start_download() {
    download_write_error_ = false;
    if (download_url_.isEmpty()) return;
    if (current_download_reply_ != nullptr) return;

    LOG_MODULE("UpdateManager", "start_download", LOG_INFO, "开始下载: " + download_url_);

    QNetworkRequest req{QUrl(download_url_)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    // 临时文件路径
    download_file_path_ = QStandardPaths::writableLocation(QStandardPaths::TempLocation) +
                          "/dglab_update_" +
                          QString::number(QCoreApplication::applicationPid()) + "_" +
                          QFileInfo(download_url_).fileName();
    QFile::remove(download_file_path_);

    current_download_reply_ = network_manager_->get(req);
    connect(current_download_reply_, &QNetworkReply::readyRead, this, &UpdateManager::on_download_ready_read);
    connect(current_download_reply_, &QNetworkReply::downloadProgress, this, &UpdateManager::download_progress);
    connect(current_download_reply_, &QNetworkReply::finished, this, &UpdateManager::on_download_finished);
}

void UpdateManager::on_download_ready_read() {
    if (!current_download_reply_) return;
    QFile file(download_file_path_);
    if (file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        file.write(current_download_reply_->readAll());
    }
    else {
        const auto err = file.error();
        const auto desc = file.errorString();
        const auto name = file.fileName();
        download_write_error_ = true;
        current_error_ = "无法写入临时文件: " + file.errorString();
        current_download_reply_->abort();
        LOG_MODULE("UpdateManager", "on_download_ready_read", LOG_WARN,
            QString("以追加方式打开下载文件失败：%1，错误：%2（code=%3）")
                .arg(name)
                .arg(desc)
                .arg(static_cast<int>(err)));
    }
}

void UpdateManager::on_download_finished() {
    if (!current_download_reply_) return;
    QNetworkReply* reply = current_download_reply_;
    current_download_reply_ = nullptr;
    reply->deleteLater();

    if (download_write_error_) {
        emit download_finished(false, current_error_);
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        current_error_ = "下载失败: " + reply->errorString();
        emit download_finished(false, current_error_);
        return;
    }

    QFile f(download_file_path_);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Append)) {
        current_error_ = "无法写入临时文件: " + f.errorString();
        emit download_finished(false, current_error_);
        return;
    }
    f.write(reply->readAll());
    f.close();
    reply->deleteLater();

    // 异步校验
    const QString path = download_file_path_;
    const QString expected = expected_sha256_;
    auto* watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher]() {
        watcher->deleteLater();
        if (!watcher->result()) {
            current_error_ = "文件校验失败 (SHA256 不匹配)";
            QFile::remove(download_file_path_);
            emit download_finished(false, current_error_);
            return;
        }
        LOG_MODULE("UpdateManager", "on_download_finished", LOG_INFO, "下载完成并通过校验");
        emit download_finished(true, "");
    });
    watcher->setFuture(QtConcurrent::run(&UpdateManager::verify_sha256, path, expected));
}

void UpdateManager::apply_update_and_restart() {
    if (download_file_path_.isEmpty() ||
        !QFileInfo::exists(download_file_path_)) {
        current_error_ = "更新包不存在，请重新下载";
        download_file_path_.clear();
        emit download_finished(false, current_error_);
        return;
    }
    if (!verify_sha256(download_file_path_, expected_sha256_)) {
        current_error_ = "更新包已损坏，请重新下载";
        QFile::remove(download_file_path_);
        download_file_path_.clear();
        emit download_finished(false, current_error_);
        return;
    }

    QString script_path;
#ifdef Q_OS_WIN
    script_path = generate_windows_script(download_file_path_);
#elif defined(Q_OS_MACOS)
    script_path = generate_macos_script(download_file_path_);
#else
    script_path = generate_linux_script(download_file_path_);
#endif
    if (script_path.isEmpty()) {
        if (current_error_.isEmpty()) current_error_ = "生成更新脚本失败";
        emit error_occurred(current_error_);
        return;
    }
    execute_script_and_quit(script_path);
}

bool UpdateManager::extract_platform_info(const QJsonObject& platforms_obj,
    QString& out_file,
    QString& out_url,
    QString& out_sha) {
    QString key;
#ifdef Q_OS_WIN
    key = "windows";
#elif defined(Q_OS_MACOS)
    key = "macos";
#else
    key = "linux";
#endif

    if (!platforms_obj.contains(key)) return false;
    QJsonObject plat = platforms_obj[key].toObject();
    out_file = plat["file"].toString();
    out_url = plat["url"].toString();
    out_sha = plat["sha256"].toString().toLower();
    if (out_sha.size() != 64) return false;
    return !out_file.isEmpty();
}

bool UpdateManager::verify_sha256(const QString& file_path, const QString& expected_sha) {
    QFile file(file_path);
    if (!file.open(QIODevice::ReadOnly)) return false;
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) return false;
    return QString(hash.result().toHex()) == expected_sha.toLower();
}

QString UpdateManager::generate_windows_script(const QString& installer_path) {
    const QString exe_path = QCoreApplication::applicationFilePath();
    const QString script_path = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/dglab_update.bat";
    const QString fail_flag = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/update_failed.flag";
    const qint64 self_pid = QCoreApplication::applicationPid();

    QFile file(script_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        current_error_ = "无法写入更新脚本: " + file.errorString();
        return QString(); // 空串，调用方检查
    }
    QTextStream out(&file);

    out << "@echo off\n"
        << "chcp 65001 >nul\n"
        << "\n"
        << "set \"TARGET_PID=" << self_pid << "\"\n"
        << "set \"INSTALLER=" << QDir::toNativeSeparators(installer_path) << "\"\n"
        << "set \"EXE=" << QDir::toNativeSeparators(exe_path) << "\"\n"
        << "set \"FLAG=" << QDir::toNativeSeparators(update_flag_path()) << "\"\n"
        << "set \"FAIL_FLAG=" << QDir::toNativeSeparators(fail_flag) << "\"\n"
        << "\n"
        << "rem 等待旧进程退出（按 PID 精确轮询）\n"
        << ":wait_exit\n"
        << "set \"RUNNING=\"\n"
        << "for /f \"tokens=2 delims=,\" %%a in (\'tasklist /FI \"PID eq %TARGET_PID%\" /FO CSV /NH 2^>nul\') do set \"RUNNING=%%~a\"\n"
        << "if \"%RUNNING%\"==\"%TARGET_PID%\" (\n"
        << "    timeout /t 1 /nobreak >nul\n"
        << "    goto wait_exit\n"
        << ")\n"
        << "\n"
        << "rem 静默安装（NSIS 覆盖旧文件）\n"
        << "start /wait \"\" \"%INSTALLER%\" /S\n"
        << "\n"
        << "rem 启动新版本\n"
        << "start \"\" \"%EXE%\" --updated\n"
        << "\n"
        << "rem 轮询等待健康标记（最多 20 秒）\n"
        << "set /a waited=0\n"
        << ":wait_flag\n"
        << "if exist \"%FLAG%\" goto flag_ok\n"
        << "timeout /t 1 /nobreak >nul\n"
        << "set /a waited+=1\n"
        << "if %waited% lss 20 goto wait_flag\n"
        << "echo update failed > \"%FAIL_FLAG%\"\n"
        << "goto cleanup\n"
        << "\n"
        << ":flag_ok\n"
        << "del \"%FLAG%\"\n"
        << "\n"
        << ":cleanup\n"
        << "del \"%~f0\"\n";

    file.close();
    return script_path;
}

QString UpdateManager::generate_macos_script(const QString& dmg_path) {
    QString app_path = QCoreApplication::applicationDirPath();
    if (app_path.contains(".app/")) {
        app_path = app_path.left(app_path.indexOf(".app/") + 4);
    }
    const QString script_path = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/dglab_update.sh";
    const qint64 self_pid = QCoreApplication::applicationPid();

    QFile file(script_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        current_error_ = "无法写入更新脚本: " + file.errorString();
        return QString(); // 空串，调用方检查
    }
    QTextStream out(&file);

    out << "#!/bin/bash\n"
        << "set -euo pipefail\n"
        << "\n"
        << "rollback() {\n"
        << "    rm -rf \"$APP_PATH\" 2>/dev/null\n"
        << "    mv \"$APP_PATH.bak\" \"$APP_PATH\" 2>/dev/null\n"
        << "    open \"$APP_PATH\" 2>/dev/null\n"
        << "    exit 1\n"
        << "}\n"
        << "trap rollback ERR\n"
        << "TARGET_PID=" << self_pid << "\n"
        << "APP_PATH=\"" << app_path << "\"\n"
        << "DMG_PATH=\"" << dmg_path << "\"\n"
        << "FLAG=\"" << update_flag_path() << "\"\n"
        << "\n"
        << "# 等待旧进程退出（按 PID 精确轮询，30 秒兜底）\n"
        << "elapsed=0\n"
        << "while kill -0 \"$TARGET_PID\" 2>/dev/null; do\n"
        << "    sleep 1\n"
        << "    elapsed=$((elapsed + 1))\n"
        << "    if [ \"$elapsed\" -ge 30 ]; then break; fi\n"
        << "done\n"
        << "\n"
        << "# 备份并替换\n"
        << "mv \"$APP_PATH\" \"$APP_PATH.bak\"\n"
        << "MOUNT_DIR=$(mktemp -d /tmp/dglab_update.XXXXXX)\n"
        << "if ! hdiutil attach \"$DMG_PATH\" -nobrowse -quiet -mountpoint \"$MOUNT_DIR\"; then\n"
        << "    rollback\n"
        << "fi\n"
        << "cp -R \"$MOUNT_DIR\"/*.app \"$APP_PATH\"\n"
        << "hdiutil detach \"$MOUNT_DIR\" -quiet\n"
        << "rmdir \"$MOUNT_DIR\"\n"
        << "\n"
        << "# 启动新版本\n"
        << "\"$APP_PATH/Contents/MacOS/DG-LAB-Client\" --updated &\n"
        << "\n"
        << "# 轮询等待健康标记（最多 20 秒）\n"
        << "elapsed=0\n"
        << "while [ ! -f \"$FLAG\" ]; do\n"
        << "    sleep 1\n"
        << "    elapsed=$((elapsed + 1))\n"
        << "    if [ \"$elapsed\" -ge 20 ]; then break; fi\n"
        << "done\n"
        << "\n"
        << "if [ -f \"$FLAG\" ]; then\n"
        << "    rm -rf \"$APP_PATH.bak\"\n"
        << "    rm -f \"$FLAG\"\n"
        << "else\n"
        << "    rm -rf \"$APP_PATH\"\n"
        << "    mv \"$APP_PATH.bak\" \"$APP_PATH\"\n"
        << "    open \"$APP_PATH\"\n"
        << "fi\n"
        << "rm -f \"$0\"\n";

    file.close();
    if (!file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                             QFileDevice::ExeOwner |
                             QFileDevice::ReadGroup | QFileDevice::ExeGroup |
                             QFileDevice::ReadOther | QFileDevice::ExeOther)) {
        current_error_ = "无法为更新脚本设置可执行权限: " + file.errorString();
        QFile::remove(script_path);
        return QString();
    }
    return script_path;
}

QString UpdateManager::generate_linux_script(const QString& appimage_path) {
    QString current_appimage = qEnvironmentVariable("APPIMAGE");
    if (current_appimage.isEmpty()) current_appimage = QCoreApplication::applicationFilePath();
    const QString script_path = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/dglab_update.sh";
    const qint64 self_pid = QCoreApplication::applicationPid();

    QFile file(script_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        current_error_ = "无法写入更新脚本: " + file.errorString();
        return QString(); // 空串，调用方检查
    }
    QTextStream out(&file);

    out << "#!/bin/bash\n"
        << "set -euo pipefail\n"
        << "\n"
        << "rollback() {\n"
        << "    rm -f \"$CURRENT_APPIMAGE\" 2>/dev/null\n"
        << "    mv \"$CURRENT_APPIMAGE.bak\" \"$CURRENT_APPIMAGE\" 2>/dev/null\n"
        << "    \"$CURRENT_APPIMAGE\" &\n"
        << "    exit 1\n"
        << "}\n"
        << "trap rollback ERR\n"
        << "TARGET_PID=" << self_pid << "\n"
        << "CURRENT_APPIMAGE=\"" << current_appimage << "\"\n"
        << "NEW_APPIMAGE=\"" << appimage_path << "\"\n"
        << "FLAG=\"" << update_flag_path() << "\"\n"
        << "\n"
        << "# 等待旧进程退出（按 PID 精确轮询，30 秒兜底）\n"
        << "elapsed=0\n"
        << "while kill -0 \"$TARGET_PID\" 2>/dev/null; do\n"
        << "    sleep 1\n"
        << "    elapsed=$((elapsed + 1))\n"
        << "    if [ \"$elapsed\" -ge 30 ]; then break; fi\n"
        << "done\n"
        << "\n"
        << "# 备份并替换\n"
        << "mv \"$CURRENT_APPIMAGE\" \"$CURRENT_APPIMAGE.bak\"\n"
        << "mv \"$NEW_APPIMAGE\" \"$CURRENT_APPIMAGE\"\n"
        << "chmod +x \"$CURRENT_APPIMAGE\"\n"
        << "\n"
        << "# 启动新版本\n"
        << "\"$CURRENT_APPIMAGE\" --updated &\n"
        << "\n"
        << "# 轮询等待健康标记（最多 20 秒）\n"
        << "elapsed=0\n"
        << "while [ ! -f \"$FLAG\" ]; do\n"
        << "    sleep 1\n"
        << "    elapsed=$((elapsed + 1))\n"
        << "    if [ \"$elapsed\" -ge 20 ]; then break; fi\n"
        << "done\n"
        << "\n"
        << "if [ -f \"$FLAG\" ]; then\n"
        << "    rm -f \"$CURRENT_APPIMAGE.bak\"\n"
        << "    rm -f \"$FLAG\"\n"
        << "else\n"
        << "    rm -f \"$CURRENT_APPIMAGE\"\n"
        << "    mv \"$CURRENT_APPIMAGE.bak\" \"$CURRENT_APPIMAGE\"\n"
        << "    \"$CURRENT_APPIMAGE\" &\n"
        << "fi\n"
        << "rm -f \"$0\"\n";

    file.close();
    if (!file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                             QFileDevice::ExeOwner |
                             QFileDevice::ReadGroup | QFileDevice::ExeGroup |
                             QFileDevice::ReadOther | QFileDevice::ExeOther)) {
        current_error_ = "无法为更新脚本设置可执行权限: " + file.errorString();
        QFile::remove(script_path);
        return QString();
    }
    return script_path;
}

void UpdateManager::execute_script_and_quit(const QString& script_path) {
    LOG_MODULE("UpdateManager", "execute_script_and_quit", LOG_INFO, "执行更新脚本并退出程序");
#ifdef Q_OS_WIN
    bool ok = QProcess::startDetached("cmd.exe", {"/c", script_path});
#else
    bool ok = QProcess::startDetached("bash", {script_path});
#endif
    if (!ok) {
        current_error_ = "无法启动更新脚本";
        emit error_occurred(current_error_);
        return;
    }
    QTimer::singleShot(100, qApp, &QCoreApplication::quit);
}

bool UpdateManager::is_self_updatable() const {
#ifdef Q_OS_LINUX
    // AppImage：环境变量非空
    if (!qEnvironmentVariableIsEmpty("APPIMAGE")) return true;

    // Flatpak / Snap：由各自商店管理
    if (qEnvironmentVariableIsSet("FLATPAK_ID")) return false;
    if (qEnvironmentVariableIsSet("SNAP")) return false;

    // 落在系统包路径下（deb/rpm 安装）
    const QString path = QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath();
    static const QStringList system_prefixes = {
        "/usr/", "/opt/", "/snap/", "/var/lib/", "/nix/"};
    for (const auto& prefix : system_prefixes) {
        if (path.startsWith(prefix)) return false;
    }
    return true; // 便携版/源码运行
#else
    return true;
#endif
}

QString UpdateManager::unsupported_update_hint() const {
#ifdef Q_OS_LINUX
    if (qEnvironmentVariableIsSet("FLATPAK_ID")) {
        return "当前为 Flatpak 版本，请通过 flatpak update 或软件中心更新";
    }
    if (qEnvironmentVariableIsSet("SNAP")) {
        return "当前为 Snap 版本，请通过 snap refresh 或软件中心更新";
    }
    const QString path = QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath();
    if (path.startsWith("/usr/") || path.startsWith("/opt/") ||
        path.startsWith("/var/lib/") || path.startsWith("/nix/")) {
        return "当前为系统包管理器安装，请使用 apt/dnf/pacman 等工具更新";
    }
#endif
    return QString();
}

bool UpdateManager::has_update() const { return !download_url_.isEmpty(); }
QString UpdateManager::remote_version() const { return remote_version_; }
QString UpdateManager::release_notes() const { return release_notes_; }
QString UpdateManager::error_message() const { return current_error_; }
QString UpdateManager::release_url() const { return release_url_; }

QString UpdateManager::update_flag_path() {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/update_ok.tmp";
}
