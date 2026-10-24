#include "UpdateManager.h"

#include "AppVersion.h"
#include "DebugLog.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>


UpdateManager::UpdateManager(QObject* parent)
    : QObject(parent)
    , network_manager_(new QNetworkAccessManager(this))
    , current_download_reply_(nullptr)
    , manifest_url_("https://github.com/CrimsonSeraph/DG-LAB-Client/releases/latest/download/manifest.json") {
}

UpdateManager::~UpdateManager() {
    if (current_download_reply_) {
        current_download_reply_->abort();
    }
}

void UpdateManager::check_for_updates() {
    LOG_MODULE("UpdateManager", "check_for_updates", LOG_INFO, "开始检查更新...");
    current_error_.clear();

    QNetworkRequest req{QUrl(manifest_url_)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = network_manager_->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
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

    if (!is_newer_version(APP_VERSION, remote_version_)) {
        LOG_MODULE("UpdateManager", "on_manifest_reply", LOG_INFO, "当前已是最新版本: " + remote_version_);
        emit update_available(false);
        return;
    }

    QString file_name;
    if (extract_platform_info(obj["platforms"].toObject(), file_name, expected_sha256_)) {
        // 动态拼接下载链接
        QString base_url = manifest_url_.left(manifest_url_.lastIndexOf('/'));
        download_url_ = base_url + "/" + file_name;
        emit update_available(true);
        LOG_MODULE("UpdateManager", "on_manifest_reply", LOG_INFO, "发现新版本: " + remote_version_);
    }
    else {
        current_error_ = "当前平台没有可用的更新包";
        emit error_occurred(current_error_);
    }
}

void UpdateManager::start_download() {
    if (download_url_.isEmpty()) return;

    LOG_MODULE("UpdateManager", "start_download", LOG_INFO, "开始下载: " + download_url_);

    QNetworkRequest req{QUrl(download_url_)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    // 临时文件路径
    download_file_path_ = QStandardPaths::writableLocation(QStandardPaths::TempLocation) +
                          "/" + QFileInfo(download_url_).fileName();

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
}

void UpdateManager::on_download_finished() {
    if (!current_download_reply_) return;
    current_download_reply_->deleteLater();

    if (current_download_reply_->error() != QNetworkReply::NoError) {
        current_error_ = "下载失败: " + current_download_reply_->errorString();
        emit download_finished(false, current_error_);
        return;
    }

    if (!verify_sha256(download_file_path_, expected_sha256_)) {
        current_error_ = "文件校验失败 (SHA256 不匹配)";
        QFile::remove(download_file_path_);
        emit download_finished(false, current_error_);
        return;
    }

    LOG_MODULE("UpdateManager", "on_download_finished", LOG_INFO, "下载完成并通过校验");
    emit download_finished(true, "");
}

void UpdateManager::apply_update_and_restart() {
    if (download_file_path_.isEmpty()) return;

    QString script_path;
#ifdef Q_OS_WIN
    script_path = generate_windows_script(download_file_path_);
#elif defined(Q_OS_MACOS)
    script_path = generate_macos_script(download_file_path_);
#else
    script_path = generate_linux_script(download_file_path_);
#endif

    execute_script_and_quit(script_path);
}

bool UpdateManager::is_newer_version(const QString& current, const QString& remote) const {
    QString r = remote;
    if (r.startsWith('v') || r.startsWith('V')) r.remove(0, 1);

    QStringList cur_parts = current.split('.');
    QStringList rem_parts = r.split('.');

    for (int i = 0; i < qMax(cur_parts.size(), rem_parts.size()); ++i) {
        int c = (i < cur_parts.size()) ? cur_parts[i].toInt() : 0;
        int rm = (i < rem_parts.size()) ? rem_parts[i].toInt() : 0;
        if (rm > c) return true;
        if (rm < c) return false;
    }
    return false;
}

bool UpdateManager::extract_platform_info(const QJsonObject& platforms_obj, QString& out_file, QString& out_sha) {
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
    out_sha = plat["sha256"].toString();
    return !out_file.isEmpty() && !out_sha.isEmpty();
}

bool UpdateManager::verify_sha256(const QString& file_path, const QString& expected_sha) {
    QFile file(file_path);
    if (!file.open(QIODevice::ReadOnly)) return false;
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (hash.addData(&file)) {
        return QString(hash.result().toHex()) == expected_sha.toLower();
    }
    return false;
}

QString UpdateManager::generate_windows_script(const QString& installer_path) {
    QString app_dir = QCoreApplication::applicationDirPath();
    QString exe_path = QCoreApplication::applicationFilePath();
    QString script_path = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/dglab_update.bat";

    QFile file(script_path);
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream out(&file);

    out << "@echo off\n"
        << "chcp 65001 >nul\n"
        << ":wait_loop\n"
        // 稍显复杂：如果是 NSIS 安装包，直接静默运行即可，无需手动备份目录（NSIS 会处理覆盖）
        << "start /wait \"\" \"" << QDir::toNativeSeparators(installer_path) << "\" /S\n"
        << "start \"\" \"" << QDir::toNativeSeparators(exe_path) << "\" --updated\n"
        << "timeout /t 5 /nobreak >nul\n"
        // 依赖主程序的健康检查文件
        << "if exist \"" << QDir::toNativeSeparators(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/update_ok.tmp") << "\" (\n"
        << "    del \"" << QDir::toNativeSeparators(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/update_ok.tmp") << "\"\n"
        << ") else (\n"
        // 如果 NSIS 覆盖了旧版，回滚比较困难（因为旧文件已丢）。建议只做弹窗提示
        << "    echo update failed > \"" << QDir::toNativeSeparators(QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/update_failed.flag") << "\"\n"
        << ")\n"
        << "del \"%~f0\"\n";
    return script_path;
}

QString UpdateManager::generate_macos_script(const QString& dmg_path) {
    QString app_path = QCoreApplication::applicationDirPath();
    // 获取 .app 路径
    if (app_path.contains(".app/")) {
        app_path = app_path.left(app_path.indexOf(".app/") + 4);
    }
    QString script_path = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/dglab_update.sh";

    QFile file(script_path);
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream out(&file);

    out << "#!/bin/bash\n"
        << "sleep 2\n"
        << "mv \"" << app_path << "\" \"" << app_path << ".bak\"\n"
        << "hdiutil attach \"" << dmg_path << "\" -nobrowse -quiet -mountpoint /tmp/dglab_update\n"
        << "cp -R /tmp/dglab_update/*.app \"" << app_path << "\"\n"
        << "hdiutil detach /tmp/dglab_update -quiet\n"
        << "\"" << app_path << "/Contents/MacOS/DG-LAB-Client\" --updated &\n"
        << "sleep 5\n"
        << "if [ -f \"" << QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) << "/update_ok.tmp\" ]; then\n"
        << "    rm -rf \"" << app_path << ".bak\"\n"
        << "    rm -f \"" << QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) << "/update_ok.tmp\"\n"
        << "else\n"
        << "    rm -rf \"" << app_path << "\"\n"
        << "    mv \"" << app_path << ".bak\" \"" << app_path << "\"\n"
        << "    open \"" << app_path << "\"\n"
        << "fi\n"
        << "rm -f \"$0\"\n";
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    return script_path;
}

QString UpdateManager::generate_linux_script(const QString& appimage_path) {
    QString current_appimage = QCoreApplication::applicationFilePath();
    QString script_path = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/dglab_update.sh";

    QFile file(script_path);
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream out(&file);

    out << "#!/bin/bash\n"
        << "sleep 1\n"
        << "mv \"" << current_appimage << "\" \"" << current_appimage << ".bak\"\n"
        << "mv \"" << appimage_path << "\" \"" << current_appimage << "\"\n"
        << "chmod +x \"" << current_appimage << "\"\n"
        << "\"" << current_appimage << "\" --updated &\n"
        << "sleep 5\n"
        << "if [ -f \"" << QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) << "/update_ok.tmp\" ]; then\n"
        << "    rm -f \"" << current_appimage << ".bak\"\n"
        << "    rm -f \"" << QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) << "/update_ok.tmp\"\n"
        << "else\n"
        << "    rm -f \"" << current_appimage << "\"\n"
        << "    mv \"" << current_appimage << ".bak\" \"" << current_appimage << "\"\n"
        << "    \"" << current_appimage << "\" &\n"
        << "fi\n"
        << "rm -f \"$0\"\n";
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    return script_path;
}

void UpdateManager::execute_script_and_quit(const QString& script_path) {
    LOG_MODULE("UpdateManager", "execute_script_and_quit", LOG_INFO, "执行更新脚本并退出程序");
#ifdef Q_OS_WIN
    QProcess::startDetached("cmd.exe", {"/c", script_path});
#else
    QProcess::startDetached("bash", {script_path});
#endif
    QTimer::singleShot(100, qApp, &QCoreApplication::quit);
}

bool UpdateManager::has_update() const { return !download_url_.isEmpty(); }
QString UpdateManager::remote_version() const { return remote_version_; }
QString UpdateManager::release_notes() const { return release_notes_; }
QString UpdateManager::error_message() const { return current_error_; }
