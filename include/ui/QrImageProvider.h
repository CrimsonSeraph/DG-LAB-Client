/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <QQuickImageProvider>
#include <QString>

// ============================================
// QrImageProvider - 应用内二维码图像提供者
// ============================================
// 通过 image://dglabqr/<rev> 向 QML 提供配对链接的二维码（Nayuki qrcodegen，MIT）。
class QrImageProvider : public QQuickImageProvider {
public:
    QrImageProvider();

    /// @brief 更新待编码文本（配对链接）
    void set_text(const QString& text);

    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;

private:
    QString text_;
};
