/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "ModuleBannerImageProvider.h"

#include "DebugLog.h"
#include "ModuleManager.h"

#include <QDir>
#include <QFileInfo>
#include <QStringList>

#include <atomic>
#include <string>

namespace {

    std::atomic<bool> g_illegal_id_logged{false}; ///< 非法 id 只记录一次，避免卡片重绘时刷屏
    std::atomic<bool> g_missing_file_logged{false}; ///< 文件不可读只记录一次

    // 检查 id 分段：非空、非当前目录、非上级目录、不含路径分隔符与盘符
    bool is_safe_component(const QString& component) {
        if (component.isEmpty() || component == QStringLiteral(".") || component == QStringLiteral("..")) {
            return false;
        }
        return !component.contains(QLatin1Char('/')) && !component.contains(QLatin1Char('\\'))
            && !component.contains(QLatin1Char(':'));
    }

    // 单次写出失败原因，重复请求不再记录
    void log_once(std::atomic<bool>& flag, const std::string& message) {
        if (!flag.exchange(true)) {
            LOG_MODULE("ModuleBannerImageProvider", "requestImage", LOG_DEBUG, message);
        }
    }

} // namespace

ModuleBannerImageProvider::ModuleBannerImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image) {
}

QImage ModuleBannerImageProvider::requestImage(const QString& id, QSize* size, const QSize& requestedSize) {
    // id 固定两段（插件文件名 / 头图文件名）：多段、空段、绝对路径与上级目录一律拒绝
    const QStringList parts = id.split(QLatin1Char('/'));
    if (parts.size() != 2 || !is_safe_component(parts.at(0)) || !is_safe_component(parts.at(1))) {
        log_once(g_illegal_id_logged, "拒绝非法头图 id: " + id.toStdString());
        return QImage();
    }
    // 侧车目录名是动态库主干名，插件目录取自 ModuleManager（与扫描目录同一份）
    const QString plugin_dir = QString::fromStdString(ModuleManager::instance().get_plugin_dir());
    const QString stem = QFileInfo(parts.at(0)).completeBaseName();
    if (plugin_dir.isEmpty() || !is_safe_component(stem)) {
        log_once(g_illegal_id_logged, "拒绝非法头图 id: " + id.toStdString());
        return QImage();
    }
    const QString path = QDir(plugin_dir).filePath(stem + QLatin1Char('/') + parts.at(1));
    QImage image(path);
    if (image.isNull()) {
        log_once(g_missing_file_logged, "头图不可读，按空态处理: " + path.toStdString());
        return QImage();
    }
    if (size != nullptr) {
        *size = image.size();
    }
    // 请求尺寸由 QML 给出时先缩再返回，避免原图长期占内存
    if (requestedSize.isValid() && !requestedSize.isEmpty()) {
        image = image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return image;
}
