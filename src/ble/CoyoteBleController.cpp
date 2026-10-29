/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "CoyoteBleController.h"

#include "AppConfig.h"
#include "CivetBleController.h"
#include "DebugLog.h"
#include "PawPrintsBleController.h"

#include <QBluetoothDeviceDiscoveryAgent>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QLowEnergyController>
#include <QLowEnergyService>
#include <QPermissions>
#include <QTimer>
#include <QVariantMap>

#include <algorithm>

namespace {

    // 郊狼 V3 蓝牙 UUID（16 位短 UUID，基址 0000xxxx-0000-1000-8000-00805f9b34fb）
    const QBluetoothUuid kCoyoteService(static_cast<quint16>(0x180C));
    const QBluetoothUuid kWriteCharacteristic(static_cast<quint16>(0x150A));
    const QBluetoothUuid kNotifyCharacteristic(static_cast<quint16>(0x150B));
    const QBluetoothUuid kBatteryService(static_cast<quint16>(0x180A));
    const QBluetoothUuid kBatteryCharacteristic(static_cast<quint16>(0x1500));

    const char* kSilentFrame = "0A0A0A0A00000000";

    // 蓝牙名前缀 → 型号。GATT 与郊狼 V3 相同的设备靠名字区分，未登记的型号一律不可连接
    struct ModelEntry {
        const char* prefix;  ///< 蓝牙名前缀
        const char* label;   ///< 型号显示名
        bool has_driver;     ///< 本版是否已实现驱动（可连接）
        bool verified;       ///< 驱动是否经实机验证（未验证按实验性展示）
        bool blocked;        ///< 登记为「暂不支持」：型号已识别但本版不可连接（如固件过旧）
        bool output;         ///< 是否具备输出能力（纯输入设备不参与默认设备评选）
        const char* type;    ///< 类型标识（默认设备优先级用）
        const char* hint;    ///< 型号特定的行内提示（空串表示按三态生成）
    };
    // 前缀按「先长后短」排列：47L120100（爪印 V1.0）必须先命中，否则会落到 47L120
    constexpr ModelEntry kModelTable[] = {
        {"47L120100", "爪印 V1.0", false, false, true, false, "pawprint",
            "需先升级固件（V1.0 不可连接，请在 DG-LAB 3.0 APP 更新配件固件）"},
        {"47L120", "爪印", true, false, false, false, "pawprint", ""},
        {"47L121", "郊狼 V3", true, true, false, true, "coyote", ""},
        {"47L124", "灵猫", true, false, false, false, "civet",
            "实验性驱动已实现，未经实机验证"},
        {"47L127", "负鼠", true, false, false, true, "opossum", ""},
    };

    // 驱动支持程度：实机验证过的驱动为已支持，驱动已实现但未验证或未实现均为实验性
    IOutputDevice::SupportState support_state_of(bool has_driver, bool verified) {
        return (has_driver && verified) ? IOutputDevice::SupportState::Supported
                                        : IOutputDevice::SupportState::Experimental;
    }

    // 型号表条目 → 识别结果
    CoyoteBleController::DeviceProfile profile_of_entry(const ModelEntry& entry) {
        CoyoteBleController::DeviceProfile profile;
        profile.model = QString::fromLatin1(entry.prefix);
        profile.label = QString::fromUtf8(entry.label);
        profile.supported = entry.has_driver;
        profile.output = entry.output;
        profile.type = QString::fromLatin1(entry.type);
        profile.state = entry.blocked ? IOutputDevice::SupportState::Unsupported
                                      : support_state_of(entry.has_driver, entry.verified);
        profile.hint = QString::fromUtf8(entry.hint);
        return profile;
    }

    // 型号标识 → 型号表条目（MAC 记忆与首帧反推共用；跳过登记为不可连接的型号）
    const ModelEntry* entry_of_type(const QString& type) {
        if (type.isEmpty()) {
            return nullptr;
        }
        for (const ModelEntry& entry : kModelTable) {
            if (entry.blocked) {
                continue;
            }
            if (type.compare(QString::fromLatin1(entry.type), Qt::CaseInsensitive) == 0) {
                return &entry;
            }
        }
        return nullptr;
    }

    // 驱动支持程度的界面文案
    QString support_text_of(IOutputDevice::SupportState state) {
        if (state == IOutputDevice::SupportState::Supported) {
            return QStringLiteral("已支持");
        }
        if (state == IOutputDevice::SupportState::Experimental) {
            return QStringLiteral("实验性");
        }
        return QStringLiteral("暂不支持");
    }

    const char* kConnectedSuffix = " · 已连接"; ///< 扫描列表里已连接设备的行内标记

} // namespace

CoyoteBleController::CoyoteBleController(QObject* parent)
    : QObject(parent) {
}

CoyoteBleController::~CoyoteBleController() {
    // 链路实例归 BleDeviceManager：本类析构只停扫描，不再断开其它设备
    if (discovery_ != nullptr) {
        discovery_->stop();
        discovery_ = nullptr;
    }
}

void CoyoteBleController::initialize() {
    status_ = QStringLiteral("未连接");
    emit statusChanged();
    LOG_MODULE("CoyoteBleController", "initialize", LOG_INFO, "蓝牙扫描入口已就绪");
}

CoyoteBleController::DeviceProfile CoyoteBleController::identify(const QString& name) {
    return identify(name, QString());
}

CoyoteBleController::DeviceProfile CoyoteBleController::identify(const QString& name,
    const QString& remembered_model) {
    // 记忆优先于名称：操作系统会把广播名换成占位名，只有 MAC 记忆能还原型号语义
    const QString remembered = remembered_model.trimmed().toLower();
    if (const ModelEntry* entry = entry_of_type(remembered)) {
        return profile_of_entry(*entry);
    }
    // 名称命中型号表：用表里的型号、显示名与支持程度
    for (const ModelEntry& entry : kModelTable) {
        if (name.startsWith(QString::fromLatin1(entry.prefix))) {
            return profile_of_entry(entry);
        }
    }
    DeviceProfile profile;
    if (name.startsWith(QStringLiteral("47L1"))) {
        // 未登记的 47L1 前缀：型号段取前 6 位，标注不支持
        profile.model = name.left(6);
        profile.label = QStringLiteral("未登记型号");
        profile.hint = QStringLiteral("未登记的 DG-LAB 型号");
        return profile;
    }
    // 非 47L1 前缀（手机、耳机、手环等）：不是 DG-LAB 设备，一律不支持
    profile.label = QStringLiteral("未知设备");
    profile.hint = QStringLiteral("非 DG-LAB 设备");
    return profile;
}

QString CoyoteBleController::model_from_frame_head(quint8 head) {
    switch (head) {
    case 0xB1:
        return QStringLiteral("coyote");
    case 0xB3:
        return QStringLiteral("opossum");
    case 0x51:
    case 0x5A:
    case 0x5B:
    case 0x5C:
        return QStringLiteral("pawprint");
    default:
        break;
    }
    // 0xD0 在爪印与灵猫之间歧义，其余首字节无映射：都不猜
    return QString();
}

QString CoyoteBleController::remembered_model(const QString& mac) {
    const QString normalized = mac.trimmed().toUpper();
    if (normalized.isEmpty()) {
        return QString();
    }
    const std::string key = "app.devices." + normalized.toStdString() + ".model";
    const std::string stored = AppConfig::instance().get_value<std::string>(key, std::string());
    return QString::fromStdString(stored).trimmed().toLower();
}

void CoyoteBleController::remember_model(const QString& mac, const QString& model) {
    const QString normalized = mac.trimmed().toUpper();
    const QString value = model.trimmed().toLower();
    if (normalized.isEmpty() || value.isEmpty() || remembered_model(normalized) == value) {
        return;
    }
    auto& config = AppConfig::instance();
    const std::string key = "app.devices." + normalized.toStdString() + ".model";
    config.set_value_with_name<std::string>(key, value.toStdString(), "user");
    config.save_all();
    LOG_MODULE("CoyoteBleController", "remember_model", LOG_INFO,
        "记忆设备型号:" << normalized.toStdString() << " → " << value.toStdString());
}

void CoyoteBleController::attach_link_manager(BleDeviceManager* manager) {
    manager_ = manager;
    if (manager_ == nullptr) {
        return;
    }
    // 新链路的信号在这里接入：界面读到的默认设备快照随任何一台设备的变化刷新
    connect(manager_, &BleDeviceManager::linkCreated, this, [this](IOutputDevice* device) {
        watch_link(device);
        refresh_device_rows();
        emit stateChanged();
        emit statusChanged();
        emit connectedLinksChanged();
    });
    // 状态文案由驱动各自维护，快照读取方必须跟着刷新，否则界面会停在上一句
    connect(manager_, &BleDeviceManager::linkStatusChanged, this, [this](IOutputDevice*) {
        emit statusChanged();
    });
    connect(manager_, &BleDeviceManager::linkDestroyed, this, [this](IOutputDevice*) {
        refresh_device_rows();
        emit stateChanged();
        emit statusChanged();
        emit connectedLinksChanged();
    });
}

void CoyoteBleController::attach_pawprints(PawPrintsBleController* pawprints) {
    pawprints_ = pawprints;
    if (pawprints_ == nullptr) {
        return;
    }
    // 驱动先于本类销毁时清空指针
    connect(pawprints_, &QObject::destroyed, this, [this]() { pawprints_ = nullptr; });
    // 爪印是纯输入设备（不继承 IOutputDevice）：只刷新状态文案与列表，不进入输出链路与默认设备评选
    connect(pawprints_, &PawPrintsBleController::stateChanged, this, [this]() {
        refresh_device_rows();
        emit stateChanged();
        emit statusChanged();
    });
    connect(pawprints_, &PawPrintsBleController::statusChanged, this, [this]() { emit statusChanged(); });
}

void CoyoteBleController::attach_civet(CivetBleController* civet) {
    civet_ = civet;
    if (civet_ == nullptr) {
        return;
    }
    // 驱动先于本类销毁时清空指针
    connect(civet_, &QObject::destroyed, this, [this]() { civet_ = nullptr; });
    // 灵猫同为纯输入设备（不继承 IOutputDevice）：只刷新状态文案与列表，不进入输出链路与默认设备评选
    connect(civet_, &CivetBleController::stateChanged, this, [this]() {
        refresh_device_rows();
        emit stateChanged();
        emit statusChanged();
    });
    connect(civet_, &CivetBleController::statusChanged, this, [this]() { emit statusChanged(); });
}

void CoyoteBleController::watch_link(IOutputDevice* device) {
    if (device == nullptr) {
        return;
    }
    connect(device, &IOutputDevice::outputStateChanged, this, [this]() {
        refresh_device_rows();
        emit stateChanged();
        emit statusChanged();
        emit batteryChanged();
        emit connectedLinksChanged();
    });
    connect(device, &IOutputDevice::outputStrengthChanged, this, [this]() {
        emit strengthChanged();
        emit batteryChanged();
    });
    connect(device, &IOutputDevice::outputWaveChanged, this, &CoyoteBleController::waveStateChanged);
}

IOutputDevice* CoyoteBleController::snapshot_output() const {
    if (manager_ == nullptr) {
        return nullptr;
    }
    QList<IOutputDevice*> online;
    QList<DefaultDeviceCandidate> candidates;
    for (const QString& address : manager_->link_addresses()) {
        IOutputDevice* device = manager_->link_for(address);
        if (device == nullptr || !device->connected() || !device->output_capable()) {
            continue;
        }
        DefaultDeviceCandidate candidate;
        candidate.address = device->device_address();
        candidate.type = device->device_type();
        candidate.order = manager_->connection_order(address);
        online.append(device);
        candidates.append(candidate);
    }
    // 与 DeviceController 同一份评选实现：用户覆盖 → 郊狼 > 负鼠 → 同类型先连接者
    const QString configured = QString::fromStdString(AppConfig::instance().get_value<std::string>(
        "app.output.default_device", std::string()));
    const int index = pick_default_device(candidates, configured);
    return index < 0 ? nullptr : online.at(index);
}

bool CoyoteBleController::connected() const {
    return snapshot_output() != nullptr;
}

QString CoyoteBleController::device_name() const {
    const IOutputDevice* device = snapshot_output();
    if (device == nullptr) {
        return QString();
    }
    const QString label = device->device_label();
    return label.isEmpty() ? device->device_address() : label;
}

int CoyoteBleController::battery() const {
    const IOutputDevice* device = snapshot_output();
    return device == nullptr ? -1 : device->battery();
}

int CoyoteBleController::strength_a() const {
    const IOutputDevice* device = snapshot_output();
    return device == nullptr ? 0 : device->strength_a();
}

int CoyoteBleController::strength_b() const {
    const IOutputDevice* device = snapshot_output();
    return device == nullptr ? 0 : device->strength_b();
}

QString CoyoteBleController::device_address() const {
    const IOutputDevice* device = snapshot_output();
    return device == nullptr ? QString() : device->device_address();
}

QString CoyoteBleController::device_model() const {
    const IOutputDevice* device = snapshot_output();
    return device == nullptr ? QString() : device->device_model();
}

QString CoyoteBleController::status() const {
    if (const IOutputDevice* device = snapshot_output(); device != nullptr) {
        const QString text = manager_ == nullptr ? QString() : manager_->status_text(device->device_address());
        if (!text.isEmpty()) {
            return text;
        }
    }
    if (pawprints_ != nullptr && pawprints_->connected()) {
        return pawprints_->status();
    }
    if (civet_ != nullptr && civet_->connected()) {
        return civet_->status();
    }
    return status_;
}

bool CoyoteBleController::screen_sync_available() const {
    const IOutputDevice* device = snapshot_output();
    if (device == nullptr || manager_ == nullptr) {
        return true;
    }
    return manager_->screen_sync_available(device->device_address());
}

bool CoyoteBleController::paw_connected() const {
    return pawprints_ != nullptr && pawprints_->connected();
}

bool CoyoteBleController::civet_connected() const {
    return civet_ != nullptr && civet_->connected();
}

QVariantList CoyoteBleController::connected_links() const {
    return manager_ == nullptr ? QVariantList() : manager_->connected_links();
}

void CoyoteBleController::set_status(const QString& text) {
    if (status_ == text) {
        return;
    }
    status_ = text;
    emit statusChanged();
}

bool CoyoteBleController::is_connected_address(const QString& address) const {
    if (manager_ != nullptr && manager_->link_for(address) != nullptr) {
        return true;
    }
    // 输入设备本阶段只有一个固定实例：该 MAC 连着也在行上标出来
    if (pawprints_ != nullptr && pawprints_->connected()
        && pawprints_->device_address().compare(address, Qt::CaseInsensitive) == 0) {
        return true;
    }
    return civet_ != nullptr && civet_->connected()
        && civet_->device_address().compare(address, Qt::CaseInsensitive) == 0;
}

void CoyoteBleController::refresh_device_rows() {
    bool changed = false;
    for (int index = 0; index < devices_.size(); ++index) {
        QVariantMap item = devices_.at(index).toMap();
        const QString address = item.value(QStringLiteral("address")).toString();
        const bool connected = is_connected_address(address);
        if (item.value(QStringLiteral("connected")).toBool() == connected) {
            continue;
        }
        const QString base_name = item.value(QStringLiteral("baseName")).toString();
        item.insert(QStringLiteral("connected"), connected);
        item.insert(QStringLiteral("name"),
            connected ? base_name + QString::fromUtf8(kConnectedSuffix) : base_name);
        devices_[index] = item;
        changed = true;
    }
    if (changed) {
        emit devicesChanged();
    }
}

void CoyoteBleController::do_start_scan() {
    if (scanning_) return;

    // 清理旧数据
    devices_.clear();
    device_cache_.clear();
    emit devicesChanged();

    // 创建发现代理
    discovery_ = new QBluetoothDeviceDiscoveryAgent(this);
    discovery_->setLowEnergyDiscoveryTimeout(15000);

    connect(discovery_, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
        this, &CoyoteBleController::on_device_discovered);
    connect(discovery_, &QBluetoothDeviceDiscoveryAgent::finished,
        this, &CoyoteBleController::on_scan_finished);
    connect(discovery_, &QBluetoothDeviceDiscoveryAgent::errorOccurred, this,
        [this](QBluetoothDeviceDiscoveryAgent::Error error) {
            scanning_ = false;
            emit stateChanged();
            set_status(QStringLiteral("蓝牙扫描失败，错误码: %1").arg(static_cast<int>(error)));
            emit errorOccurred(QStringLiteral("蓝牙扫描失败，请检查权限和蓝牙开关"));
        });

    scanning_ = true;
    emit stateChanged();
    set_status(QStringLiteral("正在扫描附近的设备…"));
    discovery_->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
}

void CoyoteBleController::startScan() {
    if (scanning_) return;

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    QBluetoothPermission permission;
    permission.setCommunicationModes(QBluetoothPermission::Access);

    qApp->requestPermission(permission, [this](const QPermission& perm) {
        if (perm.status() == Qt::PermissionStatus::Granted) {
            do_start_scan();
        }
        else {
            set_status(QStringLiteral("蓝牙权限被拒绝"));
        }
    });
    return;
#else
    do_start_scan();
#endif
}

void CoyoteBleController::on_device_discovered(const QBluetoothDeviceInfo& info) {
    if (!(info.coreConfigurations() & QBluetoothDeviceInfo::LowEnergyCoreConfiguration)) {
        return;
    }
    // 使用地址作为唯一标识去重：同一 MAC 只保留一行，已有实例时行上标「已连接」
    QString address = info.address().toString();
    if (device_cache_.contains(address)) {
        return;
    }

    // 缓存完整的 QBluetoothDeviceInfo
    device_cache_.insert(address, info);

    // 加入 UI 列表：行上带「已支持 / 实验性 / 暂不支持」三态标签
    QVariantMap item;
    QString displayName = info.name().isEmpty() ? QStringLiteral("未知设备(未知名称)") : info.name();
    const DeviceProfile profile = identify(info.name(), remembered_model(address));
    const bool connected = manager_ != nullptr && manager_->link_for(address) != nullptr;
    item.insert(QStringLiteral("baseName"), displayName);
    item.insert(QStringLiteral("name"),
        connected ? displayName + QString::fromUtf8(kConnectedSuffix) : displayName);
    item.insert(QStringLiteral("address"), address);
    item.insert(QStringLiteral("model"), profile.model);
    item.insert(QStringLiteral("type"), profile.type);
    item.insert(QStringLiteral("connected"), connected);
    item.insert(QStringLiteral("supported"), profile.supported);
    item.insert(QStringLiteral("support"), static_cast<int>(profile.state));
    item.insert(QStringLiteral("supportText"), support_text_of(profile.state));
    item.insert(QStringLiteral("connectable"), profile.state != SupportState::Unsupported);
    item.insert(QStringLiteral("output"), profile.output);
    // 行内提示：型号特定的提示优先（未登记 / 非 DG-LAB 设备 / 需升级固件），实验性型号另有统一文案
    const QString model_text = profile.model.isEmpty() ? QString() : QStringLiteral("型号 %1，").arg(profile.model);
    QString hint;
    if (!profile.hint.isEmpty()) {
        // 型号特定提示优先（如爪印 V1.0 需先升级固件）
        hint = profile.hint;
    }
    else if (profile.state == SupportState::Experimental) {
        hint = profile.supported ? QStringLiteral("%1实验性驱动已实现，未经实机验证")
                                       .arg(model_text)
                                 : QStringLiteral("暂不可连接（%1驱动尚未实现）").arg(model_text);
    }
    else if (profile.state == SupportState::Unsupported) {
        hint = QStringLiteral("不可连接（%1型号未登记）").arg(model_text);
    }
    item.insert(QStringLiteral("hint"), hint);
    devices_.append(item);
    emit devicesChanged();
}

void CoyoteBleController::on_scan_finished() {
    scanning_ = false;
    emit stateChanged();
    set_status(devices_.isEmpty() ? QStringLiteral("未发现设备") : QStringLiteral("扫描完成，请选择设备"));
}

void CoyoteBleController::connectDevice(const QString& address) {
    if (address.isEmpty()) {
        return;
    }
    QBluetoothDeviceInfo target = device_cache_.value(address);

    if (!target.isValid()) {
        set_status(QStringLiteral("设备信息已过期，请重新扫描"));
        return;
    }

    const DeviceProfile profile = identify(target.name(), remembered_model(address));
    if (profile.state == SupportState::Unsupported) {
        // 型号未登记、非 DG-LAB 设备或本版明确不支持（如固件过旧）：拒绝连接，避免写入语义不匹配的指令
        LOG_MODULE("CoyoteBleController", "connectDevice", LOG_WARN,
            "拒绝连接未支持型号:" << target.name().toStdString() << " 型号 " << profile.model.toStdString());
        const QString reason = profile.hint.isEmpty()
            ? QStringLiteral("暂不支持该设备（型号 %1）").arg(profile.model)
            : QStringLiteral("暂不支持该设备：%1").arg(profile.hint);
        set_status(reason);
        emit errorOccurred(reason);
        return;
    }
    if (profile.state == SupportState::Experimental && !profile.supported) {
        // 实验性型号且驱动未实现：行可点选，但明确告知而不是假装可用
        LOG_MODULE("CoyoteBleController", "connectDevice", LOG_WARN,
            "实验性型号驱动尚未实现:" << target.name().toStdString() << " 型号 " << profile.model.toStdString());
        set_status(QStringLiteral("%1 驱动尚未实现").arg(profile.label));
        emit errorOccurred(QStringLiteral("%1（型号 %2）驱动尚未实现，本版无法连接")
            .arg(profile.label, profile.model));
        return;
    }
    if (profile.type == QStringLiteral("pawprint")) {
        // 爪印是纯输入设备：本阶段仍只有一个固定实例，多实例留到第二阶段
        if (pawprints_ == nullptr) {
            LOG_MODULE("CoyoteBleController", "connectDevice", LOG_WARN, "爪印驱动未接入，拒绝连接");
            set_status(QStringLiteral("爪印驱动未接入"));
            emit errorOccurred(QStringLiteral("爪印驱动未接入，本版无法连接该设备"));
            return;
        }
        if (pawprints_->connected()) {
            const bool same = pawprints_->device_address().compare(address, Qt::CaseInsensitive) == 0;
            set_status(same ? QStringLiteral("爪印已连接")
                            : QStringLiteral("爪印链路已被占用，输入设备多链路留到下一阶段"));
            if (!same) {
                emit errorOccurred(QStringLiteral("爪印只支持一条链路，输入设备多链路留到下一阶段"));
            }
            return;
        }
        pawprints_->connect_device(target);
        set_status(QStringLiteral("正在连接 %1…").arg(profile.label));
        return;
    }
    if (profile.type == QStringLiteral("civet")) {
        // 灵猫同为纯输入设备：本阶段仍只有一个固定实例
        if (civet_ == nullptr) {
            LOG_MODULE("CoyoteBleController", "connectDevice", LOG_WARN, "灵猫驱动未接入，拒绝连接");
            set_status(QStringLiteral("灵猫驱动未接入"));
            emit errorOccurred(QStringLiteral("灵猫驱动未接入，本版无法连接该设备"));
            return;
        }
        if (civet_->connected()) {
            const bool same = civet_->device_address().compare(address, Qt::CaseInsensitive) == 0;
            set_status(same ? QStringLiteral("灵猫已连接")
                            : QStringLiteral("灵猫链路已被占用，输入设备多链路留到下一阶段"));
            if (!same) {
                emit errorOccurred(QStringLiteral("灵猫只支持一条链路，输入设备多链路留到下一阶段"));
            }
            return;
        }
        civet_->connect_device(target);
        set_status(QStringLiteral("正在连接 %1…").arg(profile.label));
        return;
    }
    // 输出设备（郊狼 / 负鼠）：按 MAC 建实例，任意数量并存，不打断任何在用的链路
    if (manager_ == nullptr) {
        LOG_MODULE("CoyoteBleController", "connectDevice", LOG_ERROR, "链路管理器未接入，拒绝连接");
        set_status(QStringLiteral("蓝牙链路管理器未接入"));
        emit errorOccurred(QStringLiteral("蓝牙链路管理器未接入，无法连接该设备"));
        return;
    }
    if (const IOutputDevice* existing = manager_->link_for(address); existing != nullptr) {
        set_status(QStringLiteral("%1 已在连接中").arg(profile.label));
        return;
    }
    BleLinkRequest request;
    request.type = profile.type;
    request.model = profile.model;
    request.label = profile.label;
    QString error;
    if (manager_->open_link(target, request, &error) == nullptr) {
        set_status(error);
        emit errorOccurred(error);
        return;
    }
    // 首次成功识别出型号时写回记忆：名称命中与连接后首帧反推共用同一份记忆
    if (!profile.model.isEmpty()) {
        remember_model(address, profile.type);
    }
    set_status(QStringLiteral("正在连接 %1…").arg(profile.label));
}

void CoyoteBleController::disconnectDevice() {
    if (discovery_ != nullptr) {
        discovery_->stop();
        discovery_ = nullptr;
    }
    scanning_ = false;
    // 只断开当前链路：其它已连接的设备保持原状
    if (const IOutputDevice* device = snapshot_output(); device != nullptr && manager_ != nullptr) {
        const QString address = device->device_address();
        manager_->close_link(address);
    }
    else if (pawprints_ != nullptr && pawprints_->connected()) {
        pawprints_->disconnect_device();
    }
    else if (civet_ != nullptr && civet_->connected()) {
        civet_->disconnect_device();
    }
    set_status(QStringLiteral("蓝牙已断开"));
    refresh_device_rows();
    emit stateChanged();
}

void CoyoteBleController::setStrength(int channel, int mode, int value) {
    if (IOutputDevice* device = snapshot_output(); device != nullptr) {
        device->set_strength(channel, mode, value);
    }
}

void CoyoteBleController::playWave(int channel, const QStringList& frames) {
    if (IOutputDevice* device = snapshot_output(); device != nullptr) {
        device->play_wave(channel, frames);
    }
}

void CoyoteBleController::stopWave(int channel) {
    if (IOutputDevice* device = snapshot_output(); device != nullptr) {
        device->stop_wave(channel);
    }
}

void CoyoteBleController::applyLimits(int limitA, int limitB, int freqBalance, int strengthBalance) {
    Q_UNUSED(freqBalance);
    Q_UNUSED(strengthBalance);
    // 软上限按设备下发：每台设备各自一份，互不覆盖
    if (manager_ == nullptr) {
        return;
    }
    for (const QString& address : manager_->link_addresses()) {
        IOutputDevice* device = manager_->link_for(address);
        if (device != nullptr && device->connected()) {
            device->apply_limits(limitA, limitB);
        }
    }
}

CoyoteBleDevice::CoyoteBleDevice(QObject* parent)
    : IOutputDevice(parent)
    , write_char_uuid_(kWriteCharacteristic)
    , notify_char_uuid_(kNotifyCharacteristic) {
    // 接口信号转发既有信号：发射时机与次数不变，避免上层依赖具体实现
    connect(this, &CoyoteBleDevice::stateChanged, this, &IOutputDevice::outputStateChanged);
    connect(this, &CoyoteBleDevice::strengthChanged, this, &IOutputDevice::outputStrengthChanged);
    connect(this, &CoyoteBleDevice::waveStateChanged, this, &IOutputDevice::outputWaveChanged);
    tick_timer_ = std::make_unique<QTimer>(this);
    tick_timer_->setInterval(100);
    connect(tick_timer_.get(), &QTimer::timeout, this, [this]() {
        if (connected_ && ready_) {
            write_b0();
        }
    });
    battery_timer_ = std::make_unique<QTimer>(this);
    battery_timer_->setInterval(60000);
    connect(battery_timer_.get(), &QTimer::timeout, this, [this]() { read_battery(); });
}

CoyoteBleDevice::~CoyoteBleDevice() {
    disconnect_device();
}

QString CoyoteBleDevice::display_name() const {
    // 状态文案与界面的设备名同源：型号显示名优先，其次蓝牙名
    return device_label_.isEmpty() ? device_name_ : device_label_;
}

void CoyoteBleDevice::adopt_profile(const CoyoteBleController::DeviceProfile& profile) {
    if (profile.model.isEmpty() || !device_model_.isEmpty()) {
        return;
    }
    device_model_ = profile.model;
    device_label_ = profile.label;
    device_type_ = profile.type;
    device_output_capable_ = profile.output;
    support_state_ = profile.state;
    emit stateChanged();
}

void CoyoteBleDevice::set_status(const QString& text) {
    if (status_ == text) {
        return;
    }
    status_ = text;
    emit statusChanged();
}

const CoyoteBleDevice::ChannelState& CoyoteBleDevice::state_of(int channel) const {
    return (channel == 1) ? channel_a_ : channel_b_;
}

bool CoyoteBleDevice::wave_sending(int channel) const {
    return !state_of(channel).frames.isEmpty();
}

int CoyoteBleDevice::wave_frame(int channel) const {
    const ChannelState& state = state_of(channel);
    if (state.frames.isEmpty()) {
        return 0;
    }
    // frame_index 指向下一帧，已下发的那一帧是它的前一格（1 起计数）
    return (state.frame_index + state.frames.size() - 1) % state.frames.size() + 1;
}

int CoyoteBleDevice::wave_frames(int channel) const {
    return state_of(channel).frames.size();
}

QByteArray CoyoteBleDevice::frame_for_channel(const QStringList& frames, int index) const {
    if (frames.isEmpty()) {
        return QByteArray::fromHex(kSilentFrame);
    }
    const QByteArray bytes = QByteArray::fromHex(frames.at(index % frames.size()).toLatin1());
    return bytes.size() == 8 ? bytes : QByteArray::fromHex(kSilentFrame);
}

void CoyoteBleDevice::connect_device(const QBluetoothDeviceInfo& info, const QString& model, const QString& label) {
    if (!info.isValid() || controller_ != nullptr) {
        return;
    }
    device_address_ = info.address().toString();
    device_name_ = info.name().isEmpty() ? QStringLiteral("郊狼设备") : info.name();
    device_model_ = model;
    device_label_ = label.isEmpty() ? device_name_ : label;
    device_type_ = QStringLiteral("coyote");
    device_output_capable_ = true;
    support_state_ = SupportState::Supported;

    // 用完整的扫描结果对象创建控制器
    controller_ = QLowEnergyController::createCentral(info, this);

    connect(controller_, &QLowEnergyController::connected, this, &CoyoteBleDevice::on_controller_connected);
    connect(controller_, &QLowEnergyController::disconnected, this, &CoyoteBleDevice::on_controller_disconnected);
    connect(controller_, &QLowEnergyController::serviceDiscovered, this, [](const QBluetoothUuid&) {});
    connect(controller_, &QLowEnergyController::discoveryFinished, this, [this]() {
        if (controller_ == nullptr) {
            return;
        }
        coyote_service_ = controller_->createServiceObject(kCoyoteService, this);
        if (coyote_service_ != nullptr) {
            connect(coyote_service_, &QLowEnergyService::stateChanged,
                this, &CoyoteBleDevice::on_service_state_changed);
            connect(coyote_service_, &QLowEnergyService::characteristicChanged,
                this, &CoyoteBleDevice::on_characteristic_changed);
            coyote_service_->discoverDetails();
            connect(coyote_service_, QOverload<QLowEnergyService::ServiceError>::of(&QLowEnergyService::errorOccurred),
                this, [this](QLowEnergyService::ServiceError error) {
                    LOG_MODULE("CoyoteBleDevice", "connect_device", LOG_ERROR, "服务错误:" << error);
                    set_status(QStringLiteral("蓝牙服务错误: %1").arg(error));
                });
            connect(coyote_service_, &QLowEnergyService::characteristicWritten,
                this, [](const QLowEnergyCharacteristic& c, const QByteArray& value) {
                    LOG_MODULE("CoyoteBleDevice", "connect_device", LOG_INFO,
                        "写入成功:" << c.uuid().toString().toStdString() << " " << value.toHex().toStdString());
                });
        }
        battery_service_ = controller_->createServiceObject(kBatteryService, this);
        if (battery_service_ != nullptr) {
            connect(battery_service_, &QLowEnergyService::stateChanged, this,
                [this](QLowEnergyService::ServiceState state) {
                    if (state == QLowEnergyService::RemoteServiceDiscovered) {
                        subscribe_battery();
                    }
                });
            connect(battery_service_, &QLowEnergyService::characteristicChanged, this,
                [this](const QLowEnergyCharacteristic& characteristic, const QByteArray& value) {
                    if (characteristic.uuid() == kBatteryCharacteristic) {
                        apply_battery_value(value);
                    }
                });
            // 主动读取的回包同样带电量：不接这个信号读回值会被丢弃
            connect(battery_service_, &QLowEnergyService::characteristicRead, this,
                [this](const QLowEnergyCharacteristic& characteristic, const QByteArray& value) {
                    if (characteristic.uuid() == kBatteryCharacteristic) {
                        apply_battery_value(value);
                    }
                });
            battery_service_->discoverDetails();
        }
    });
    connect(controller_, &QLowEnergyController::errorOccurred, this,
        [this](QLowEnergyController::Error) {
            set_status(QStringLiteral("蓝牙连接失败"));
        });

    set_status(QStringLiteral("正在连接 %1…").arg(display_name()));
    controller_->connectToDevice();
}

void CoyoteBleDevice::on_controller_connected() {
    // 重连时旧控制器可能补发信号，只认当前控制器的事件
    if (sender() != controller_) {
        return;
    }
    connected_ = true;
    emit stateChanged();
    set_status(QStringLiteral("已连接 %1，正在发现服务…").arg(display_name()));
    for (auto& channel : {&channel_a_, &channel_b_}) {
        channel->frames.clear();
        channel->frame_index = 0;
        channel->pending_mode = 0;
        channel->waiting_ack = false;
    }
    if (controller_ != nullptr) {
        controller_->discoverServices();
    }
}

void CoyoteBleDevice::on_controller_disconnected() {
    // 重连时旧控制器可能补发信号，只认当前控制器的事件
    if (sender() != controller_) {
        return;
    }
    LOG_MODULE("CoyoteBleDevice", "on_controller_disconnected", LOG_INFO, "蓝牙连接已断开");
    connected_ = false;
    ready_ = false;
    strength_a_ = 0;
    strength_b_ = 0;
    battery_ = -1;
    tick_timer_->stop();
    battery_timer_->stop();
    for (auto& channel : {&channel_a_, &channel_b_}) {
        channel->frames.clear();
        channel->frame_index = 0;
    }
    emit stateChanged();
    emit strengthChanged();
    emit batteryChanged();
    emit waveStateChanged();
    set_status(QStringLiteral("蓝牙已断开"));
}

void CoyoteBleDevice::on_service_state_changed(QLowEnergyService::ServiceState state) {
    if (state == QLowEnergyService::RemoteServiceDiscovering) {
        return;
    }
    if (state != QLowEnergyService::RemoteServiceDiscovered) {
        return;
    }
    ensure_characteristics();
}

void CoyoteBleDevice::ensure_characteristics() {
    if (coyote_service_ == nullptr) {
        return;
    }
    const auto write_char = coyote_service_->characteristic(write_char_uuid_);
    const auto notify_char = coyote_service_->characteristic(notify_char_uuid_);

    LOG_MODULE("CoyoteBleDevice", "ensure_characteristics", LOG_DEBUG, "写特征是否有效:" << write_char.isValid());
    LOG_MODULE("CoyoteBleDevice", "ensure_characteristics", LOG_DEBUG, "通知特征是否有效:" << notify_char.isValid());

    if (!write_char.isValid() || !notify_char.isValid()) {
        set_status(QStringLiteral("未找到郊狼写入/通知特征"));
        return;
    }
    // 写入 CCCD 打开通知（0x0100 小端）
    const auto cccd = notify_char.descriptor(QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration);
    if (cccd.isValid()) {
        coyote_service_->writeDescriptor(cccd, QByteArray::fromHex("0100"));
    }

    ready_ = true;
    emit stateChanged();
    set_status(QStringLiteral("已连接 %1").arg(display_name()));
    applyLimits(limit_a_, limit_b_, 128, 128);
    // 重连后重发上次绝对强度：设备不会保留断连前的目标值
    if (last_set_a_ >= 0) {
        set_strength(1, 2, last_set_a_);
    }
    if (last_set_b_ >= 0) {
        set_strength(2, 2, last_set_b_);
    }
    tick_timer_->start();
}

void CoyoteBleDevice::subscribe_battery() {
    if (battery_service_ == nullptr) {
        return;
    }
    const auto characteristic = battery_service_->characteristic(kBatteryCharacteristic);
    if (!characteristic.isValid()) {
        LOG_MODULE("CoyoteBleDevice", "subscribe_battery", LOG_WARN, "未找到电量特征");
        return;
    }
    // 支持 Notify 时写入 CCCD 订阅电量变化，让电量能随时间刷新
    const auto cccd = characteristic.descriptor(QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration);
    if (cccd.isValid() && (characteristic.properties() & QLowEnergyCharacteristic::Notify)) {
        battery_service_->writeDescriptor(cccd, QByteArray::fromHex("0100"));
    }
    read_battery();
    battery_timer_->start();
}

void CoyoteBleDevice::read_battery() {
    if (battery_service_ == nullptr || !connected_) {
        return;
    }
    const auto characteristic = battery_service_->characteristic(kBatteryCharacteristic);
    if (!characteristic.isValid()) {
        return;
    }
    battery_service_->readCharacteristic(characteristic);
}

void CoyoteBleDevice::apply_battery_value(const QByteArray& value) {
    if (value.isEmpty()) {
        return;
    }
    const int percent = static_cast<quint8>(value.at(0));
    LOG_MODULE("CoyoteBleDevice", "apply_battery_value", LOG_INFO,
        "电量回传:" << value.toHex().toStdString() << " → " << percent << "%");
    if (percent == battery_) {
        return;
    }
    battery_ = percent;
    emit batteryChanged();
}

void CoyoteBleDevice::on_characteristic_changed(const QLowEnergyCharacteristic& characteristic,
    const QByteArray& value) {
    if (characteristic.uuid() == kBatteryCharacteristic) {
        if (!value.isEmpty()) {
            battery_ = static_cast<quint8>(value.at(0));
            emit batteryChanged();
        }
        return;
    }
    // 尚未识别出型号时按首帧反推：只认能唯一判定的首字节，已有记忆或名称命中时不覆盖
    if (device_model_.isEmpty() && !value.isEmpty()) {
        const quint8 head = static_cast<quint8>(value.at(0));
        const QString inferred = CoyoteBleController::model_from_frame_head(head);
        if (inferred.isEmpty()) {
            if (head == 0xD0 && !model_ambiguity_logged_) {
                model_ambiguity_logged_ = true;
                LOG_MODULE("CoyoteBleDevice", "on_characteristic_changed", LOG_DEBUG,
                    "无法据首帧判定型号: 首字节 D0 在爪印与灵猫之间歧义");
            }
        }
        else if (const QString remembered = CoyoteBleController::remembered_model(device_address_);
                 !remembered.isEmpty() && remembered != inferred) {
            if (!model_conflict_logged_) {
                model_conflict_logged_ = true;
                LOG_MODULE("CoyoteBleDevice", "on_characteristic_changed", LOG_WARN,
                    "首帧反推型号与记忆不一致，以记忆为准: 记忆 " << remembered.toStdString()
                                                                << " 首帧 " << inferred.toStdString());
            }
        }
        else {
            const CoyoteBleController::DeviceProfile inferred_profile =
                CoyoteBleController::identify(QString(), inferred);
            if (!inferred_profile.model.isEmpty()) {
                adopt_profile(inferred_profile);
                CoyoteBleController::remember_model(device_address_, inferred_profile.type);
                LOG_MODULE("CoyoteBleDevice", "on_characteristic_changed", LOG_INFO,
                    "按首帧反推型号: " << inferred_profile.type.toStdString() << " 型号段 "
                                       << inferred_profile.model.toStdString());
            }
        }
    }
    if (!value.isEmpty() && static_cast<quint8>(value.at(0)) == 0x53) {
        // 设备自报身份帧（0x53 + MAC）：只记一次，避免 100ms 级刷屏
        if (!identity_frame_logged_) {
            identity_frame_logged_ = true;
            LOG_MODULE("CoyoteBleDevice", "on_characteristic_changed", LOG_DEBUG,
                "设备身份帧:" << value.toHex().toStdString());
        }
        return;
    }
    if (value.size() >= 4 && static_cast<quint8>(value.at(0)) == 0xB1) {
        const int seq = static_cast<quint8>(value.at(1));
        strength_a_ = static_cast<quint8>(value.at(2));
        strength_b_ = static_cast<quint8>(value.at(3));
        if (channel_a_.waiting_ack && seq == channel_a_.pending_seq) {
            channel_a_.waiting_ack = false;
        }
        if (channel_b_.waiting_ack && seq == channel_b_.pending_seq) {
            channel_b_.waiting_ack = false;
        }
        emit strengthChanged();
    }
}

QByteArray CoyoteBleDevice::build_b0() {
    int a_mode = 0;
    int b_mode = 0;
    int a_value = 0;
    int b_value = 0;
    bool has_op = false;

    if (channel_a_.pending_mode != 0 && !channel_a_.waiting_ack) {
        a_mode = channel_a_.pending_mode;
        a_value = channel_a_.pending_value;
        has_op = true;
    }
    if (channel_b_.pending_mode != 0 && !channel_b_.waiting_ack) {
        b_mode = channel_b_.pending_mode;
        b_value = channel_b_.pending_value;
        has_op = true;
    }

    int seq = 0;
    if (has_op) {
        seq = ++sequence_;
        if (seq > 15) {
            sequence_ = 1;
            seq = 1;
        }
    }

    const QByteArray frame_a = frame_for_channel(channel_a_.frames, channel_a_.frame_index);
    const QByteArray frame_b = frame_for_channel(channel_b_.frames, channel_b_.frame_index);

    QByteArray data;
    data.append(static_cast<char>(0xB0));
    data.append(static_cast<char>(((seq & 0x0F) << 4) | ((a_mode & 0x03) << 2) | (b_mode & 0x03)));
    data.append(static_cast<char>(a_value & 0xFF));
    data.append(static_cast<char>(b_value & 0xFF));
    data.append(frame_a.mid(0, 4));
    data.append(frame_a.mid(4, 4));
    data.append(frame_b.mid(0, 4));
    data.append(frame_b.mid(4, 4));
    return data;
}

void CoyoteBleDevice::write_b0() {
    if (coyote_service_ == nullptr) {
        return;
    }
    const auto write_char = coyote_service_->characteristic(write_char_uuid_);
    if (!write_char.isValid()) {
        return;
    }

    const QByteArray data = build_b0();
    coyote_service_->writeCharacteristic(write_char, data, QLowEnergyService::WriteWithoutResponse);

    // 记录本 tick 使用的序号，等待 B1 回执后再接受下一次强度输入
    const int seq = (static_cast<quint8>(data.at(1)) >> 4) & 0x0F;
    if (seq != 0) {
        if (channel_a_.pending_mode != 0 && !channel_a_.waiting_ack) {
            channel_a_.waiting_ack = true;
            channel_a_.pending_seq = seq;
            channel_a_.pending_mode = 0;
        }
        if (channel_b_.pending_mode != 0 && !channel_b_.waiting_ack) {
            channel_b_.waiting_ack = true;
            channel_b_.pending_seq = seq;
            channel_b_.pending_mode = 0;
        }
    }

    if (!channel_a_.frames.isEmpty()) {
        channel_a_.frame_index = (channel_a_.frame_index + 1) % channel_a_.frames.size();
    }
    if (!channel_b_.frames.isEmpty()) {
        channel_b_.frame_index = (channel_b_.frame_index + 1) % channel_b_.frames.size();
    }
    if (!channel_a_.frames.isEmpty() || !channel_b_.frames.isEmpty()) {
        emit waveStateChanged();
    }
}

void CoyoteBleDevice::write_bf() {
    if (coyote_service_ == nullptr) {
        return;
    }
    const auto write_char = coyote_service_->characteristic(write_char_uuid_);
    if (!write_char.isValid()) {
        return;
    }
    QByteArray data;
    data.append(static_cast<char>(0xBF));
    data.append(static_cast<char>(limit_a_ & 0xFF));
    data.append(static_cast<char>(limit_b_ & 0xFF));
    data.append(static_cast<char>(128));
    data.append(static_cast<char>(128));
    data.append(static_cast<char>(128));
    data.append(static_cast<char>(128));
    coyote_service_->writeCharacteristic(write_char, data, QLowEnergyService::WriteWithResponse);
}

void CoyoteBleDevice::applyLimits(int limitA, int limitB, int freqBalance, int strengthBalance) {
    limit_a_ = std::clamp(limitA, 0, 200);
    limit_b_ = std::clamp(limitB, 0, 200);
    Q_UNUSED(freqBalance);
    Q_UNUSED(strengthBalance);
    write_bf();
}

void CoyoteBleDevice::set_strength(int channel, int mode, int value) {
    ChannelState& state = (channel == 1) ? channel_a_ : channel_b_;
    if (mode == 3 || mode == 4) {
        // 连续增减：按次数转换为单次增减（沿用规则引擎的 0/1/2 语义）
        state.pending_mode = (mode == 3) ? 2 : 1;
        state.pending_value = 1;
        return;
    }
    if (mode == 0) {
        state.pending_mode = 2;
        state.pending_value = std::max(1, value);
    }
    else if (mode == 1) {
        state.pending_mode = 1;
        state.pending_value = std::max(1, value);
    }
    else {
        state.pending_mode = 3;
        state.pending_value = std::clamp(value, 0, 200);
        // 记录最后一条绝对强度：重连后按它重发，避免界面值与设备实际强度脱节
        if (channel == 1) {
            last_set_a_ = state.pending_value;
        }
        else {
            last_set_b_ = state.pending_value;
        }
    }
}

void CoyoteBleDevice::play_wave(int channel, const QStringList& frames) {
    ChannelState& state = (channel == 1) ? channel_a_ : channel_b_;
    state.frames = frames;
    state.frame_index = 0;
    emit waveStateChanged();
}

void CoyoteBleDevice::stop_wave(int channel) {
    ChannelState& state = (channel == 1) ? channel_a_ : channel_b_;
    state.frames.clear();
    state.frame_index = 0;
    emit waveStateChanged();
}

void CoyoteBleDevice::disconnect_device() {
    tick_timer_->stop();
    battery_timer_->stop();
    if (controller_ != nullptr) {
        controller_->disconnectFromDevice();
        controller_->deleteLater();
        controller_ = nullptr;
    }
    coyote_service_ = nullptr;
    battery_service_ = nullptr;
    for (auto& channel : {&channel_a_, &channel_b_}) {
        channel->frames.clear();
        channel->frame_index = 0;
    }
    connected_ = false;
    ready_ = false;
    strength_a_ = 0;
    strength_b_ = 0;
    battery_ = -1;
    // 设备标识随连接失效：离线设备不参与默认设备评选，配置键仍在下次连接后继续生效
    device_address_.clear();
    device_label_.clear();
    device_model_.clear();
    emit stateChanged();
    emit strengthChanged();
    emit batteryChanged();
    emit waveStateChanged();
    set_status(QStringLiteral("蓝牙已断开"));
}
