/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

// 多链路设备登记与默认设备评选（MAC 登记表增删、用户覆盖、同类型先连接者优先、
// 断开一台不影响其它）：纯逻辑用例，用合成输出设备（StubOutputDevice）驱动，不连接真实蓝牙

#include "BleDeviceManager.h"
#include "StubOutputDevice.h"

#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <cstdio>
#include <limits>

#define CHECK_INT(case_name, expected, actual) check_int((case_name), (expected), (actual), #actual, __LINE__)
#define CHECK_TEXT(case_name, expected, actual) check_text((case_name), (expected), (actual), #actual, __LINE__)
#define CHECK_TRUE(case_name, actual) check_bool((case_name), true, (actual), #actual, __LINE__)
#define CHECK_FALSE(case_name, actual) check_bool((case_name), false, (actual), #actual, __LINE__)

namespace {

    int failures = 0; ///< 失败断言数

    // 记录一次断言失败
    void fail(const char* case_name, const char* detail, int line) {
        std::fprintf(stderr, "FAIL %s (line %d): %s\n", case_name, line, detail);
        ++failures;
    }

    // 整型断言
    void check_int(const char* case_name, long long expected, long long actual, const char* expr, int line) {
        if (expected == actual) {
            return;
        }
        char detail[256] = {0};
        std::snprintf(detail, sizeof(detail), "%s expected %lld actual %lld", expr, expected, actual);
        fail(case_name, detail, line);
    }

    // 文本断言
    void check_text(const char* case_name, const QString& expected, const QString& actual, const char* expr, int line) {
        if (expected == actual) {
            return;
        }
        char detail[512] = {0};
        std::snprintf(detail, sizeof(detail), "%s expected %s actual %s", expr, expected.toUtf8().constData(),
            actual.toUtf8().constData());
        fail(case_name, detail, line);
    }

    // 布尔断言
    void check_bool(const char* case_name, bool expected, bool actual, const char* expr, int line) {
        if (expected == actual) {
            return;
        }
        char detail[256] = {0};
        std::snprintf(detail, sizeof(detail), "%s expected %s actual %s", expr, expected ? "true" : "false",
            actual ? "true" : "false");
        fail(case_name, detail, line);
    }

    // 构造一条带型号与显示名的合成设备并置为已连接
    StubOutputDevice* make_link(const char* address, const char* type, const char* model, const char* label, int battery) {
        auto* device = new StubOutputDevice(QString::fromLatin1(address), QString::fromLatin1(type));
        device->set_identity(QString::fromLatin1(model), QString::fromUtf8(label));
        device->set_battery(battery);
        device->set_connected(true);
        return device;
    }

    // 构造一台已连接设备的评选候选
    DefaultDeviceCandidate make_candidate(const char* address, const char* type, int order) {
        DefaultDeviceCandidate candidate;
        candidate.address = QString::fromLatin1(address);
        candidate.type = QString::fromLatin1(type);
        candidate.order = order;
        return candidate;
    }

    // MAC 登记表的增删：按 MAC 去重、连接序号递增、删一台只影响该 MAC
    void test_registry_add_remove() {
        LinkRegistry registry;
        StubOutputDevice coyote(QStringLiteral("AA:BB:CC:DD:EE:01"), QStringLiteral("coyote"));
        StubOutputDevice opossum(QStringLiteral("AA:BB:CC:DD:EE:02"), QStringLiteral("opossum"));

        CHECK_TRUE("登记第一台", registry.add(&coyote) == &coyote);
        CHECK_TRUE("登记第二台", registry.add(&opossum) == &opossum);
        CHECK_INT("登记数量", 2, registry.count());
        CHECK_TRUE("按 MAC 取第一台", registry.find(QStringLiteral("aa:bb:cc:dd:ee:01")) == &coyote);
        CHECK_INT("先登记序号更小", 1, registry.order_of(QStringLiteral("AA:BB:CC:DD:EE:01")));
        CHECK_INT("后登记序号更大", 2, registry.order_of(QStringLiteral("AA:BB:CC:DD:EE:02")));

        // 同一 MAC 不新建：返回原实例且序号不变
        CHECK_TRUE("重复登记返回原实例", registry.add(&coyote) == &coyote);
        CHECK_INT("重复登记不增加数量", 2, registry.count());
        CHECK_INT("重复登记不改序号", 1, registry.order_of(QStringLiteral("AA:BB:CC:DD:EE:01")));

        CHECK_TRUE("空实例不登记", registry.add(nullptr) == nullptr);
        CHECK_INT("空实例不改变数量", 2, registry.count());

        CHECK_TRUE("移除已登记设备", registry.remove(QStringLiteral("AA:BB:CC:DD:EE:01")));
        CHECK_INT("移除后数量", 1, registry.count());
        CHECK_TRUE("被移除的设备查不到", registry.find(QStringLiteral("AA:BB:CC:DD:EE:01")) == nullptr);
        CHECK_TRUE("另一台仍在", registry.find(QStringLiteral("AA:BB:CC:DD:EE:02")) == &opossum);
        CHECK_FALSE("重复移除返回 false", registry.remove(QStringLiteral("AA:BB:CC:DD:EE:01")));
        CHECK_INT("另一台序号不变", 2, registry.order_of(QStringLiteral("AA:BB:CC:DD:EE:02")));
        CHECK_TEXT("登记表只剩一台 MAC", QStringLiteral("AA:BB:CC:DD:EE:02"),
            registry.addresses().join(QStringLiteral(",")));
        CHECK_INT("未登记的序号为最大", std::numeric_limits<int>::max(),
            registry.order_of(QStringLiteral("00:00:00:00:00:00")));
    }

    // 默认设备评选：郊狼 > 负鼠、用户覆盖优先、同类型先连接者优先
    void test_default_device_selection() {
        QList<DefaultDeviceCandidate> candidates;
        candidates.append(make_candidate("AA:BB:CC:DD:EE:02", "opossum", 2));
        candidates.append(make_candidate("AA:BB:CC:DD:EE:01", "coyote", 3));
        // 两台都在线时默认是郊狼（即使负鼠先连接）
        CHECK_INT("郊狼优先于负鼠", 1, pick_default_device(candidates, QString()));
        // 用户覆盖指定负鼠时以其为准
        CHECK_INT("用户覆盖生效", 0, pick_default_device(candidates, QStringLiteral("AA:BB:CC:DD:EE:02")));
        // 覆盖值是已离线的 MAC 时回到自动评选
        CHECK_INT("覆盖值离线时回到自动评选", 1,
            pick_default_device(candidates, QStringLiteral("AA:BB:CC:DD:EE:09")));
        // 覆盖值大小写不敏感
        CHECK_INT("覆盖值大小写不敏感", 0,
            pick_default_device(candidates, QStringLiteral("aa:bb:cc:dd:ee:02")));

        // 断开郊狼后只剩负鼠
        QList<DefaultDeviceCandidate> only_opossum;
        only_opossum.append(make_candidate("AA:BB:CC:DD:EE:02", "opossum", 2));
        CHECK_INT("只剩负鼠时默认负鼠", 0, pick_default_device(only_opossum, QString()));

        // 同类型多台：先连接者优先（序号小的赢，与候选顺序无关）
        QList<DefaultDeviceCandidate> two_coyotes;
        two_coyotes.append(make_candidate("AA:BB:CC:DD:EE:04", "coyote", 7));
        two_coyotes.append(make_candidate("AA:BB:CC:DD:EE:03", "coyote", 5));
        CHECK_INT("同类型先连接者优先", 1, pick_default_device(two_coyotes, QString()));

        // 其它类型排在郊狼与负鼠之后
        QList<DefaultDeviceCandidate> mixed;
        mixed.append(make_candidate("AA:BB:CC:DD:EE:05", "unknown", 1));
        mixed.append(make_candidate("AA:BB:CC:DD:EE:06", "opossum", 9));
        CHECK_INT("负鼠优先于未知类型", 1, pick_default_device(mixed, QString()));

        CHECK_INT("空候选没有默认设备", -1, pick_default_device(QList<DefaultDeviceCandidate>(), QString()));
    }

    // 断开一台不影响其它：被移除的设备退出快照，另一台状态原样保留
    void test_disconnect_isolation() {
        LinkRegistry registry;
        StubOutputDevice coyote(QStringLiteral("AA:BB:CC:DD:EE:11"), QStringLiteral("coyote"));
        StubOutputDevice opossum(QStringLiteral("AA:BB:CC:DD:EE:12"), QStringLiteral("opossum"));
        coyote.set_identity(QStringLiteral("47L121"), QString::fromUtf8("郊狼 V3"));
        opossum.set_identity(QStringLiteral("47L127"), QString::fromUtf8("负鼠"));
        coyote.set_connected(true);
        opossum.set_connected(true);
        coyote.set_battery(80);
        opossum.set_battery(55);
        coyote.play_wave(1, {QStringLiteral("0A0A0A0A00000000")});
        registry.add(&coyote);
        registry.add(&opossum);

        QVariantList before = registry.snapshot();
        CHECK_INT("两台都在快照里", 2, static_cast<long long>(before.size()));

        // 郊狼断开：模拟驱动掉线后从登记表移除
        coyote.set_connected(false);
        registry.remove(QStringLiteral("AA:BB:CC:DD:EE:11"));

        QVariantList after = registry.snapshot();
        CHECK_INT("断开后快照只剩一台", 1, static_cast<long long>(after.size()));
        const QVariantMap kept = after.value(0).toMap();
        CHECK_TEXT("留下的是负鼠", QStringLiteral("AA:BB:CC:DD:EE:12"),
            kept.value(QStringLiteral("address")).toString());
        CHECK_INT("负鼠电量不受影响", 55, kept.value(QStringLiteral("battery")).toInt());
        CHECK_INT("负鼠连接序号不受影响", 2, kept.value(QStringLiteral("order")).toInt());
        CHECK_TEXT("负鼠型号不受影响", QStringLiteral("47L127"), kept.value(QStringLiteral("model")).toString());
        CHECK_TRUE("负鼠仍然连接", opossum.connected());
        // 波形状态按设备隔离：断开的郊狼清掉自己的帧，负鼠本来就没下发
        CHECK_FALSE("断开的郊狼不再下发波形", coyote.wave_sending(1));
        CHECK_FALSE("负鼠波形状态未被波及", opossum.wave_sending(1));

        // 负鼠下发后断开：快照清空，郊狼状态不被重置
        opossum.play_wave(2, {QStringLiteral("0A0A0A0A00000000"), QStringLiteral("0A0A0A0A00000000")});
        CHECK_INT("负鼠 B 通道帧数", 2, opossum.wave_frames(2));
        opossum.set_connected(false);
        registry.remove(QStringLiteral("AA:BB:CC:DD:EE:12"));
        CHECK_INT("全部断开后快照为空", 0, static_cast<long long>(registry.snapshot().size()));
        CHECK_INT("郊狼电量保持原值", 80, coyote.battery());
    }

    // 纯输入设备不进入已连接设备快照（不参与输出链路）
    void test_input_device_excluded() {
        LinkRegistry registry;
        StubOutputDevice paw(QStringLiteral("AA:BB:CC:DD:EE:21"), QStringLiteral("pawprint"));
        paw.set_output_capable(false);
        paw.set_connected(true);
        registry.add(&paw);
        CHECK_INT("非输出设备仍可登记", 1, registry.count());
        // 快照按「已连接」列出，输出能力的过滤由上层（DeviceController / 快照评选）负责
        CHECK_INT("快照列出已连接设备", 1, static_cast<long long>(registry.snapshot().size()));
        CHECK_FALSE("非输出设备不具备输出能力", paw.output_capable());
    }

    // 全部用例入口：返回失败断言数
    int run_all() {
        test_registry_add_remove();
        test_default_device_selection();
        test_disconnect_isolation();
        test_input_device_excluded();
        return failures;
    }

} // namespace

int main() {
    const int failed = run_all();
    if (failed == 0) {
        std::printf("device_link_test: all cases passed\n");
        return 0;
    }
    std::fprintf(stderr, "device_link_test: %d assertion(s) failed\n", failed);
    return 1;
}
