/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "BleDeviceManager.h"

#include "CoyoteBleController.h"
#include "DebugLog.h"
#include "OpossumBleController.h"

#include <QBluetoothDeviceInfo>

BleDeviceManager::BleDeviceManager(QObject* parent)
    : QObject(parent) {
}

BleDeviceManager::~BleDeviceManager() {
    // 实例是本对象的子对象，由 QObject 析构统一回收；这里只清登记表，避免销毁期回调读到悬空项
    registry_.clear();
    status_text_.clear();
    screen_sync_.clear();
}

IOutputDevice* BleDeviceManager::open_link(const QBluetoothDeviceInfo& info, const BleLinkRequest& request,
    QString* error) {
    if (error != nullptr) {
        error->clear();
    }
    const QString mac = LinkRegistry::normalize(info.address().toString());
    if (!info.isValid() || mac.isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("设备信息无效，请重新扫描");
        }
        return nullptr;
    }
    // 同一 MAC 已有实例：不新建，避免同一台设备被两份驱动同时写入
    if (IOutputDevice* existing = registry_.find(mac); existing != nullptr) {
        if (error != nullptr) {
            *error = QStringLiteral("该设备已在连接中");
        }
        return nullptr;
    }

    IOutputDevice* link = nullptr;
    if (request.type == QStringLiteral("coyote")) {
        auto* coyote = new CoyoteBleDevice(this);
        watch_link(coyote, mac);
        watch_coyote(coyote, mac);
        coyote->connect_device(info, request.model, request.label);
        link = coyote;
    }
    else if (request.type == QStringLiteral("opossum")) {
        auto* opossum = new OpossumBleController(this);
        opossum->initialize();
        watch_link(opossum, mac);
        watch_opossum(opossum, mac);
        opossum->connect_device(info);
        link = opossum;
    }
    else {
        // 型号未知或本版没有对应驱动：拒绝并说明原因，不假装可用
        LOG_MODULE("BleDeviceManager", "open_link", LOG_WARN,
            "拒绝连接无驱动的型号:" << request.type.toStdString() << " MAC " << mac.toStdString());
        if (error != nullptr) {
            *error = request.label.isEmpty()
                ? QStringLiteral("暂不支持该设备（无对应驱动）")
                : QStringLiteral("暂不支持该设备：%1 无对应驱动").arg(request.label);
        }
        return nullptr;
    }

    if (registry_.add(link) != link) {
        // 地址在连接开始时才写入，这里兜底：登记失败说明实例拿不到 MAC，直接销毁
        LOG_MODULE("BleDeviceManager", "open_link", LOG_ERROR, "链路实例登记失败，已销毁:" << mac.toStdString());
        link->deleteLater();
        if (error != nullptr) {
            *error = QStringLiteral("设备地址无效，无法登记链路");
        }
        return nullptr;
    }
    LOG_MODULE("BleDeviceManager", "open_link", LOG_INFO,
        "链路已登记:" << mac.toStdString() << " 型号 " << request.model.toStdString() << " 当前共 "
                      << registry_.count() << " 台");
    emit linkCreated(link);
    return link;
}

void BleDeviceManager::close_link(const QString& mac) {
    const QString key = LinkRegistry::normalize(mac);
    IOutputDevice* link = registry_.find(key);
    if (link == nullptr) {
        return;
    }
    registry_.remove(key);
    status_text_.remove(key);
    screen_sync_.remove(key);
    emit linkDestroyed(link);
    // 断开先于销毁：驱动析构也会走同一条断开路径，这里显式调用以先发一次状态变化
    if (auto* coyote = qobject_cast<CoyoteBleDevice*>(link); coyote != nullptr) {
        coyote->disconnect_device();
    }
    else if (auto* opossum = qobject_cast<OpossumBleController*>(link); opossum != nullptr) {
        opossum->disconnect_device();
    }
    link->deleteLater();
    LOG_MODULE("BleDeviceManager", "close_link", LOG_INFO,
        "链路已断开:" << key.toStdString() << " 其余 " << registry_.count() << " 台保持连接");
}

void BleDeviceManager::close_all() {
    const QStringList addresses = registry_.addresses();
    for (const QString& address : addresses) {
        close_link(address);
    }
}

void BleDeviceManager::watch_link(IOutputDevice* device, const QString& mac) {
    if (device == nullptr) {
        return;
    }
    const QString key = LinkRegistry::normalize(mac);
    connect(device, &IOutputDevice::outputStateChanged, this, [this, device, key]() {
        if (device->connected() || !registry_.contains(key)) {
            return;
        }
        // 设备自行掉线（断电 / 超距）：注销并销毁该 MAC 的实例，其它设备保持连接
        LOG_MODULE("BleDeviceManager", "watch_link", LOG_INFO,
            "链路自行掉线，已注销:" << key.toStdString());
        close_link(key);
    });
}

void BleDeviceManager::watch_coyote(CoyoteBleDevice* device, const QString& mac) {
    if (device == nullptr) {
        return;
    }
    const QString key = LinkRegistry::normalize(mac);
    connect(device, &CoyoteBleDevice::statusChanged, this, [this, device, key]() {
        status_text_.insert(key, device->status());
        emit linkStatusChanged(device);
    });
    connect(device, &QObject::destroyed, this, [this, key]() {
        registry_.remove(key);
        status_text_.remove(key);
        screen_sync_.remove(key);
    });
}

void BleDeviceManager::watch_opossum(OpossumBleController* device, const QString& mac) {
    if (device == nullptr) {
        return;
    }
    const QString key = LinkRegistry::normalize(mac);
    connect(device, &OpossumBleController::statusChanged, this, [this, device, key]() {
        status_text_.insert(key, device->status());
        emit linkStatusChanged(device);
    });
    connect(device, &OpossumBleController::screenSyncChanged, this, [this, device, key]() {
        screen_sync_.insert(key, device->screen_sync_available());
    });
    connect(device, &QObject::destroyed, this, [this, key]() {
        registry_.remove(key);
        status_text_.remove(key);
        screen_sync_.remove(key);
    });
}
