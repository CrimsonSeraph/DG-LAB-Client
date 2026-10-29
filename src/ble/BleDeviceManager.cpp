/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "BleDeviceManager.h"

#include "CivetBleController.h"
#include "CoyoteBleController.h"
#include "DebugLog.h"
#include "OpossumBleController.h"
#include "PawPrintsBleController.h"

#include <QBluetoothDeviceInfo>

BleDeviceManager::BleDeviceManager(QObject* parent)
    : QObject(parent) {
}

BleDeviceManager::~BleDeviceManager() {
    // 实例是本对象的子对象，由 QObject 析构统一回收；这里只清登记表，避免销毁期回调读到悬空项
    registry_.clear();
    status_text_.clear();
    screen_sync_.clear();
    // 输入设备实例的模块名与登记项归本类持有：destroyed 阶段不再发信号，直接释放
    qDeleteAll(input_links_);
    input_links_.clear();
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
    close_all_input_links();
}

InputLink* BleDeviceManager::open_input_link(const QBluetoothDeviceInfo& info, const BleLinkRequest& request,
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
    const QString prefix = request.type == QStringLiteral("pawprint") ? QStringLiteral("paw")
        : request.type == QStringLiteral("civet")                      ? QStringLiteral("civet")
                                                                      : QString();
    if (prefix.isEmpty()) {
        LOG_MODULE("BleDeviceManager", "open_input_link", LOG_WARN,
            "拒绝创建无驱动的输入设备型号:" << request.type.toStdString() << " MAC " << mac.toStdString());
        if (error != nullptr) {
            *error = QStringLiteral("暂不支持该输入设备（无对应驱动）");
        }
        return nullptr;
    }
    if (const InputLink* existing = input_links_.value(mac, nullptr); existing != nullptr) {
        if (existing->connected) {
            if (error != nullptr) {
                *error = QStringLiteral("该设备已在连接中");
            }
            return nullptr;
        }
        // 上一次连接失败留下的空实例：先销毁再重建，避免半路实例占着模块名与状态
        LOG_MODULE("BleDeviceManager", "open_input_link", LOG_INFO,
            "输入设备存在上次未连上的实例，先销毁再重建:" << mac.toStdString());
        close_input_link(mac);
    }

    auto* link = new InputLink();
    link->address = mac;
    link->type = request.type;
    link->model = request.model;
    link->label = request.label;
    int ordinal = 1;
    link->module_name = input_names_.acquire(prefix, mac, &ordinal);
    link->display_name = InputModuleNames::display_name(request.label, mac);
    if (ordinal > 1) {
        // 不同设备的 MAC 后四位可能相同（同型号也常见）：追加序号消歧，避免两台互相覆盖
        LOG_MODULE("BleDeviceManager", "open_input_link", LOG_WARN,
            "输入设备模块名后四位冲突，已消歧为:" << link->module_name.toStdString() << " MAC "
                                                  << mac.toStdString());
    }

    if (request.type == QStringLiteral("pawprint")) {
        auto* paw = new PawPrintsBleController(this);
        connect(paw, &PawPrintsBleController::stateChanged, this, [this, mac]() { sync_input_link_state(mac); });
        connect(paw, &PawPrintsBleController::statusChanged, this, [this, mac]() { sync_input_link_status(mac); });
        paw->initialize();
        link->driver = paw;
    }
    else {
        auto* civet = new CivetBleController(this);
        connect(civet, &CivetBleController::stateChanged, this, [this, mac]() { sync_input_link_state(mac); });
        connect(civet, &CivetBleController::statusChanged, this, [this, mac]() { sync_input_link_status(mac); });
        civet->initialize();
        link->driver = civet;
    }

    input_links_.insert(mac, link);
    // 先广播实例（订阅方在此注册数值模块与内置事件声明），再发起连接，首帧数据不会丢
    emit inputLinkCreated(link);
    if (auto* paw = qobject_cast<PawPrintsBleController*>(link->driver); paw != nullptr) {
        paw->connect_device(info);
    }
    else if (auto* civet = qobject_cast<CivetBleController*>(link->driver); civet != nullptr) {
        civet->connect_device(info);
    }
    LOG_MODULE("BleDeviceManager", "open_input_link", LOG_INFO,
        "输入设备实例已登记:" << mac.toStdString() << " 型号 " << request.model.toStdString() << " 模块 "
                              << link->module_name.toStdString() << " 当前共 " << input_links_.size() << " 台");
    return link;
}

void BleDeviceManager::close_input_link(const QString& mac) {
    const QString key = LinkRegistry::normalize(mac);
    InputLink* link = input_links_.take(key);
    if (link == nullptr) {
        return;
    }
    // 顺序：先让订阅方注销数值模块与内置声明（此刻驱动仍活着），再断开并销毁驱动，最后释放实例
    emit inputLinkDestroyed(link);
    input_names_.release(link->module_name);
    if (auto* paw = qobject_cast<PawPrintsBleController*>(link->driver); paw != nullptr) {
        paw->disconnect_device();
    }
    else if (auto* civet = qobject_cast<CivetBleController*>(link->driver); civet != nullptr) {
        civet->disconnect_device();
    }
    if (link->driver != nullptr) {
        link->driver->deleteLater();
    }
    const QString module_name = link->module_name;
    delete link;
    LOG_MODULE("BleDeviceManager", "close_input_link", LOG_INFO,
        "输入设备实例已注销:" << key.toStdString() << " 模块 " << module_name.toStdString() << " 其余 "
                              << input_links_.size() << " 台保持连接");
}

void BleDeviceManager::close_all_input_links() {
    const QStringList addresses = input_links_.keys();
    for (const QString& address : addresses) {
        close_input_link(address);
    }
}

void BleDeviceManager::sync_input_link_state(const QString& mac) {
    const QString key = LinkRegistry::normalize(mac);
    InputLink* link = input_links_.value(key, nullptr);
    if (link == nullptr || link->driver == nullptr) {
        return;
    }
    bool connected = false;
    int battery = -1;
    QString status;
    if (auto* paw = qobject_cast<PawPrintsBleController*>(link->driver); paw != nullptr) {
        connected = paw->connected();
        battery = paw->battery();
        status = paw->status();
    }
    else if (auto* civet = qobject_cast<CivetBleController*>(link->driver); civet != nullptr) {
        connected = civet->connected();
        battery = civet->battery();
        status = civet->status();
    }
    const bool was_connected = link->connected;
    link->connected = connected;
    link->battery = battery;
    link->status_text = status;
    if (connected && !was_connected) {
        emit inputLinkConnected(link);
    }
    emit inputLinkStatusChanged(link);
    if (!connected && was_connected) {
        // 设备自行掉线（断电 / 超距）：注销并销毁该 MAC 的实例，其它设备保持连接
        LOG_MODULE("BleDeviceManager", "sync_input_link_state", LOG_INFO,
            "输入设备链路自行掉线，已注销:" << key.toStdString());
        close_input_link(key);
    }
}

void BleDeviceManager::sync_input_link_status(const QString& mac) {
    const QString key = LinkRegistry::normalize(mac);
    InputLink* link = input_links_.value(key, nullptr);
    if (link == nullptr || link->driver == nullptr) {
        return;
    }
    if (auto* paw = qobject_cast<PawPrintsBleController*>(link->driver); paw != nullptr) {
        link->status_text = paw->status();
        link->battery = paw->battery();
    }
    else if (auto* civet = qobject_cast<CivetBleController*>(link->driver); civet != nullptr) {
        link->status_text = civet->status();
        link->battery = civet->battery();
    }
    emit inputLinkStatusChanged(link);
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
