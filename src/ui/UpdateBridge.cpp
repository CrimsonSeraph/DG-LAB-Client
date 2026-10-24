#include "UpdateBridge.h"

#include "UpdateManager.h"

UpdateBridge::UpdateBridge(UpdateManager* manager, QObject* parent)
    : QObject(parent)
    , manager_(manager)
    , checking_(false)
    , downloading_(false)
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


bool UpdateBridge::has_update() const { return manager_->has_update(); }

void UpdateBridge::on_check_update_clicked() {
    checking_ = true;
    downloading_ = false;
    ready_to_install_ = false;
    progress_ = 0;
    error_message_.clear();
    emit state_changed();
    emit progress_changed();
    manager_->check_for_updates();
}

void UpdateBridge::on_start_download_clicked() {
    downloading_ = true;
    progress_ = 0;
    emit state_changed();
    emit progress_changed();
    manager_->start_download();
}

void UpdateBridge::on_apply_update_clicked() {
    manager_->apply_update_and_restart();
}

void UpdateBridge::on_update_available(bool available) {
    checking_ = false;
    emit state_changed();
    if (available) {
        emit toast_requested(tr("发现新版本: %1").arg(manager_->remote_version()), 1); // 1 = info
    }
    else if (error_message_.isEmpty()) {
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
        emit state_changed();
        emit progress_changed();
        emit toast_requested(tr("更新包下载完成"), 1);
    }
    else {
        error_message_ = error;
        emit state_changed();
        emit toast_requested(error, 3); // 3 = danger
    }
}

void UpdateBridge::on_error_occurred(const QString& error) {
    checking_ = false;
    error_message_ = error;
    emit state_changed();
    emit toast_requested(error, 3);
}
