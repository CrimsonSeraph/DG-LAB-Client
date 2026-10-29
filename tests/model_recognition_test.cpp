/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

// 型号识别测试：名称命中型号表、名称未命中时按 MAC 记忆补齐、首帧反推（含 0xD0 歧义返回未知）
// 全部用例只调纯函数（CoyoteBleController::identify / model_from_frame_head），不连接设备

#include "CoyoteBleController.h"

#include <QString>

#include <cstdio>
#include <cstdlib>

namespace {

    int failures = 0; ///< 失败断言数

    // 记录一次断言失败
    void fail(const char* case_name, const char* detail, int line) {
        std::fprintf(stderr, "FAIL %s (line %d): %s\n", case_name, line, detail);
        ++failures;
    }

    // 断言字符串相等
    void expect_text(const char* case_name, const QString& actual, const QString& expected, int line) {
        if (actual == expected) {
            return;
        }
        const QByteArray a = actual.toUtf8();
        const QByteArray e = expected.toUtf8();
        const std::string detail = std::string("expected \"") + e.constData() + "\" but got \""
            + a.constData() + "\"";
        fail(case_name, detail.c_str(), line);
    }

    // 断言整数相等
    void expect_int(const char* case_name, int actual, int expected, int line) {
        if (actual == expected) {
            return;
        }
        const std::string detail = "expected " + std::to_string(expected) + " but got "
            + std::to_string(actual);
        fail(case_name, detail.c_str(), line);
    }

    // 断言布尔相等
    void expect_bool(const char* case_name, bool actual, bool expected, int line) {
        if (actual == expected) {
            return;
        }
        fail(case_name, expected ? "expected true but got false" : "expected false but got true", line);
    }

#define EXPECT_TEXT(case_name, actual, expected) expect_text(case_name, (actual), (expected), __LINE__)
#define EXPECT_INT(case_name, actual, expected) expect_int(case_name, (actual), (expected), __LINE__)
#define EXPECT_BOOL(case_name, actual, expected) expect_bool(case_name, (actual), (expected), __LINE__)

    using SupportState = CoyoteBleController::SupportState;

    // 名称命中型号表：五个登记型号 + 未登记的 47L1 前缀 + 非 47L1 名字（后两类不支持）
    void test_identify_by_name() {
        const char* c = "identify_by_name";
        auto coyote = CoyoteBleController::identify(QStringLiteral("47L121000"));
        EXPECT_TEXT(c, coyote.model, QStringLiteral("47L121"));
        EXPECT_TEXT(c, coyote.label, QStringLiteral("郊狼 V3"));
        EXPECT_TEXT(c, coyote.type, QStringLiteral("coyote"));
        EXPECT_BOOL(c, coyote.supported, true);
        EXPECT_BOOL(c, coyote.output, true);
        EXPECT_INT(c, static_cast<int>(coyote.state), static_cast<int>(SupportState::Supported));
        EXPECT_TEXT(c, coyote.hint, QString());

        auto paw_v11 = CoyoteBleController::identify(QStringLiteral("47L120300"));
        EXPECT_TEXT(c, paw_v11.model, QStringLiteral("47L120"));
        EXPECT_BOOL(c, paw_v11.output, false);
        EXPECT_INT(c, static_cast<int>(paw_v11.state), static_cast<int>(SupportState::Experimental));

        auto paw_v10 = CoyoteBleController::identify(QStringLiteral("47L120100"));
        EXPECT_TEXT(c, paw_v10.model, QStringLiteral("47L120100"));
        EXPECT_INT(c, static_cast<int>(paw_v10.state), static_cast<int>(SupportState::Unsupported));
        EXPECT_BOOL(c, paw_v10.hint.contains(QStringLiteral("固件")), true);

        auto civet = CoyoteBleController::identify(QStringLiteral("47L124000"));
        EXPECT_TEXT(c, civet.model, QStringLiteral("47L124"));
        EXPECT_TEXT(c, civet.type, QStringLiteral("civet"));
        EXPECT_INT(c, static_cast<int>(civet.state), static_cast<int>(SupportState::Experimental));

        auto opossum = CoyoteBleController::identify(QStringLiteral("47L127000"));
        EXPECT_TEXT(c, opossum.model, QStringLiteral("47L127"));
        EXPECT_BOOL(c, opossum.output, true);

        // 未登记的 47L1 前缀：不支持且不可连接
        auto unregistered = CoyoteBleController::identify(QStringLiteral("47L199000"));
        EXPECT_TEXT(c, unregistered.model, QStringLiteral("47L199"));
        EXPECT_BOOL(c, unregistered.supported, false);
        EXPECT_INT(c, static_cast<int>(unregistered.state), static_cast<int>(SupportState::Unsupported));
        EXPECT_TEXT(c, unregistered.hint, QStringLiteral("未登记的 DG-LAB 型号"));

        // 非 47L1 前缀（手机、耳机、手环等）：不是 DG-LAB 设备，不支持且不可连接
        auto other_device = CoyoteBleController::identify(QStringLiteral("Bluetooth d3:3b:5a:fa:43:0c"));
        EXPECT_TEXT(c, other_device.model, QString());
        EXPECT_TEXT(c, other_device.type, QStringLiteral("unknown"));
        EXPECT_BOOL(c, other_device.supported, false);
        EXPECT_INT(c, static_cast<int>(other_device.state), static_cast<int>(SupportState::Unsupported));
        EXPECT_TEXT(c, other_device.hint, QStringLiteral("非 DG-LAB 设备"));
    }

    // 四类口径：已支持 / 实验性 / 需升级固件 / 不支持（未登记 47L1、非 DG-LAB）
    void test_support_categories() {
        const char* c = "support_categories";
        // 已支持：唯一实机验证过的郊狼 V3，可连接
        auto coyote = CoyoteBleController::identify(QStringLiteral("47L121000"));
        EXPECT_INT(c, static_cast<int>(coyote.state), static_cast<int>(SupportState::Supported));
        EXPECT_BOOL(c, coyote.supported, true);

        // 实验性：驱动已实现但未实机验证，可连接
        for (const char* name : {"47L120300", "47L124000", "47L127000"}) {
            auto experimental = CoyoteBleController::identify(QString::fromLatin1(name));
            EXPECT_INT(c, static_cast<int>(experimental.state),
                static_cast<int>(SupportState::Experimental));
            EXPECT_BOOL(c, experimental.supported, true);
        }

        // 需升级固件：型号已识别但本版不可连接
        auto paw_v10 = CoyoteBleController::identify(QStringLiteral("47L120100"));
        EXPECT_INT(c, static_cast<int>(paw_v10.state), static_cast<int>(SupportState::Unsupported));
        EXPECT_BOOL(c, paw_v10.supported, false);
        EXPECT_BOOL(c, paw_v10.hint.contains(QStringLiteral("固件")), true);

        // 不支持：未登记的 47L1 与非 47L1 前缀都给不出型号，均不可连接
        auto unregistered = CoyoteBleController::identify(QStringLiteral("47L199000"));
        auto other_device = CoyoteBleController::identify(QStringLiteral("AIRPODS-PRO"));
        EXPECT_INT(c, static_cast<int>(unregistered.state), static_cast<int>(SupportState::Unsupported));
        EXPECT_INT(c, static_cast<int>(other_device.state), static_cast<int>(SupportState::Unsupported));
        EXPECT_BOOL(c, unregistered.supported || other_device.supported, false);
    }

    // 记忆命中优先于名称：占位名与相互矛盾的名称都按记忆处理
    void test_identify_by_memory() {
        const char* c = "identify_by_memory";
        const QString placeholder = QStringLiteral("Bluetooth d3:3b:5a:fa:43:0c");

        auto coyote = CoyoteBleController::identify(placeholder, QStringLiteral("coyote"));
        EXPECT_TEXT(c, coyote.model, QStringLiteral("47L121"));
        EXPECT_TEXT(c, coyote.label, QStringLiteral("郊狼 V3"));
        EXPECT_TEXT(c, coyote.type, QStringLiteral("coyote"));
        EXPECT_BOOL(c, coyote.supported, true);
        EXPECT_BOOL(c, coyote.output, true);
        EXPECT_INT(c, static_cast<int>(coyote.state), static_cast<int>(SupportState::Supported));
        EXPECT_TEXT(c, coyote.hint, QString());

        auto paw = CoyoteBleController::identify(placeholder, QStringLiteral("pawprint"));
        EXPECT_TEXT(c, paw.model, QStringLiteral("47L120"));
        EXPECT_BOOL(c, paw.output, false);
        EXPECT_INT(c, static_cast<int>(paw.state), static_cast<int>(SupportState::Experimental));

        auto civet = CoyoteBleController::identify(placeholder, QStringLiteral("civet"));
        EXPECT_TEXT(c, civet.model, QStringLiteral("47L124"));

        auto opossum = CoyoteBleController::identify(placeholder, QStringLiteral("opossum"));
        EXPECT_TEXT(c, opossum.model, QStringLiteral("47L127"));

        // 记忆命中优先于名称：名称是郊狼但记忆为灵猫时按灵猫（实验性）处理
        auto by_memory = CoyoteBleController::identify(QStringLiteral("47L121000"), QStringLiteral("civet"));
        EXPECT_TEXT(c, by_memory.model, QStringLiteral("47L124"));
        EXPECT_TEXT(c, by_memory.type, QStringLiteral("civet"));
        EXPECT_INT(c, static_cast<int>(by_memory.state), static_cast<int>(SupportState::Experimental));

        // 记忆命中且型号为郊狼时按已支持处理（命名是别的登记型号也一样）
        auto coyote_by_memory = CoyoteBleController::identify(QStringLiteral("47L124000"),
            QStringLiteral("coyote"));
        EXPECT_TEXT(c, coyote_by_memory.model, QStringLiteral("47L121"));
        EXPECT_TEXT(c, coyote_by_memory.label, QStringLiteral("郊狼 V3"));
        EXPECT_BOOL(c, coyote_by_memory.supported, true);
        EXPECT_INT(c, static_cast<int>(coyote_by_memory.state),
            static_cast<int>(SupportState::Supported));

        // 记忆命中优先于未登记名称：未登记的 47L1 前缀 + 记忆为郊狼 → 已支持
        auto remembered_wins = CoyoteBleController::identify(QStringLiteral("47L199000"),
            QStringLiteral("coyote"));
        EXPECT_TEXT(c, remembered_wins.model, QStringLiteral("47L121"));
        EXPECT_INT(c, static_cast<int>(remembered_wins.state),
            static_cast<int>(SupportState::Supported));

        // 记忆值不合法时回到「非 DG-LAB 设备」（不支持）
        auto bogus = CoyoteBleController::identify(placeholder, QStringLiteral("bogus"));
        EXPECT_TEXT(c, bogus.model, QString());
        EXPECT_BOOL(c, bogus.supported, false);
        EXPECT_INT(c, static_cast<int>(bogus.state), static_cast<int>(SupportState::Unsupported));

        // 空名字（首帧反推路径）：记忆命中即给出型号，无记忆则无型号
        auto inferred = CoyoteBleController::identify(QString(), QStringLiteral("coyote"));
        EXPECT_TEXT(c, inferred.model, QStringLiteral("47L121"));
        auto no_memory = CoyoteBleController::identify(QString(), QString());
        EXPECT_TEXT(c, no_memory.model, QString());
    }

    // 首帧反推：可判定的首字节给出型号，0xD0 与无映射首字节返回空
    void test_model_from_frame_head() {
        const char* c = "model_from_frame_head";
        EXPECT_TEXT(c, CoyoteBleController::model_from_frame_head(0xB1), QStringLiteral("coyote"));
        EXPECT_TEXT(c, CoyoteBleController::model_from_frame_head(0xB3), QStringLiteral("opossum"));
        EXPECT_TEXT(c, CoyoteBleController::model_from_frame_head(0x51), QStringLiteral("pawprint"));
        EXPECT_TEXT(c, CoyoteBleController::model_from_frame_head(0x5A), QStringLiteral("pawprint"));
        EXPECT_TEXT(c, CoyoteBleController::model_from_frame_head(0x5B), QStringLiteral("pawprint"));
        EXPECT_TEXT(c, CoyoteBleController::model_from_frame_head(0x5C), QStringLiteral("pawprint"));
        // 0xD0 在爪印与灵猫之间歧义：不猜
        EXPECT_TEXT(c, CoyoteBleController::model_from_frame_head(0xD0), QString());
        EXPECT_TEXT(c, CoyoteBleController::model_from_frame_head(0x53), QString());
        EXPECT_TEXT(c, CoyoteBleController::model_from_frame_head(0x00), QString());
    }

    // 汇总执行
    int run_all() {
        test_identify_by_name();
        test_support_categories();
        test_identify_by_memory();
        test_model_from_frame_head();
        return failures;
    }

} // namespace

int main(int argc, char** argv) {
    Q_UNUSED(argc);
    Q_UNUSED(argv);
    const int failed = run_all();
    if (failed == 0) {
        std::printf("model_recognition_test: all cases passed\n");
        return EXIT_SUCCESS;
    }
    std::fprintf(stderr, "model_recognition_test: %d assertion(s) failed\n", failed);
    return EXIT_FAILURE;
}
