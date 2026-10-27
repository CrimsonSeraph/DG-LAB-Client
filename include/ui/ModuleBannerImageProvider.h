/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QImage>
#include <QQuickImageProvider>
#include <QSize>
#include <QString>

// ModuleBannerImageProvider - 插件侧车头图提供者
// 通过 image://modulebanner/<插件文件名>/<头图文件名> 读取插件安装目录下
// <动态库主干名>/<头图文件名>；id 非法、文件缺失或不是图片时返回空图，QML 侧保持空态。
class ModuleBannerImageProvider : public QQuickImageProvider {
public:
    ModuleBannerImageProvider();

    /// @brief 解析 id 并读取头图
    /// @param id "<插件文件名>/<头图文件名>"
    /// @param size 非空时写入原图尺寸
    /// @param requestedSize 有效时按等比缩放到该尺寸
    /// @return 头图；id 非法、插件目录未知、文件缺失或不是图片时为空图
    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;
};
