/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "QrImageProvider.h"

#include "qrcodegen.hpp"

#include <QColor>
#include <QImage>

#include <algorithm>

QrImageProvider::QrImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image) {
}

void QrImageProvider::set_text(const QString& text) {
    text_ = text;
}

QImage QrImageProvider::requestImage(const QString& id, QSize* size, const QSize& requestedSize) {
    Q_UNUSED(id);

    const int fallback = 256;
    const int target = (requestedSize.isValid() && requestedSize.width() > 0) ? requestedSize.width() : fallback;

    if (text_.isEmpty()) {
        QImage empty(target, target, QImage::Format_ARGB32);
        empty.fill(Qt::transparent);
        if (size != nullptr) {
            *size = empty.size();
        }
        return empty;
    }

    const qrcodegen::QrCode qr
        = qrcodegen::QrCode::encodeText(text_.toUtf8().constData(), qrcodegen::QrCode::Ecc::MEDIUM);

    const int modules = qr.getSize();
    const int border = 4;
    const int total_modules = modules + border * 2;
    const int scale = std::max(1, target / total_modules);
    const int image_size = total_modules * scale;

    QImage image(image_size, image_size, QImage::Format_ARGB32);
    image.fill(Qt::white);

    for (int y = 0; y < modules; ++y) {
        for (int x = 0; x < modules; ++x) {
            if (!qr.getModule(x, y)) {
                continue;
            }
            for (int dy = 0; dy < scale; ++dy) {
                for (int dx = 0; dx < scale; ++dx) {
                    image.setPixelColor((x + border) * scale + dx, (y + border) * scale + dy, Qt::black);
                }
            }
        }
    }

    if (size != nullptr) {
        *size = image.size();
    }
    return image;
}
