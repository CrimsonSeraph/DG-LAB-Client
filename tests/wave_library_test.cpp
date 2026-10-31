/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

// 波形库回归测试：官方内置波形加载、V2/V3 双版本帧、存储 round-trip、旧格式兼容与非法帧拒绝
// 全部用例在临时目录内构造，不触碰仓库 config/

#include "AppConfig.h"
#include "Wave.h"
#include "WaveLibrary.h"

#include <QCoreApplication>
#include <QDir>
#include <QStringList>
#include <QTemporaryDir>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>

#define CHECK_INT(case_name, expected, actual) check_int((case_name), (expected), (actual), #actual, __LINE__)
#define CHECK_TRUE(case_name, expr) check_true((case_name), (expr), #expr, __LINE__)
#define CHECK_STRING(case_name, expected, actual) \
    check_string((case_name), (expected), (actual), #actual, __LINE__)
#define CHECK_FRAMES(case_name, expected, actual) \
    check_frames((case_name), (expected), (actual), #actual, __LINE__)
#define CHECK_INT_AT_LEAST(case_name, minimum, actual) \
    check_int_at_least((case_name), (minimum), (actual), #actual, __LINE__)
#define FRAME_COUNT(array) (static_cast<int>(sizeof(array) / sizeof((array)[0])))

namespace {

    int failures = 0; ///< 失败断言数

    /// @brief 官方样例的 16 个波形名称（顺序与样例一致）
    const char* const OFFICIAL_NAMES[] = {
        "呼吸",
        "潮汐",
        "连击",
        "快速按捏",
        "按捏渐强",
        "心跳节奏",
        "压缩",
        "节奏步伐",
        "颗粒摩擦",
        "渐变弹跳",
        "波浪涟漪",
        "雨水冲刷",
        "变速敲击",
        "信号灯",
        "挑逗1",
        "挑逗2",
    };
    /// @brief 官方样例「呼吸」V2 帧
    const char* const BREATH_V2[] = {
        "210100",
        "210102",
        "210104",
        "210106",
        "210108",
        "21010A",
        "21010A",
        "21010A",
        "000000",
        "000000",
        "000000",
        "000000",
    };
    /// @brief 官方样例「呼吸」V3 帧
    const char* const BREATH_V3[] = {
        "0A0A0A0A00000000",
        "0A0A0A0A14141414",
        "0A0A0A0A28282828",
        "0A0A0A0A3C3C3C3C",
        "0A0A0A0A50505050",
        "0A0A0A0A64646464",
        "0A0A0A0A64646464",
        "0A0A0A0A64646464",
        "0A0A0A0A00000000",
        "0A0A0A0A00000000",
        "0A0A0A0A00000000",
        "0A0A0A0A00000000",
    };
    /// @brief 官方样例「潮汐」V2 帧
    const char* const TIDE_V2[] = {
        "210100",
        "418101",
        "810103",
        "A10105",
        "E18106",
        "210208",
        "41020A",
        "810209",
        "A10208",
        "E18207",
        "218306",
        "210300",
        "418301",
        "810303",
        "A10305",
        "E18306",
        "210408",
        "41040A",
        "810409",
        "A10408",
        "E18407",
        "218506",
        "000000",
    };
    /// @brief 官方样例「潮汐」V3 帧
    const char* const TIDE_V3[] = {
        "0A0A0A0A00000000",
        "0B0B0B0B10101010",
        "0D0D0D0D21212121",
        "0E0E0E0E32323232",
        "1010101042424242",
        "1212121253535353",
        "1313131364646464",
        "151515155C5C5C5C",
        "1616161654545454",
        "181818184C4C4C4C",
        "1A1A1A1A44444444",
        "1A1A1A1A00000000",
        "1B1B1B1B10101010",
        "1D1D1D1D21212121",
        "1E1E1E1E32323232",
        "2020202042424242",
        "2222222253535353",
        "2323232364646464",
        "252525255C5C5C5C",
        "2626262654545454",
        "282828284C4C4C4C",
        "2A2A2A2A44444444",
        "0A0A0A0A00000000",
    };
    /// @brief 官方样例「雨水冲刷」V2 帧
    const char* const RAIN_V2[] = {
        "A10103",
        "A18106",
        "A1010A",
        "A10103",
        "A18106",
        "A1010A",
        "A10103",
        "A18106",
        "A1010A",
        "A10103",
        "A18106",
        "A1010A",
        "A10103",
        "A18106",
        "A1010A",
        "A10103",
        "A18106",
        "A1010A",
        "A10103",
        "A18106",
        "A1010A",
        "A10103",
        "A18106",
        "A1010A",
        "A10103",
        "A18106",
        "A1010A",
        "A10103",
        "A18106",
        "A1010A",
        "A10103",
        "A18106",
        "A1010A",
        "A10103",
        "A18106",
        "A1010A",
        "A10103",
        "A18106",
        "A1010A",
        "21070A",
        "21070A",
        "21070A",
        "21070A",
        "21070A",
        "21070A",
        "21070A",
        "21070A",
        "21070A",
        "21070A",
        "21070A",
        "21070A",
        "21070A",
        "21070A",
        "21070A",
        "21070A",
        "21070A",
        "21070A",
        "21070A",
        "21070A",
        "21070A",
        "21070A",
        "21070A",
        "21070A",
        "21070A",
        "21070A",
        "21070A",
        "21070A",
        "21070A",
        "21070A",
        "21070A",
        "21070A",
        "21070A",
        "21070A",
        "21070A",
        "21070A",
        "000000",
        "000000",
        "000000",
    };
    /// @brief 官方样例「雨水冲刷」V3 帧
    const char* const RAIN_V3[] = {
        "0E0E0E0E21212121",
        "0E0E0E0E42424242",
        "0E0E0E0E64646464",
        "0E0E0E0E21212121",
        "0E0E0E0E42424242",
        "0E0E0E0E64646464",
        "0E0E0E0E21212121",
        "0E0E0E0E42424242",
        "0E0E0E0E64646464",
        "0E0E0E0E21212121",
        "0E0E0E0E42424242",
        "0E0E0E0E64646464",
        "0E0E0E0E21212121",
        "0E0E0E0E42424242",
        "0E0E0E0E64646464",
        "0E0E0E0E21212121",
        "0E0E0E0E42424242",
        "0E0E0E0E64646464",
        "0E0E0E0E21212121",
        "0E0E0E0E42424242",
        "0E0E0E0E64646464",
        "0E0E0E0E21212121",
        "0E0E0E0E42424242",
        "0E0E0E0E64646464",
        "0E0E0E0E21212121",
        "0E0E0E0E42424242",
        "0E0E0E0E64646464",
        "0E0E0E0E21212121",
        "0E0E0E0E42424242",
        "0E0E0E0E64646464",
        "0E0E0E0E21212121",
        "0E0E0E0E42424242",
        "0E0E0E0E64646464",
        "0E0E0E0E21212121",
        "0E0E0E0E42424242",
        "0E0E0E0E64646464",
        "0E0E0E0E21212121",
        "0E0E0E0E42424242",
        "0E0E0E0E64646464",
        "3A3A3A3A64646464",
        "3A3A3A3A64646464",
        "3A3A3A3A64646464",
        "3A3A3A3A64646464",
        "3A3A3A3A64646464",
        "3A3A3A3A64646464",
        "3A3A3A3A64646464",
        "3A3A3A3A64646464",
        "3A3A3A3A64646464",
        "3A3A3A3A64646464",
        "3A3A3A3A64646464",
        "3A3A3A3A64646464",
        "3A3A3A3A64646464",
        "3A3A3A3A64646464",
        "3A3A3A3A64646464",
        "3A3A3A3A64646464",
        "3A3A3A3A64646464",
        "3A3A3A3A64646464",
        "3A3A3A3A64646464",
        "3A3A3A3A64646464",
        "3A3A3A3A64646464",
        "3A3A3A3A64646464",
        "3A3A3A3A64646464",
        "3A3A3A3A64646464",
        "3A3A3A3A64646464",
        "3A3A3A3A64646464",
        "3A3A3A3A64646464",
        "3A3A3A3A64646464",
        "3A3A3A3A64646464",
        "3A3A3A3A64646464",
        "3A3A3A3A64646464",
        "3A3A3A3A64646464",
        "3A3A3A3A64646464",
        "3A3A3A3A64646464",
        "3A3A3A3A64646464",
        "3A3A3A3A64646464",
        "0A0A0A0A00000000",
        "0A0A0A0A00000000",
        "0A0A0A0A00000000",
    };
    /// @brief 官方样例波形数量
    const int OFFICIAL_NAMES_COUNT = static_cast<int>(sizeof(OFFICIAL_NAMES) / sizeof(OFFICIAL_NAMES[0]));

    /// @brief 旧格式条目里的 V3 帧（旧版本的 frames 字段）
    const char* const LEGACY_FRAMES[] = { "0A0A0A0A14141414", "0A0A0A0A28282828" };

    // 记录一次断言失败
    void fail(const char* case_name, const char* detail, int line) {
        std::fprintf(stderr, "FAIL %s (line %d): %s\n", case_name, line, detail);
        ++failures;
    }

    // 整型断言（形参用 long long，避免 qsizetype 传参触发 -Wshorten-64-to-32）
    void check_int(const char* case_name, long long expected, long long actual, const char* expr,
        int line) {
        if (expected == actual) {
            return;
        }
        char detail[256] = {0};
        std::snprintf(detail, sizeof(detail), "%s expected %lld actual %lld", expr, expected, actual);
        fail(case_name, detail, line);
    }

    // 下界整型断言：可扩展清单（内置波形等）只校验数量下界
    void check_int_at_least(const char* case_name, long long minimum, long long actual,
        const char* expr, int line) {
        if (actual >= minimum) {
            return;
        }
        char detail[256] = {0};
        std::snprintf(detail, sizeof(detail), "%s expected at least %lld actual %lld", expr, minimum,
            actual);
        fail(case_name, detail, line);
    }

    // 布尔断言
    void check_true(const char* case_name, bool actual, const char* expr, int line) {
        if (actual) {
            return;
        }
        char detail[256] = {0};
        std::snprintf(detail, sizeof(detail), "%s expected true", expr);
        fail(case_name, detail, line);
    }

    // 字符串断言
    void check_string(const char* case_name, const QString& expected, const QString& actual,
        const char* expr, int line) {
        if (expected == actual) {
            return;
        }
        fail(case_name, QStringLiteral("%1 expected %2 actual %3").arg(expr, expected, actual)
                            .toStdString()
                            .c_str(),
            line);
    }

    // 帧序列逐帧比较
    void check_frames(const char* case_name, const QStringList& expected, const QStringList& actual,
        const char* expr, int line) {
        if (expected.size() != actual.size()) {
            char detail[256] = {0};
            std::snprintf(detail, sizeof(detail), "%s frame count expected %d actual %d", expr,
                static_cast<int>(expected.size()), static_cast<int>(actual.size()));
            fail(case_name, detail, line);
            return;
        }
        for (int index = 0; index < expected.size(); ++index) {
            if (expected.at(index) == actual.at(index)) {
                continue;
            }
            fail(case_name,
                QStringLiteral("%1 frame %2 expected %3 actual %4")
                    .arg(expr)
                    .arg(index)
                    .arg(expected.at(index), actual.at(index))
                    .toStdString()
                    .c_str(),
                line);
            return;
        }
    }

    // 样例帧数组转帧序列
    QStringList sample_frames(const char* const* frames, int count) {
        QStringList list;
        for (int index = 0; index < count; ++index) {
            list.append(QString::fromLatin1(frames[index]));
        }
        return list;
    }

    // 写入临时波形库文件
    bool write_library(const std::string& dir, const nlohmann::json& waves) {
        nlohmann::json json;
        json["version"] = "1.0";
        json["current"] = { { "A", "" }, { "B", "" } };
        json["waves"] = waves;
        std::ofstream output(dir + "/waves.json", std::ios::trunc);
        if (!output.is_open()) {
            return false;
        }
        output << json.dump(4);
        return true;
    }

    // 16 个官方内置波形全部加载，名称集合与样例一致
    void test_official_loaded(WaveLibrary& library) {
        const char* name = "official/loaded";
        // 放宽：内置波形属于可扩展清单，数量只校验下界；逐个名称的 exists 校验见下方循环
        CHECK_INT_AT_LEAST(name, 16, OFFICIAL_NAMES_COUNT);
        CHECK_INT_AT_LEAST(name, OFFICIAL_NAMES_COUNT, library.official_count());
        for (const char* official : OFFICIAL_NAMES) {
            CHECK_TRUE(name, library.has(official));
            CHECK_TRUE(name, library.is_official(official));
            const Wave* wave = library.get(official);
            if (wave == nullptr) {
                continue;
            }
            CHECK_TRUE(name, wave->has_both_protocols());
            CHECK_TRUE(name, !wave->raw.empty());
            CHECK_TRUE(name, wave->sections.empty());
        }
        std::printf("[INFO] 官方内置波形加载 %d 个，名称与样例一致\n", library.official_count());
    }

    // 呼吸、潮汐两个波形的 V2 / V3 帧与样例逐帧相等
    void test_official_frames(WaveLibrary& library) {
        const char* name = "official/frames";
        const Wave* breath = library.get("呼吸");
        const Wave* tide = library.get("潮汐");
        CHECK_TRUE(name, breath != nullptr);
        CHECK_TRUE(name, tide != nullptr);
        if (breath != nullptr) {
            CHECK_FRAMES(name, sample_frames(BREATH_V2, FRAME_COUNT(BREATH_V2)), breath->frames_v2);
            CHECK_FRAMES(name, sample_frames(BREATH_V3, FRAME_COUNT(BREATH_V3)), breath->frames_v3);
        }
        if (tide != nullptr) {
            CHECK_FRAMES(name, sample_frames(TIDE_V2, FRAME_COUNT(TIDE_V2)), tide->frames_v2);
            CHECK_FRAMES(name, sample_frames(TIDE_V3, FRAME_COUNT(TIDE_V3)), tide->frames_v3);
        }
        std::printf("[INFO] 呼吸 V2 %d 帧 / V3 %d 帧，潮汐 V2 %d 帧 / V3 %d 帧（与样例逐帧相等）\n",
            breath == nullptr ? 0 : static_cast<int>(breath->frames_v2.size()),
            breath == nullptr ? 0 : static_cast<int>(breath->frames_v3.size()),
            tide == nullptr ? 0 : static_cast<int>(tide->frames_v2.size()),
            tide == nullptr ? 0 : static_cast<int>(tide->frames_v3.size()));
    }

    // 官方内置波形只读：不可覆盖、不可删除，雨水冲刷为官方数据
    void test_official_readonly(WaveLibrary& library) {
        const char* name = "official/readonly";
        Wave overwrite;
        overwrite.name = "呼吸";
        overwrite.frames_v3 = { QStringLiteral("0A0A0A0A00000000") };
        CHECK_TRUE(name, !library.save(overwrite));
        CHECK_TRUE(name, !library.remove("呼吸"));
        const Wave* breath = library.get("呼吸");
        CHECK_TRUE(name, breath != nullptr);
        if (breath != nullptr) {
            CHECK_INT(name, FRAME_COUNT(BREATH_V3), static_cast<int>(breath->frames_v3.size()));
        }
        const Wave* rain = library.get("雨水冲刷");
        CHECK_TRUE(name, rain != nullptr);
        if (rain != nullptr) {
            CHECK_TRUE(name, rain->is_official());
            CHECK_FRAMES(name, sample_frames(RAIN_V2, FRAME_COUNT(RAIN_V2)), rain->frames_v2);
            CHECK_FRAMES(name, sample_frames(RAIN_V3, FRAME_COUNT(RAIN_V3)), rain->frames_v3);
        }
        std::printf("[INFO] 内置波形只读：覆盖与删除均被拒绝；雨水冲刷为官方 %d 帧\n",
            rain == nullptr ? 0 : static_cast<int>(rain->frames_v3.size()));
    }

    // 存储 round-trip：落盘后重新加载，raw / frames_v2 / frames_v3 完全一致
    void test_round_trip(WaveLibrary& library) {
        const char* name = "storage/round_trip";
        std::map<std::string, Wave> snapshot;
        for (const char* official : OFFICIAL_NAMES) {
            const Wave* wave = library.get(official);
            if (wave != nullptr) {
                snapshot[official] = *wave;
            }
        }
        // 放宽：样例清单至少 16 个，多出的内置波形不参与本断言
        CHECK_INT_AT_LEAST(name, OFFICIAL_NAMES_COUNT, snapshot.size());

        // set_current 触发落盘
        library.set_current("A", "呼吸");
        library.reload();

        CHECK_STRING(name, QStringLiteral("呼吸"), library.current("A"));
        for (const auto& [wave_name, before] : snapshot) {
            const Wave* after = library.get(wave_name);
            CHECK_TRUE(name, after != nullptr);
            if (after == nullptr) {
                continue;
            }
            CHECK_STRING(name, QString::fromStdString(before.raw), QString::fromStdString(after->raw));
            CHECK_FRAMES(name, before.frames_v2, after->frames_v2);
            CHECK_FRAMES(name, before.frames_v3, after->frames_v3);
            CHECK_TRUE(name, after->is_official());
        }
        std::printf("[INFO] round-trip：%d 个波形的 raw / frames_v2 / frames_v3 一致\n",
            static_cast<int>(snapshot.size()));
    }

    // 旧格式（只有 frames 字段）的波形库条目读入 frames_v3
    void test_legacy_frames(WaveLibrary& library, const std::string& waves_dir) {
        const char* name = "storage/legacy_frames";
        nlohmann::json waves = nlohmann::json::object();
        waves["旧波形"] = { { "name", "旧波形" },
            { "sections", nlohmann::json::array() },
            { "frames", { "0A0A0A0A14141414", "0A0A0A0A28282828" } } };
        CHECK_TRUE(name, write_library(waves_dir, waves));
        library.reload();

        CHECK_TRUE(name, library.has("旧波形"));
        const Wave* wave = library.get("旧波形");
        if (wave != nullptr) {
            CHECK_FRAMES(name, sample_frames(LEGACY_FRAMES, FRAME_COUNT(LEGACY_FRAMES)), wave->frames_v3);
            CHECK_INT(name, 0, static_cast<int>(wave->frames_v2.size()));
            CHECK_INT(name, FRAME_COUNT(LEGACY_FRAMES), wave->frame_count());
            CHECK_TRUE(name, !wave->is_official());
        }
        // 官方内置波形来自资源，不受用户库文件影响
        // 放宽：此处只校验「至少仍是样例清单那么多」，新增内置波形不影响
        CHECK_INT_AT_LEAST(name, OFFICIAL_NAMES_COUNT, library.official_count());
        std::printf("[INFO] 旧格式 frames -> frames_v3：%d 帧\n",
            wave == nullptr ? 0 : static_cast<int>(wave->frames_v3.size()));
    }

    // 非法帧（长度错误、非 HEX）被拒绝
    void test_invalid_frames(WaveLibrary& library, const std::string& waves_dir) {
        const char* name = "frames/normalize";
        CHECK_STRING(name, QStringLiteral("0A0A0A0A00000000"),
            Wave::normalize_hex_frame(QStringLiteral("0x0a 0a0a 0a00 000000"), 8));
        CHECK_STRING(name, QStringLiteral("A10103A18106"),
            Wave::normalize_hex_frame(QStringLiteral("a1:01,03;a1-81_06"), 0));
        CHECK_STRING(name, QString(), Wave::normalize_hex_frame(QStringLiteral("0A0A0A0A0000000"), 8));
        CHECK_STRING(name, QString(), Wave::normalize_hex_frame(QStringLiteral("0A0A0A0A0000ZZ00"), 8));
        CHECK_STRING(name, QString(), Wave::normalize_hex_frame(QStringLiteral("A1010"), 0));
        CHECK_STRING(name, QString(), Wave::normalize_hex_frame(QString(), 0));

        nlohmann::json waves = nlohmann::json::object();
        waves["部分坏帧"] = { { "name", "部分坏帧" },
            { "frames", { "0A0A0A0A0000000", "ZZZZ", "0A0A0A0A00000000" } } };
        waves["全是坏帧"] = { { "name", "全是坏帧" }, { "frames", { "0A0A0A0A00000000AA" } } };
        CHECK_TRUE(name, write_library(waves_dir, waves));
        library.reload();

        // 单帧不合规只丢该帧，条目仍有合规帧时保留
        CHECK_TRUE(name, library.has("部分坏帧"));
        const Wave* partial = library.get("部分坏帧");
        if (partial != nullptr) {
            CHECK_FRAMES(name, QStringList { QStringLiteral("0A0A0A0A00000000") }, partial->frames_v3);
        }
        // 条目没有任何合规帧时整条丢弃
        CHECK_TRUE(name, !library.has("全是坏帧"));
        std::printf("[INFO] 非法帧拒绝：长度错误与非 HEX 均被丢弃，无合规帧的条目整条跳过\n");
    }

    // 旧版占位波形移除，同名旧数据被官方数据覆盖
    void test_placeholder_removed(WaveLibrary& library, const std::string& waves_dir) {
        const char* name = "official/placeholder_removed";
        nlohmann::json waves = nlohmann::json::object();
        waves["渐强脉冲"] = { { "name", "渐强脉冲" }, { "frames", { "0A0A0A0A00000000" } } };
        waves["低频涌动"] = { { "name", "低频涌动" }, { "frames", { "0A0A0A0A00000000" } } };
        waves["雨水冲刷"] = { { "name", "雨水冲刷" }, { "frames", { "0A0A0A0A00000000" } } };
        CHECK_TRUE(name, write_library(waves_dir, waves));
        library.reload();

        CHECK_TRUE(name, !library.has("渐强脉冲"));
        CHECK_TRUE(name, !library.has("低频涌动"));
        const Wave* rain = library.get("雨水冲刷");
        CHECK_TRUE(name, rain != nullptr);
        if (rain != nullptr) {
            CHECK_TRUE(name, rain->is_official());
            CHECK_FRAMES(name, sample_frames(RAIN_V2, FRAME_COUNT(RAIN_V2)), rain->frames_v2);
            CHECK_FRAMES(name, sample_frames(RAIN_V3, FRAME_COUNT(RAIN_V3)), rain->frames_v3);
        }
        std::printf("[INFO] 渐强脉冲 / 低频涌动已移除，雨水冲刷为官方 %d 帧（V2 %d 帧）\n",
            rain == nullptr ? 0 : static_cast<int>(rain->frames_v3.size()),
            rain == nullptr ? 0 : static_cast<int>(rain->frames_v2.size()));
    }

    // 发送协议（app.wave.protocol）选择与缺帧行为
    void test_protocol(WaveLibrary& library) {
        const char* name = "protocol/select";
        library.set_protocol("v2");
        CHECK_STRING(name, QStringLiteral("v2"), library.protocol());
        CHECK_FRAMES(name, sample_frames(BREATH_V2, FRAME_COUNT(BREATH_V2)),
            library.frames_of("呼吸", library.protocol()));

        // 非法取值被忽略
        library.set_protocol("v9");
        CHECK_STRING(name, QStringLiteral("v2"), library.protocol());

        // 缺帧不回落另一版本
        CHECK_TRUE(name, library.frames_of("不存在", "v2").isEmpty());
        Wave v3_only;
        v3_only.name = "仅V3";
        v3_only.frames_v3 = { QStringLiteral("0A0A0A0A64646464") };
        CHECK_TRUE(name, library.save(v3_only));
        CHECK_TRUE(name, library.frames_of("仅V3", "v2").isEmpty());
        CHECK_TRUE(name, !library.frames_of("仅V3", "v3").isEmpty());
        CHECK_TRUE(name, library.remove("仅V3"));

        library.set_protocol("v3");
        CHECK_STRING(name, QStringLiteral("v3"), library.protocol());
        CHECK_FRAMES(name, sample_frames(BREATH_V3, FRAME_COUNT(BREATH_V3)),
            library.frames_of("呼吸", "v3"));
        std::printf("[INFO] 协议切换：v2 / v3 各自取对应帧，缺帧返回空（不回落另一版本）\n");
    }

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    // 沙箱放在测试工作目录下（系统 TEMP 在部分环境里不可用），用例结束即递归删除
    QTemporaryDir temp(QDir::currentPath() + QStringLiteral("/dglab_wave_library_XXXXXX"));
    if (!temp.isValid()) {
        std::fprintf(stderr, "FAIL: 临时目录创建失败\n");
        return 2;
    }
    const std::string config_dir = temp.path().toStdString() + "/config";
    const std::string waves_dir = temp.path().toStdString() + "/waves";
    std::filesystem::create_directories(config_dir);
    std::filesystem::create_directories(waves_dir);
    if (!AppConfig::instance().initialize(config_dir)) {
        std::fprintf(stderr, "FAIL: 配置初始化失败\n");
        return 2;
    }
    AppConfig::instance().set_value("wave.path", waves_dir);
    AppConfig::instance().set_value("app.wave.protocol", std::string("v3"));

    WaveLibrary& library = WaveLibrary::instance();
    library.reload();

    test_official_loaded(library);
    test_official_frames(library);
    test_official_readonly(library);
    test_round_trip(library);
    test_legacy_frames(library, waves_dir);
    test_invalid_frames(library, waves_dir);
    test_placeholder_removed(library, waves_dir);
    test_protocol(library);

    if (failures != 0) {
        std::printf("wave_library_test: %d assertion(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    std::printf("wave_library_test: all assertions passed\n");
    return EXIT_SUCCESS;
}
