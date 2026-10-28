/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

// 波形导入导出回归测试：JSON 单对象 / 数组、.pulse 单文件、名称冲突三选、
// 逐条容错（只有 raw / 只有 V3 / 非法帧 / 别名键）与缺帧标记。
// 全部用例在临时目录内构造，不触碰仓库 config/

#include "AppConfig.h"
#include "Wave.h"
#include "WaveImportExport.h"
#include "WaveLibrary.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStringList>
#include <QTemporaryDir>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#define CHECK_INT(case_name, expected, actual) check_int((case_name), (expected), (actual), #actual, __LINE__)
#define CHECK_TRUE(case_name, expr) check_true((case_name), (expr), #expr, __LINE__)
#define CHECK_STRING(case_name, expected, actual) \
    check_string((case_name), (expected), (actual), #actual, __LINE__)
#define CHECK_FRAMES(case_name, expected, actual) \
    check_frames((case_name), (expected), (actual), #actual, __LINE__)
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
    /// @brief 官方样例波形数量
    const int OFFICIAL_NAMES_COUNT = static_cast<int>(sizeof(OFFICIAL_NAMES) / sizeof(OFFICIAL_NAMES[0]));

    // 记录一次断言失败
    void fail(const char* case_name, const char* detail, int line) {
        std::fprintf(stderr, "FAIL %s (line %d): %s\n", case_name, line, detail);
        ++failures;
    }

    // 整型断言
    void check_int(const char* case_name, int expected, int actual, const char* expr, int line) {
        if (expected == actual) {
            return;
        }
        char detail[256] = {0};
        std::snprintf(detail, sizeof(detail), "%s expected %d actual %d", expr, expected, actual);
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

    // 构造测试波形
    Wave make_wave(const char* name, const char* raw, const QStringList& v2, const QStringList& v3) {
        Wave wave;
        wave.name = name;
        wave.raw = raw;
        wave.frames_v2 = v2;
        wave.frames_v3 = v3;
        return wave;
    }

    // 以 UTF-8 写入文本文件
    bool write_file_text(const QString& path, const QString& text) {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return false;
        }
        const QByteArray data = text.toUtf8();
        const bool ok = file.write(data) == data.size();
        file.close();
        return ok;
    }

    // 读取文件全部字节
    QByteArray read_file_bytes(const QString& path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return QByteArray();
        }
        const QByteArray data = file.readAll();
        file.close();
        return data;
    }

    // 清空用户波形库（只留官方内置波形）并重新加载
    bool reset_library(WaveLibrary& library, const std::string& waves_dir) {
        nlohmann::json json;
        json["version"] = "1.1";
        json["current"] = { { "A", "" }, { "B", "" } };
        json["waves"] = nlohmann::json::object();
        std::ofstream output(waves_dir + "/waves.json", std::ios::trunc);
        if (!output.is_open()) {
            return false;
        }
        output << json.dump(4);
        output.close();
        library.reload();
        return true;
    }

    // 导出全部官方内置波形为 JSON
    bool export_official_json(WaveLibrary& library, const QString& path, QString& error,
        QStringList& notes) {
        std::vector<std::string> names;
        for (const char* official : OFFICIAL_NAMES) {
            names.push_back(official);
        }
        return library.export_json(path, names, error, notes);
    }

    // JSON 导出结构：数组形式、四个键齐全、缺帧写空数组并记说明
    void test_json_export_shape(WaveLibrary& library, const std::string& waves_dir,
        const QString& temp_dir) {
        const char* name = "json/export_shape";
        CHECK_TRUE(name, reset_library(library, waves_dir));

        Wave both = make_wave("导出甲", "Dungeonlab+pulse:AAAA", { QStringLiteral("210100") },
            { QStringLiteral("0A0A0A0A14141414") });
        Wave v3_only = make_wave("导出乙", "Dungeonlab+pulse:BBBB", {},
            { QStringLiteral("0A0A0A0A28282828") });
        CHECK_TRUE(name, library.save(both));
        CHECK_TRUE(name, library.save(v3_only));

        const QString path = temp_dir + QStringLiteral("/export_shape.json");
        QString error;
        QStringList notes;
        const bool exported
            = library.export_json(path, { std::string("导出甲"), std::string("导出乙") }, error, notes);
        CHECK_TRUE(name, exported);
        CHECK_TRUE(name, error.isEmpty());

        const QByteArray bytes = read_file_bytes(path);
        CHECK_TRUE(name, !bytes.isEmpty());
        nlohmann::json parsed;
        try {
            parsed = nlohmann::json::parse(bytes.constData(), bytes.constData() + bytes.size());
        }
        catch (const std::exception&) {
            CHECK_TRUE(name, false);
            return;
        }
        // 始终是数组形式，即使只有一条
        CHECK_TRUE(name, parsed.is_array());
        CHECK_INT(name, 2, static_cast<int>(parsed.size()));
        for (const auto& item : parsed) {
            CHECK_TRUE(name, item.contains("name"));
            CHECK_TRUE(name, item.contains("raw"));
            CHECK_TRUE(name, item.contains("expectedV2"));
            CHECK_TRUE(name, item.contains("expectedV3"));
        }
        // 缺 V2 的波形写空数组，并在 notes 里标注
        CHECK_INT(name, 0, static_cast<int>(parsed[1]["expectedV2"].size()));
        CHECK_INT(name, 1, static_cast<int>(parsed[1]["expectedV3"].size()));
        CHECK_TRUE(name, notes.join(QStringLiteral("；")).contains(QStringLiteral("缺少 V2 帧")));
        std::printf("[INFO] JSON 导出：数组 %d 条，缺帧写空数组并记说明（%s）\n",
            static_cast<int>(parsed.size()), notes.join(QStringLiteral("；")).toStdString().c_str());
    }

    // 官方 16 个波形导出后再解析：名称 / raw / framesV2 / framesV3 逐条一致
    void test_json_round_trip_official(WaveLibrary& library, const QString& temp_dir) {
        const char* name = "json/round_trip_official";
        CHECK_INT(name, static_cast<int>(OFFICIAL_NAMES_COUNT), library.official_count());

        const QString path = temp_dir + QStringLiteral("/official.json");
        QString error;
        QStringList notes;
        CHECK_TRUE(name, export_official_json(library, path, error, notes));
        CHECK_TRUE(name, error.isEmpty());

        std::vector<Wave> imported;
        QStringList skipped;
        CHECK_TRUE(name,
            wave_io::parse_json(QString::fromUtf8(read_file_bytes(path)), imported, skipped, error));
        CHECK_TRUE(name, error.isEmpty());
        CHECK_INT(name, 0, static_cast<int>(skipped.size()));
        CHECK_INT(name, static_cast<int>(OFFICIAL_NAMES_COUNT), static_cast<int>(imported.size()));

        std::map<std::string, const Wave*> parsed_by_name;
        for (const auto& wave : imported) {
            parsed_by_name[wave.name] = &wave;
        }
        for (const char* official : OFFICIAL_NAMES) {
            const Wave* original = library.get(official);
            CHECK_TRUE(name, original != nullptr);
            const auto it = parsed_by_name.find(official);
            CHECK_TRUE(name, it != parsed_by_name.end());
            if (original == nullptr || it == parsed_by_name.end()) {
                continue;
            }
            const Wave& parsed = *it->second;
            CHECK_STRING(name, QString::fromStdString(original->raw), QString::fromStdString(parsed.raw));
            CHECK_FRAMES(name, original->frames_v2, parsed.frames_v2);
            CHECK_FRAMES(name, original->frames_v3, parsed.frames_v3);
        }

        // 「呼吸」「潮汐」逐帧断言
        const auto breath = parsed_by_name.find("呼吸");
        const auto tide = parsed_by_name.find("潮汐");
        CHECK_TRUE(name, breath != parsed_by_name.end());
        CHECK_TRUE(name, tide != parsed_by_name.end());
        if (breath != parsed_by_name.end()) {
            CHECK_FRAMES(name, sample_frames(BREATH_V2, FRAME_COUNT(BREATH_V2)), breath->second->frames_v2);
            CHECK_FRAMES(name, sample_frames(BREATH_V3, FRAME_COUNT(BREATH_V3)), breath->second->frames_v3);
        }
        if (tide != parsed_by_name.end()) {
            CHECK_FRAMES(name, sample_frames(TIDE_V2, FRAME_COUNT(TIDE_V2)), tide->second->frames_v2);
            CHECK_FRAMES(name, sample_frames(TIDE_V3, FRAME_COUNT(TIDE_V3)), tide->second->frames_v3);
        }
        // 写入路径：官方数据以「名称 (2)」重命名入库后，raw 与两套帧仍与原件一致
        const WaveImportPlan rename_plan = library.plan_import(path);
        CHECK_INT(name, static_cast<int>(OFFICIAL_NAMES_COUNT),
            static_cast<int>(rename_plan.conflicts.size()));
        const WaveImportResult rename_result
            = library.apply_import(rename_plan, WaveConflictPolicy::Rename);
        CHECK_INT(name, static_cast<int>(OFFICIAL_NAMES_COUNT), rename_result.renamed);
        const Wave* renamed_breath = library.get("呼吸 (2)");
        CHECK_TRUE(name, renamed_breath != nullptr);
        if (breath != parsed_by_name.end() && renamed_breath != nullptr) {
            CHECK_STRING(name, QString::fromStdString(breath->second->raw),
                QString::fromStdString(renamed_breath->raw));
            CHECK_FRAMES(name, sample_frames(BREATH_V2, FRAME_COUNT(BREATH_V2)), renamed_breath->frames_v2);
            CHECK_FRAMES(name, sample_frames(BREATH_V3, FRAME_COUNT(BREATH_V3)), renamed_breath->frames_v3);
        }
        std::printf("[INFO] JSON round-trip：%d 个官方波形逐条一致，呼吸 / 潮汐逐帧一致；"
                    "以「名称 (2)」重命名写入后仍与原件一致\n",
            static_cast<int>(imported.size()));
    }

    // 用户库清空后重新导入：名称 / raw / 两套帧逐条一致，且帧齐全可发送
    void test_json_import_into_empty_library(WaveLibrary& library, const std::string& waves_dir,
        const QString& temp_dir) {
        const char* name = "json/import_empty";
        CHECK_TRUE(name, reset_library(library, waves_dir));
        CHECK_TRUE(name, library.save(make_wave("导入甲", "Dungeonlab+pulse:AAA",
                           { QStringLiteral("210100") },
                           { QStringLiteral("0A0A0A0A14141414"), QStringLiteral("0A0A0A0A28282828") })));
        CHECK_TRUE(name, library.save(make_wave("导入乙", "Dungeonlab+pulse:BBB",
                           { QStringLiteral("210100"), QStringLiteral("210102") },
                           { QStringLiteral("0A0A0A0A3C3C3C3C") })));

        const QString path = temp_dir + QStringLiteral("/import_source.json");
        QString error;
        QStringList notes;
        const bool exported
            = library.export_json(path, { std::string("导入甲"), std::string("导入乙") }, error, notes);
        CHECK_TRUE(name, exported);

        // 清空用户库后重新导入
        CHECK_TRUE(name, reset_library(library, waves_dir));
        CHECK_TRUE(name, !library.has("导入甲"));

        const WaveImportPlan plan = library.plan_import(path);
        CHECK_TRUE(name, plan.error.isEmpty());
        CHECK_INT(name, 2, static_cast<int>(plan.waves.size()));
        CHECK_INT(name, 0, static_cast<int>(plan.conflicts.size()));

        const WaveImportResult result = library.apply_import(plan, WaveConflictPolicy::Skip);
        CHECK_INT(name, 2, result.added);
        CHECK_INT(name, 0, result.skipped);

        const Wave* first = library.get("导入甲");
        const Wave* second = library.get("导入乙");
        CHECK_TRUE(name, first != nullptr);
        CHECK_TRUE(name, second != nullptr);
        if (first != nullptr) {
            CHECK_STRING(name, QStringLiteral("Dungeonlab+pulse:AAA"), QString::fromStdString(first->raw));
            CHECK_FRAMES(name, QStringList { QStringLiteral("210100") }, first->frames_v2);
            const QStringList expected_v3 { QStringLiteral("0A0A0A0A14141414"),
                QStringLiteral("0A0A0A0A28282828") };
            CHECK_FRAMES(name, expected_v3, first->frames_v3);
            // V3 帧齐全：当前协议（v3）下可发送
            CHECK_TRUE(name, !library.frames_of("导入甲", "v3").isEmpty());
            CHECK_TRUE(name, !library.frames_of("导入甲", "v2").isEmpty());
        }
        if (second != nullptr) {
            CHECK_STRING(name, QStringLiteral("Dungeonlab+pulse:BBB"), QString::fromStdString(second->raw));
            CHECK_FRAMES(name, QStringList { QStringLiteral("0A0A0A0A3C3C3C3C") }, second->frames_v3);
        }
        std::printf("[INFO] 导入空库：2 条全部写入，raw / framesV2 / framesV3 与原件一致\n");
    }

    // .pulse round-trip：导出内容与 raw 逐字节一致，读回后可写入且标记缺帧不可发送
    void test_pulse_round_trip(WaveLibrary& library, const std::string& waves_dir,
        const QString& temp_dir) {
        const char* name = "pulse/round_trip";
        CHECK_TRUE(name, reset_library(library, waves_dir));
        const Wave* breath = library.get("呼吸");
        CHECK_TRUE(name, breath != nullptr);
        if (breath == nullptr) {
            return;
        }

        const QString path = temp_dir + QStringLiteral("/呼吸.pulse");
        QString error;
        CHECK_TRUE(name, library.export_pulse(path, "呼吸", error));
        CHECK_TRUE(name, error.isEmpty());

        const QByteArray bytes = read_file_bytes(path);
        CHECK_TRUE(name, bytes == QByteArray::fromStdString(breath->raw));
        CHECK_INT(name, static_cast<int>(breath->raw.size()), static_cast<int>(bytes.size()));

        const WaveImportPlan plan = library.plan_import(path);
        CHECK_TRUE(name, plan.error.isEmpty());
        CHECK_INT(name, 1, static_cast<int>(plan.waves.size()));
        if (plan.waves.size() == 1) {
            CHECK_STRING(name, QStringLiteral("呼吸"), QString::fromStdString(plan.waves.front().name));
            CHECK_STRING(name, QString::fromStdString(breath->raw),
                QString::fromStdString(plan.waves.front().raw));
            CHECK_INT(name, 0, static_cast<int>(plan.waves.front().frames_v2.size()));
            CHECK_INT(name, 0, static_cast<int>(plan.waves.front().frames_v3.size()));
        }
        // 与官方同名：进入冲突流程
        CHECK_INT(name, 1, static_cast<int>(plan.conflicts.size()));

        const WaveImportResult result
            = library.apply_import(plan, WaveConflictPolicy::Rename, QStringLiteral("呼吸导入"));
        CHECK_INT(name, 1, result.renamed);
        const Wave* imported = library.get("呼吸导入");
        CHECK_TRUE(name, imported != nullptr);
        if (imported != nullptr) {
            CHECK_STRING(name, QString::fromStdString(breath->raw), QString::fromStdString(imported->raw));
            // 缺帧：明确标记不可发送，不静默失败
            CHECK_TRUE(name, library.frames_of("呼吸导入", "v3").isEmpty());
            CHECK_TRUE(name, result.notes.join(QStringLiteral("；")).contains(QStringLiteral("不可发送")));
            CHECK_TRUE(name,
                result.warnings.join(QStringLiteral("；")).contains(QStringLiteral("不可发送")));
            CHECK_TRUE(name, result.summary().contains(QStringLiteral("不可发送")));
        }
        std::printf("[INFO] .pulse round-trip：raw %d 字节逐字节一致，导入后标记缺帧不可发送\n",
            static_cast<int>(bytes.size()));
    }

    // 逐条容错：只有 raw / 只有 V3 / 非法帧 / 单对象 / 别名键
    void test_json_tolerance() {
        const char* name = "json/tolerance";
        std::vector<Wave> waves;
        QStringList skipped;
        QString error;

        // 只有 raw 的单对象：存 raw、帧为空
        CHECK_TRUE(name, wave_io::parse_json(
                             QString::fromUtf8(R"({"name":"只有raw","raw":"Dungeonlab+pulse:AAAA"})"),
                             waves, skipped, error));
        CHECK_INT(name, 1, static_cast<int>(waves.size()));
        CHECK_INT(name, 0, static_cast<int>(skipped.size()));
        if (waves.size() == 1) {
            CHECK_INT(name, 0, static_cast<int>(waves.front().frames_v3.size()));
            CHECK_STRING(name, QStringLiteral("Dungeonlab+pulse:AAAA"),
                QString::fromStdString(waves.front().raw));
        }

        // 只有 V3：存该套，V2 为空
        CHECK_TRUE(name, wave_io::parse_json(
                             QString::fromUtf8(R"({"name":"只有V3","expectedV3":["0A0A0A0A14141414"]})"),
                             waves, skipped, error));
        CHECK_INT(name, 1, static_cast<int>(waves.size()));
        if (waves.size() == 1) {
            CHECK_INT(name, 0, static_cast<int>(waves.front().frames_v2.size()));
            CHECK_FRAMES(name, QStringList { QStringLiteral("0A0A0A0A14141414") },
                waves.front().frames_v3);
        }

        // 只有 V2：存该套，V3 为空
        CHECK_TRUE(name, wave_io::parse_json(
                             QString::fromUtf8(R"({"name":"只有V2","expectedV2":["210100"]})"), waves,
                             skipped, error));
        CHECK_INT(name, 1, static_cast<int>(waves.size()));
        if (waves.size() == 1) {
            CHECK_INT(name, 0, static_cast<int>(waves.front().frames_v3.size()));
            CHECK_FRAMES(name, QStringList { QStringLiteral("210100") }, waves.front().frames_v2);
        }

        // 别名键 framesV2 / framesV3 等价于 expectedV2 / expectedV3
        CHECK_TRUE(name, wave_io::parse_json(
                             QString::fromUtf8(R"({"name":"别名","framesV2":["210100"],)"
                                               R"("framesV3":["0A0A0A0A64646464"]})"),
                             waves, skipped, error));
        CHECK_INT(name, 1, static_cast<int>(waves.size()));
        if (waves.size() == 1) {
            CHECK_FRAMES(name, QStringList { QStringLiteral("210100") }, waves.front().frames_v2);
            CHECK_FRAMES(name, QStringList { QStringLiteral("0A0A0A0A64646464") }, waves.front().frames_v3);
        }

        // 非法帧：该条报错跳过，其它条目照常解析
        CHECK_TRUE(name, wave_io::parse_json(
                             QString::fromUtf8(R"([{"name":"坏帧","expectedV2":["ZZZZ"]},)"
                                               R"({"name":"好帧","expectedV3":["0A0A0A0A14141414"]}])"),
                             waves, skipped, error));
        CHECK_TRUE(name, error.isEmpty());
        CHECK_INT(name, 1, static_cast<int>(waves.size()));
        CHECK_INT(name, 1, static_cast<int>(skipped.size()));
        CHECK_TRUE(name, skipped.join(QStringLiteral("；")).contains(QStringLiteral("坏帧")));

        // 既没有 raw 也没有帧：跳过
        CHECK_TRUE(name, wave_io::parse_json(QString::fromUtf8(R"({"name":"空条目"})"), waves, skipped,
                             error));
        CHECK_INT(name, 0, static_cast<int>(waves.size()));
        CHECK_INT(name, 1, static_cast<int>(skipped.size()));

        // 数组元素都不是对象：逐条跳过（不算致命错误）
        CHECK_TRUE(name, wave_io::parse_json(QStringLiteral("[1, 2, 3]"), waves, skipped, error));
        CHECK_INT(name, 0, static_cast<int>(waves.size()));
        CHECK_INT(name, 3, static_cast<int>(skipped.size()));

        // 不是合法 JSON：致命错误
        CHECK_TRUE(name, !wave_io::parse_json(QStringLiteral("{not json}"), waves, skipped, error));
        CHECK_TRUE(name, !error.isEmpty());
        CHECK_TRUE(name, !wave_io::parse_json(QStringLiteral("42"), waves, skipped, error));
        CHECK_TRUE(name, !error.isEmpty());

        // .pulse 前缀校验
        Wave pulse;
        CHECK_TRUE(name, !wave_io::parse_pulse(QStringLiteral("hello"), QStringLiteral("x"), pulse, error));
        CHECK_TRUE(name, error.contains(QStringLiteral("Dungeonlab+pulse:")));
        CHECK_TRUE(name, wave_io::parse_pulse(QStringLiteral("Dungeonlab+pulse:AA"), QStringLiteral("x"),
                             pulse, error));
        CHECK_STRING(name, QStringLiteral("Dungeonlab+pulse:AA"), QString::fromStdString(pulse.raw));
        std::printf("[INFO] 容错：只有 raw / 只有 V2 / 只有 V3 / 别名键 / 非法帧跳过 / 致命错误均符合预期\n");
    }

    // 冲突策略：覆盖
    void test_conflict_overwrite(WaveLibrary& library, const std::string& waves_dir,
        const QString& temp_dir) {
        const char* name = "conflict/overwrite";
        CHECK_TRUE(name, reset_library(library, waves_dir));
        CHECK_TRUE(name, library.save(make_wave("冲突基准", "Dungeonlab+pulse:OLD",
                           { QStringLiteral("210100") }, { QStringLiteral("0A0A0A0A00000000") })));

        const QString path = temp_dir + QStringLiteral("/conflict_overwrite.json");
        CHECK_TRUE(name, write_file_text(path,
                           QString::fromUtf8(R"([{"name":"冲突基准","raw":"Dungeonlab+pulse:NEW",)"
                                             R"("expectedV2":["210102"],)"
                                             R"("expectedV3":["0A0A0A0A64646464"]}])")));
        const WaveImportPlan plan = library.plan_import(path);
        CHECK_TRUE(name, plan.error.isEmpty());
        CHECK_INT(name, 1, static_cast<int>(plan.conflicts.size()));

        const WaveImportResult result = library.apply_import(plan, WaveConflictPolicy::Overwrite);
        CHECK_INT(name, 1, result.overwritten);
        CHECK_INT(name, 0, result.added);
        const Wave* after = library.get("冲突基准");
        CHECK_TRUE(name, after != nullptr);
        if (after != nullptr) {
            CHECK_STRING(name, QStringLiteral("Dungeonlab+pulse:NEW"), QString::fromStdString(after->raw));
            CHECK_FRAMES(name, QStringList { QStringLiteral("0A0A0A0A64646464") }, after->frames_v3);
            CHECK_FRAMES(name, QStringList { QStringLiteral("210102") }, after->frames_v2);
        }

        // 内置官方波形不可覆盖：同样选择覆盖时该条被跳过
        const QString official_path = temp_dir + QStringLiteral("/conflict_official.json");
        CHECK_TRUE(name, write_file_text(official_path,
                           QString::fromUtf8(R"({"name":"呼吸","expectedV3":["0A0A0A0A14141414"]})")));
        const WaveImportPlan official_plan = library.plan_import(official_path);
        const WaveImportResult official_result
            = library.apply_import(official_plan, WaveConflictPolicy::Overwrite);
        CHECK_INT(name, 0, official_result.overwritten);
        CHECK_INT(name, 1, official_result.skipped);
        const Wave* breath = library.get("呼吸");
        CHECK_TRUE(name, breath != nullptr);
        if (breath != nullptr) {
            CHECK_FRAMES(name, sample_frames(BREATH_V3, FRAME_COUNT(BREATH_V3)), breath->frames_v3);
        }
        std::printf("[INFO] 冲突覆盖：用户同名波形被替换，官方同名条目拒绝覆盖并跳过\n");
    }

    // 冲突策略：重命名（单条冲突使用弹窗里改写的名称）
    void test_conflict_rename(WaveLibrary& library, const std::string& waves_dir,
        const QString& temp_dir) {
        const char* name = "conflict/rename";
        CHECK_TRUE(name, reset_library(library, waves_dir));
        CHECK_TRUE(name, library.save(make_wave("冲突基准", "Dungeonlab+pulse:OLD",
                           { QStringLiteral("210100") }, { QStringLiteral("0A0A0A0A00000000") })));

        const QString path = temp_dir + QStringLiteral("/conflict_rename.json");
        CHECK_TRUE(name, write_file_text(path,
                           QString::fromUtf8(R"([{"name":"冲突基准","raw":"Dungeonlab+pulse:NEW",)"
                                             R"("expectedV3":["0A0A0A0A64646464"]}])")));
        const WaveImportPlan plan = library.plan_import(path);
        const WaveImportResult result
            = library.apply_import(plan, WaveConflictPolicy::Rename, QStringLiteral("冲突改名"));
        CHECK_INT(name, 1, result.renamed);
        CHECK_INT(name, 0, result.overwritten);

        const Wave* original = library.get("冲突基准");
        CHECK_TRUE(name, original != nullptr);
        if (original != nullptr) {
            CHECK_STRING(name, QStringLiteral("Dungeonlab+pulse:OLD"), QString::fromStdString(original->raw));
        }
        const Wave* renamed = library.get("冲突改名");
        CHECK_TRUE(name, renamed != nullptr);
        if (renamed != nullptr) {
            CHECK_STRING(name, QStringLiteral("Dungeonlab+pulse:NEW"), QString::fromStdString(renamed->raw));
        }

        // 未指定新名称时按序号自动生成
        const WaveImportPlan again = library.plan_import(path);
        const WaveImportResult auto_rename
            = library.apply_import(again, WaveConflictPolicy::Rename, QString());
        CHECK_INT(name, 1, auto_rename.renamed);
        CHECK_TRUE(name, library.has("冲突基准 (2)"));
        std::printf("[INFO] 冲突重命名：指定名称写入「冲突改名」，未指定时自动生成「冲突基准 (2)」\n");
    }

    // 冲突策略：跳过
    void test_conflict_skip(WaveLibrary& library, const std::string& waves_dir,
        const QString& temp_dir) {
        const char* name = "conflict/skip";
        CHECK_TRUE(name, reset_library(library, waves_dir));
        CHECK_TRUE(name, library.save(make_wave("冲突基准", "Dungeonlab+pulse:OLD",
                           { QStringLiteral("210100") }, { QStringLiteral("0A0A0A0A00000000") })));

        const QString path = temp_dir + QStringLiteral("/conflict_skip.json");
        CHECK_TRUE(name, write_file_text(path,
                           QString::fromUtf8(R"([{"name":"冲突基准","raw":"Dungeonlab+pulse:NEW",)"
                                             R"("expectedV3":["0A0A0A0A64646464"]},)"
                                             R"({"name":"冲突新条目","raw":"Dungeonlab+pulse:FRESH"}])")));

        const int before = static_cast<int>(library.names().size());
        const WaveImportPlan plan = library.plan_import(path);
        const WaveImportResult result = library.apply_import(plan, WaveConflictPolicy::Skip);
        CHECK_INT(name, 1, result.skipped);
        CHECK_INT(name, 1, result.added);
        // 同名的被跳过（内容不变），另一条照常写入
        CHECK_INT(name, before + 1, static_cast<int>(library.names().size()));
        const Wave* original = library.get("冲突基准");
        CHECK_TRUE(name, original != nullptr);
        if (original != nullptr) {
            CHECK_STRING(name, QStringLiteral("Dungeonlab+pulse:OLD"), QString::fromStdString(original->raw));
        }
        CHECK_TRUE(name, library.has("冲突新条目"));
        // 汇总文案带跳过条数与原因
        CHECK_TRUE(name, result.summary().contains(QStringLiteral("跳过 1")));
        CHECK_TRUE(name, result.summary().contains(QStringLiteral("冲突基准")));
        std::printf("[INFO] 冲突跳过：同名条目未写入，非同名的 %d 条新增照常写入；汇总=%s\n",
            result.added, result.summary().toStdString().c_str());
    }

    // .pulse 导出拒绝：raw 为空时不产生文件
    void test_pulse_export_refused(WaveLibrary& library, const std::string& waves_dir,
        const QString& temp_dir) {
        const char* name = "pulse/empty_raw";
        CHECK_TRUE(name, reset_library(library, waves_dir));
        CHECK_TRUE(name, library.save(make_wave("无原始串", "", {},
                           { QStringLiteral("0A0A0A0A14141414") })));

        const QString path = temp_dir + QStringLiteral("/无原始串.pulse");
        QString error;
        CHECK_TRUE(name, !library.export_pulse(path, "无原始串", error));
        CHECK_TRUE(name, error.contains(QStringLiteral("raw")));
        CHECK_TRUE(name, !QFile::exists(path));

        const Wave* wave = library.get("无原始串");
        QString text;
        CHECK_TRUE(name, wave != nullptr);
        if (wave != nullptr) {
            CHECK_TRUE(name, !wave_io::build_pulse(*wave, text, error));
            CHECK_TRUE(name, text.isEmpty());
        }
        std::printf("[INFO] .pulse 导出拒绝：raw 为空时返回原因且不写文件（%s）\n",
            error.toStdString().c_str());
    }

    // 只有 raw 的波形可落盘并在重载后保留（不可发送但可再导出）
    void test_raw_only_persisted(WaveLibrary& library, const std::string& waves_dir) {
        const char* name = "storage/raw_only";
        CHECK_TRUE(name, reset_library(library, waves_dir));
        CHECK_TRUE(name, library.save(make_wave("只有原始串", "Dungeonlab+pulse:KEEP", {}, {})));
        library.reload();

        const Wave* wave = library.get("只有原始串");
        CHECK_TRUE(name, wave != nullptr);
        if (wave != nullptr) {
            CHECK_STRING(name, QStringLiteral("Dungeonlab+pulse:KEEP"), QString::fromStdString(wave->raw));
            CHECK_INT(name, 0, static_cast<int>(wave->frames_v3.size()));
            CHECK_TRUE(name, library.frames_of("只有原始串", "v3").isEmpty());
        }
        std::printf("[INFO] 只有 raw 的波形重载后仍在库中（不可发送）\n");
    }

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    // 沙箱放在测试工作目录下（系统 TEMP 在部分环境里不可用），用例结束即递归删除
    QTemporaryDir temp(QDir::currentPath() + QStringLiteral("/dglab_wave_import_export_XXXXXX"));
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
    const QString temp_dir = temp.path();

    test_json_export_shape(library, waves_dir, temp_dir);
    test_json_round_trip_official(library, temp_dir);
    test_json_import_into_empty_library(library, waves_dir, temp_dir);
    test_pulse_round_trip(library, waves_dir, temp_dir);
    test_json_tolerance();
    test_conflict_overwrite(library, waves_dir, temp_dir);
    test_conflict_rename(library, waves_dir, temp_dir);
    test_conflict_skip(library, waves_dir, temp_dir);
    test_pulse_export_refused(library, waves_dir, temp_dir);
    test_raw_only_persisted(library, waves_dir);

    if (failures != 0) {
        std::printf("wave_import_export_test: %d assertion(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    std::printf("wave_import_export_test: all assertions passed\n");
    return EXIT_SUCCESS;
}
