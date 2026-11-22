// Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
// SPDX-License-Identifier: GPL-3.0-only

#include "UpdateBridge.h"

#include "AppConfig.h"
#include "DebugLog.h"
#include "UpdateManager.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QTimer>
#include <QUrl>

namespace {

// 启动静默检查的延迟，避开启动高峰
constexpr int kStartupCheckDelayMs = 60'000;
// 静默检查的最小间隔（秒），一天最多打扰一次
constexpr qint64 kStartupCheckIntervalSecs = 24 * 60 * 60;
// 上次自动检查时间的配置键（Unix 秒，写在 user 层）
const char* const kLastCheckKey = "app.update.last_check";

} // namespace

UpdateBridge::UpdateBridge(UpdateManager* manager, QObject* parent)
    : QObject(parent)
    , manager_(manager)
    , checking_(false)
    , downloading_(false)
    , silent_check_(false)
    , ready_to_install_(false)
    , progress_(0) {
    Q_ASSERT(manager_ != nullptr);
    if (manager_ == nullptr) return;
    connect(manager_, &UpdateManager::update_available, this, &UpdateBridge::on_update_available);
    connect(manager_, &UpdateManager::download_progress, this, &UpdateBridge::on_download_progress);
    connect(manager_, &UpdateManager::download_finished, this, &UpdateBridge::on_download_finished);
    connect(manager_, &UpdateManager::error_occurred, this, &UpdateBridge::on_error_occurred);
}

bool UpdateBridge::checking() const { return checking_; }
bool UpdateBridge::downloading() const { return downloading_; }
bool UpdateBridge::ready_to_install() const { return ready_to_install_; }
int UpdateBridge::progress() const { return progress_; }
QString UpdateBridge::remote_version() const { return manager_->remote_version(); }
QString UpdateBridge::release_notes() const { return manager_->release_notes(); }
QString UpdateBridge::error_message() const { return error_message_; }
QString UpdateBridge::release_url() const { return manager_->release_url(); }
bool UpdateBridge::has_update() const { return manager_->has_update(); }

void UpdateBridge::open_release_url() {
    if (manager_ == nullptr) {
        return;
    }
    const QString url = manager_->release_url();
    if (!url.isEmpty()) {
        QDesktopServices::openUrl(QUrl(url));
    }
}

void UpdateBridge::schedule_startup_check() {
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const qint64 last = AppConfig::instance().get_value<qint64>(kLastCheckKey, 0);
    if (last > 0 && now - last < kStartupCheckIntervalSecs) {
        LOG_MODULE("UpdateBridge", "schedule_startup_check", LOG_DEBUG, "距上次自动检查不足一天，跳过本次静默检查");
        return;
    }
    LOG_MODULE("UpdateBridge", "schedule_startup_check", LOG_INFO,
        QString("启动 %1 秒后执行静默检查更新").arg(kStartupCheckDelayMs / 1000));
    QTimer::singleShot(kStartupCheckDelayMs, this, &UpdateBridge::on_startup_check_timer);
}

void UpdateBridge::on_check_update_clicked() {
    if (checking_ || downloading_) return;
    checking_ = true;
    ready_to_install_ = false;
    progress_ = 0;
    error_message_.clear();
    emit state_changed();
    emit progress_changed();
    manager_->check_for_updates();
}

void UpdateBridge::on_start_download_clicked() {
    if (!manager_->has_update() || downloading_) return;
    downloading_ = true;
    progress_ = 0;
    emit state_changed();
    emit progress_changed();
    manager_->start_download();
}

void UpdateBridge::on_apply_update_clicked() {
    if (!ready_to_install_) return;
    manager_->apply_update_and_restart();
}

void UpdateBridge::on_update_available(bool available) {
    const bool silent = silent_check_;
    silent_check_ = false;
    checking_ = false;
    emit state_changed();
    if (available) {
        // 静默检查发现新版本同样只发非打扰的 Toast 提示
        emit toast_requested(tr("发现新版本: %1").arg(manager_->remote_version()), 1); // 1 = info
    }
    else if (!silent && error_message_.isEmpty()) {
        emit toast_requested(tr("当前已是最新版本"), 1);
    }
}

void UpdateBridge::on_download_progress(qint64 received, qint64 total) {
    if (total > 0) {
        progress_ = static_cast<int>((received * 100) / total);
        emit progress_changed();
    }
}

void UpdateBridge::on_download_finished(bool success, const QString& error) {
    downloading_ = false;
    if (success) {
        ready_to_install_ = true;
        progress_ = 100;
        error_message_.clear();
        emit state_changed();
        emit progress_changed();
        emit toast_requested(tr("更新包下载完成"), 1);
    }
    else {
        ready_to_install_ = false;
        progress_ = 0;
        error_message_ = error;
        emit state_changed();
        emit progress_changed();
        emit toast_requested(error, 3);
    }
}

void UpdateBridge::on_error_occurred(const QString& error) {
    const bool silent = silent_check_;
    silent_check_ = false;
    checking_ = false;
    downloading_ = false;
    if (silent) {
        // 启动静默检查失败不打扰用户，失败原因已由 UpdateManager 记入日志
        emit state_changed();
        return;
    }
    error_message_ = error;
    emit state_changed();
    emit toast_requested(error, 3);
}

void UpdateBridge::on_startup_check_timer() {
    if (checking_ || downloading_) {
        LOG_MODULE("UpdateBridge", "on_startup_check_timer", LOG_DEBUG, "已有检查或下载在进行，跳过本次静默检查");
        return;
    }
    // 发起前就落盘时间戳：本次即使失败也计入，保证一天最多打扰一次
    AppConfig::instance().set_value_with_name<qint64>(
        kLastCheckKey, QDateTime::currentSecsSinceEpoch(), "user");
    AppConfig::instance().save_all();

    LOG_MODULE("UpdateBridge", "on_startup_check_timer", LOG_INFO, "开始静默检查更新");
    silent_check_ = true;
    checking_ = true;
    emit state_changed();
    manager_->check_for_updates();
}
