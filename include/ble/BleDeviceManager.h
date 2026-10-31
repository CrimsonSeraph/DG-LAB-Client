/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "IOutputDevice.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <limits>

class CoyoteBleDevice;
class OpossumBleController;
class QBluetoothDeviceInfo;

// 链路登记与默认设备评选：扫描入口（CoyoteBleController）只负责发现设备与型号识别，
// BleDeviceManager 按 MAC 建立、登记与销毁驱动实例，并给出「当前已连接设备」快照。
// 每台设备各自持有连接状态、电量、强度回执与波形进度，断开一台不影响其它台。

/// @brief 设备类型的默认设备优先级（数值越小越优先）：郊狼 > 负鼠 > 其它
/// @param type 设备类型标识（coyote / opossum / unknown）
/// @return 优先级（0 郊狼 / 1 负鼠 / 2 其它）
inline int device_type_priority(const QString& type) {
    if (type == QStringLiteral("coyote")) {
        return 0;
    }
    if (type == QStringLiteral("opossum")) {
        return 1;
    }
    return 2;
}

/// @brief 默认设备评选候选（地址、类型标识、连接序号）
struct DefaultDeviceCandidate {
    QString address;                             ///< 设备 MAC
    QString type;                                ///< 类型标识（coyote / opossum / unknown）
    int order = std::numeric_limits<int>::max(); ///< 连接序号（越小越早，未登记按最大）
};

/// @brief 按「用户覆盖 → 郊狼 > 负鼠 > 其它 → 同类型先连接者」选出默认设备
/// @param candidates 在线候选（顺序无关）
/// @param configured 用户覆盖的设备 MAC（空串表示自动评选）
/// @return 候选下标；候选为空返回 -1，覆盖值不在候选内时回到自动评选
inline int pick_default_device(const QList<DefaultDeviceCandidate>& candidates, const QString& configured) {
    if (candidates.isEmpty()) {
        return -1;
    }
    const QString target = configured.trimmed();
    if (!target.isEmpty()) {
        for (int index = 0; index < candidates.size(); ++index) {
            if (candidates.at(index).address.compare(target, Qt::CaseInsensitive) == 0) {
                return index;
            }
        }
    }
    int best = 0;
    for (int index = 1; index < candidates.size(); ++index) {
        const int priority = device_type_priority(candidates.at(index).type);
        const int best_priority = device_type_priority(candidates.at(best).type);
        if (priority < best_priority) {
            best = index;
        }
        else if (priority == best_priority && candidates.at(index).order < candidates.at(best).order) {
            best = index;
        }
    }
    return best;
}

// LinkRegistry - MAC → 链路实例登记表（按 MAC 去重、记录连接先后，断开一台只移除该 MAC）
class LinkRegistry {
public:
    /// @brief MAC 归一（去空白、转大写；空串表示未识别）
    static QString normalize(const QString& mac) {
        return mac.trimmed().toUpper();
    }

    /// @brief 该 MAC 是否已登记
    bool contains(const QString& mac) const {
        return links_.contains(normalize(mac));
    }

    /// @brief 按 MAC 取设备（未登记返回 nullptr）
    IOutputDevice* find(const QString& mac) const {
        return links_.value(normalize(mac), nullptr);
    }

    /// @brief 登记设备并分配连接序号（同一 MAC 已登记时返回原实例，序号不变）
    /// @param device 驱动实例（地址取自 device->device_address()）
    /// @return 登记后的实例；地址为空或实例为空时返回 nullptr
    IOutputDevice* add(IOutputDevice* device) {
        if (device == nullptr) {
            return nullptr;
        }
        const QString key = normalize(device->device_address());
        if (key.isEmpty()) {
            return nullptr;
        }
        if (IOutputDevice* existing = links_.value(key, nullptr); existing != nullptr) {
            return existing;
        }
        links_.insert(key, device);
        order_.insert(key, ++counter_);
        sequence_.append(key);
        return device;
    }

    /// @brief 移除该 MAC 的登记（只影响该 MAC，其它设备与序号保持原样）
    /// @param mac 设备 MAC
    /// @return 该 MAC 原本已登记返回 true
    bool remove(const QString& mac) {
        const QString key = normalize(mac);
        if (links_.remove(key) == 0) {
            return false;
        }
        order_.remove(key);
        sequence_.removeAll(key);
        return true;
    }

    /// @brief 已登记设备数量
    qsizetype count() const {
        return links_.size();
    }

    /// @brief 已登记设备列表（按连接先后）
    QList<IOutputDevice*> all() const {
        QList<IOutputDevice*> list;
        for (const QString& key : sequence_) {
            if (IOutputDevice* device = links_.value(key, nullptr); device != nullptr) {
                list.append(device);
            }
        }
        return list;
    }

    /// @brief 已登记设备的 MAC 列表（按连接先后）
    QStringList addresses() const {
        return sequence_;
    }

    /// @brief 连接序号（同类型多台时先连接者优先；未登记返回 INT_MAX）
    /// @param mac 设备 MAC
    int order_of(const QString& mac) const {
        return order_.value(normalize(mac), std::numeric_limits<int>::max());
    }

    /// @brief 已连接设备的只读快照（MAC / 型号 / 类型 / 电量 / 连接序号）
    QVariantList snapshot() const {
        QVariantList list;
        for (const QString& key : sequence_) {
            IOutputDevice* device = links_.value(key, nullptr);
            if (device == nullptr || !device->connected()) {
                continue;
            }
            QVariantMap item;
            item.insert(QStringLiteral("address"), key);
            item.insert(QStringLiteral("label"), device->device_label());
            item.insert(QStringLiteral("model"), device->device_model());
            item.insert(QStringLiteral("type"), device->device_type());
            item.insert(QStringLiteral("battery"), device->battery());
            item.insert(QStringLiteral("order"), order_.value(key, 0));
            list.append(item);
        }
        return list;
    }

    /// @brief 清空登记表（不销毁实例，实例生命周期归调用方）
    void clear() {
        links_.clear();
        order_.clear();
        sequence_.clear();
        counter_ = 0;
    }

private:
    QHash<QString, IOutputDevice*> links_; ///< MAC → 驱动实例
    QHash<QString, int> order_;            ///< MAC → 连接序号
    QStringList sequence_;                 ///< 登记顺序（按 MAC，用于「先连接者优先」）
    int counter_ = 0;                      ///< 连接序号发号器
};

/// @brief 链路请求（扫描入口按型号识别结果向管理器申请驱动实例）
struct BleLinkRequest {
    QString type;  ///< 型号标识（coyote / opossum / pawprint / civet）
    QString model; ///< 型号段（如 47L121）
    QString label; ///< 型号显示名（如 郊狼 V3）
};

/// @brief 输入设备实例（爪印 / 灵猫）：驱动 + 实例数值模块名，以 MAC 为键登记
/// @note 指针由 BleDeviceManager 持有，inputLinkDestroyed 之后即失效，订阅方不得留存
struct InputLink {
    QString address;      ///< 设备 MAC（归一为大写）
    QString type;         ///< 型号标识（pawprint / civet）
    QString model;        ///< 型号段（如 47L120）
    QString label;        ///< 型号显示名（如 爪印）
    QString module_name;  ///< 实例数值模块名（如 paw_430C，同时是内置事件声明的模块标识）
    QString display_name; ///< 实例显示名（如 爪印 430C）
    QString status_text;  ///< 最近一次状态文案（驱动各自维护）
    int battery = -1;     ///< 电量百分比（-1 未知）
    bool connected = false; ///< 驱动是否已连上设备
    QObject* driver = nullptr; ///< 驱动实例（PawPrintsBleController / CivetBleController）
};

// InputModuleNames - 输入设备实例模块名分配表（同型号按 MAC 后四位取名，后四位重复时追加序号）
class InputModuleNames {
public:
    /// @brief MAC 后四位（去掉分隔符，取末尾 4 个十六进制字符并转大写；不足 4 位按实际长度）
    static QString mac_suffix(const QString& mac) {
        QString compact;
        for (const QChar ch : mac) {
            if (ch.isLetterOrNumber()) {
                compact.append(ch.toUpper());
            }
        }
        return compact.right(4);
    }

    /// @brief 不带冲突处理的基础模块名：<prefix>_<后四位>（MAC 无可用字符时退回 prefix）
    static QString base_name(const QString& prefix, const QString& mac) {
        const QString suffix = mac_suffix(mac);
        return suffix.isEmpty() ? prefix : prefix + QLatin1Char('_') + suffix;
    }

    /// @brief 实例显示名：<型号显示名> <后四位>（如「爪印 430C」）
    static QString display_name(const QString& label, const QString& mac) {
        const QString suffix = mac_suffix(mac);
        if (suffix.isEmpty()) {
            return label;
        }
        return label.isEmpty() ? suffix : label + QLatin1Char(' ') + suffix;
    }

    /// @brief 分配唯一模块名（同后四位冲突时依次追加 _2、_3…）
    /// @param prefix 型号前缀（paw / civet）
    /// @param mac 设备 MAC
    /// @param ordinal 输出：本次分配用的序号（1 表示未冲突；调用方据此记录消歧日志）
    /// @return 已占用的模块名
    QString acquire(const QString& prefix, const QString& mac, int* ordinal = nullptr) {
        const QString base = base_name(prefix, mac);
        QString name = base;
        int sequence = 1;
        while (names_.contains(name)) {
            ++sequence;
            name = base + QLatin1Char('_') + QString::number(sequence);
        }
        names_.append(name);
        if (ordinal != nullptr) {
            *ordinal = sequence;
        }
        return name;
    }

    /// @brief 释放该模块名（断开时调用，名字可被后续实例复用）
    /// @param module_name 已分配的模块名
    void release(const QString& module_name) {
        names_.removeAll(module_name);
    }

    /// @brief 当前已分配的模块名数量
    qsizetype count() const { return names_.size(); }

private:
    QStringList names_; ///< 已分配的模块名（防同名后四位互相覆盖）
};

// BleDeviceManager - 输出设备链路管理器（扫描之外的链路生命周期唯一归属）
// 职责：按识别出的型号 new 出对应驱动（郊狼 / 负鼠），以 MAC 为键登记；断开时销毁并注销。
// 没有任何互斥：多台设备可同时在线，各自的连接状态与电量互不覆盖。
class BleDeviceManager : public QObject {
    Q_OBJECT

public:
    explicit BleDeviceManager(QObject* parent = nullptr);
    ~BleDeviceManager() override;

    /// @brief 按型号为扫描到的设备建立链路实例并开始连接（型号未知或无驱动时拒绝）
    /// @param info 扫描结果（含地址与名称）
    /// @param request 型号识别结果
    /// @param error 输出：拒绝原因（成功时清空；可为空）
    /// @return 新建并登记的驱动实例；该 MAC 已登记或型号无驱动时返回 nullptr
    IOutputDevice* open_link(const QBluetoothDeviceInfo& info, const BleLinkRequest& request, QString* error = nullptr);

    /// @brief 断开并销毁指定 MAC 的链路实例（其它设备不受影响）
    /// @param mac 设备 MAC
    void close_link(const QString& mac);

    /// @brief 断开并销毁全部链路实例（应用退出）
    void close_all();

    /// @brief 按 MAC 取已登记链路（未登记时为空）
    /// @param mac 设备 MAC
    IOutputDevice* link_for(const QString& mac) const { return registry_.find(mac); }

    /// @brief 已登记链路数量
    qsizetype link_count() const { return registry_.count(); }

    /// @brief 已登记设备的 MAC 列表（按连接先后）
    QStringList link_addresses() const { return registry_.addresses(); }

    /// @brief 已连接设备的只读快照（MAC / 型号 / 电量 / 连接序号）
    QVariantList connected_links() const { return registry_.snapshot(); }

    /// @brief 连接序号（同类型多台时先连接者优先；未登记返回 INT_MAX）
    /// @param mac 设备 MAC
    int connection_order(const QString& mac) const { return registry_.order_of(mac); }

    /// @brief 链路状态文案（驱动各自维护，按 MAC 取；未登记为空）
    /// @param mac 设备 MAC
    QString status_text(const QString& mac) const { return status_text_.value(LinkRegistry::normalize(mac)); }

    /// @brief 该链路屏幕强度同步是否可用（郊狼恒为 true，负鼠按驱动回执）
    /// @param mac 设备 MAC
    bool screen_sync_available(const QString& mac) const {
        return screen_sync_.value(LinkRegistry::normalize(mac), true);
    }

    /// @brief 按型号为输入设备（爪印 / 灵猫）建立实例并按 MAC 登记（同型号可多台并存）
    /// @param info 扫描结果（含地址与名称）
    /// @param request 型号识别结果（type 为 pawprint / civet）
    /// @param error 输出：拒绝原因（成功时清空；可为空）
    /// @return 新建并登记的输入设备实例；该 MAC 已连接或型号无驱动时返回 nullptr
    InputLink* open_input_link(const QBluetoothDeviceInfo& info, const BleLinkRequest& request,
        QString* error = nullptr);

    /// @brief 断开并销毁指定 MAC 的输入设备实例（注销数值模块与内置声明由订阅方在 inputLinkDestroyed 里完成）
    /// @param mac 设备 MAC
    void close_input_link(const QString& mac);

    /// @brief 断开并销毁全部输入设备实例（应用退出）
    void close_all_input_links();

    /// @brief 按 MAC 取已登记的输入设备实例（未登记时为空）
    /// @param mac 设备 MAC
    InputLink* input_link_for(const QString& mac) const {
        return input_links_.value(LinkRegistry::normalize(mac), nullptr);
    }

    /// @brief 已登记输入设备的 MAC 列表（顺序不保证）
    QStringList input_link_addresses() const { return input_links_.keys(); }

signals:
    /// @brief 新建并登记了一台设备的链路实例（订阅方在此接入设备信号）
    /// @param device 新实例
    void linkCreated(IOutputDevice* device);

    /// @brief 一台设备的链路实例已注销（随后销毁）
    /// @param device 即将销毁的实例
    void linkDestroyed(IOutputDevice* device);

    /// @brief 一台设备的状态文案发生变化（快照读取方据此刷新状态）
    /// @param device 状态变化的实例
    void linkStatusChanged(IOutputDevice* device);

    /// @brief 新建并登记了一台输入设备实例（订阅方在此解析模块名并注册数值桥接）
    /// @param link 新实例
    void inputLinkCreated(InputLink* link);

    /// @brief 一台输入设备实例已注销（订阅方在此注销数值模块与内置事件声明，随后实例销毁）
    /// @param link 即将销毁的实例
    void inputLinkDestroyed(InputLink* link);

    /// @brief 输入设备首次连上（实验性提示等一次性动作在此触发）
    /// @param link 状态变化的实例
    void inputLinkConnected(InputLink* link);

    /// @brief 输入设备状态文案或电量变化（快照读取方据此刷新状态）
    /// @param link 状态变化的实例
    void inputLinkStatusChanged(InputLink* link);

private:
    /// @brief 订阅链路的掉线清理（设备自行断开时注销并销毁该 MAC 的实例）
    /// @param device 驱动实例
    /// @param mac 设备 MAC
    void watch_link(IOutputDevice* device, const QString& mac);

    /// @brief 订阅郊狼实例的状态文案信号
    /// @param device 郊狼实例
    /// @param mac 设备 MAC
    void watch_coyote(CoyoteBleDevice* device, const QString& mac);

    /// @brief 订阅负鼠实例的状态文案与屏幕同步信号
    /// @param device 负鼠实例
    /// @param mac 设备 MAC
    void watch_opossum(OpossumBleController* device, const QString& mac);

    /// @brief 订阅输入设备实例的状态变化（掉线时注销并销毁该 MAC 的实例）
    /// @param mac 设备 MAC
    void sync_input_link_state(const QString& mac);

    /// @brief 刷新输入设备实例的状态文案与电量并广播
    /// @param mac 设备 MAC
    void sync_input_link_status(const QString& mac);

    LinkRegistry registry_;              ///< MAC 登记表（输出设备）
    QHash<QString, QString> status_text_; ///< MAC → 最近一次状态文案
    QHash<QString, bool> screen_sync_;    ///< MAC → 屏幕强度同步是否可用
    QHash<QString, InputLink*> input_links_; ///< MAC → 输入设备实例（爪印 / 灵猫）
    InputModuleNames input_names_;        ///< 输入设备实例模块名分配表
};
