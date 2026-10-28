/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

// 波形标签库回归测试：标签规范化、波形上的标签 round-trip、旧库无 tags 字段兼容、
// 导入自动登记、内置标签不可删、删除自定义标签时从使用中的波形上一并移除、
// JSON 导出含 tags 且可再导入
// 全部用例在临时目录内构造，不触碰仓库 config/

#include "AppConfig.h"
#include "Wave.h"
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
#include <string>
#include <vector>

#define CHECK_INT(case_name, expected, actual) check_int((case_name), (expected), (actual), #actual, __LINE__)
#define CHECK_TRUE(case_name, expr) check_true((case_name), (expr), #expr, __LINE__)
#define CHECK_STRING(case_name, expected, actual) \
    check_string((case_name), (expected), (actual), #actual, __LINE__)
#define CHECK_STRINGS(case_name, expected, actual) \
    check_strings((case_name), (expected), (actual), #actual, __LINE__)

namespace {

    int failures = 0; ///< 失败断言数

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
        fail(case_name, expr, line);
    }

    // 字符串断言
    void check_string(const char* case_name, const QString& expected, const QString& actual,
        const char* expr, int line) {
        if (expected == actual) {
            return;
        }
        fail(case_name,
            QStringLiteral("%1 expected %2 actual %3").arg(expr, expected, actual).toStdString().c_str(),
            line);
    }

    // 标签列表断言（顺序敏感）
    void check_strings(const char* case_name, const QStringList& expected, const QStringList& actual,
        const char* expr, int line) {
        if (expected == actual) {
            return;
        }
        fail(case_name,
            QStringLiteral("%1 expected [%2] actual [%3]")
                .arg(expr, expected.join(QStringLiteral(",")), actual.join(QStringLiteral(",")))
                .toStdString()
                .c_str(),
            line);
    }

    // 读取文本文件；失败返回空串
    QString read_text_file(const QString& path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return QString();
        }
        const QByteArray data = file.readAll();
        file.close();
        return QString::fromUtf8(data);
    }

    // 把 JSON 写入文件
    bool write_json_file(const QString& path, const nlohmann::json& json) {
        std::ofstream output(path.toStdString(), std::ios::trunc);
        if (!output.is_open()) {
            return false;
        }
        output << json.dump(4);
        return true;
    }

    // 解析文本为 JSON；失败返回 null
    nlohmann::json parse_text(const QString& text) {
        try {
            return nlohmann::json::parse(text.toStdString());
        }
        catch (const std::exception&) {
            return nlohmann::json();
        }
    }

    // 由 UTF-8 字面量构造标签列表（宏参数里有逗号，列表必须包在括号内）
    QStringList tag_list(std::initializer_list<const char*> items) {
        QStringList list;
        for (const char* item : items) {
            list.append(QString::fromUtf8(item));
        }
        return list;
    }

    // 自定义标签库文件的标签数组
    QStringList stored_tags(const std::string& dir) {
        QStringList tags;
        const nlohmann::json json = parse_text(read_text_file(
            QString::fromStdString(dir + "/tags.json")));
        if (!json.is_object() || !json.contains("tags") || !json["tags"].is_array()) {
            return tags;
        }
        for (const auto& item : json["tags"]) {
            if (item.is_string()) {
                tags.append(QString::fromStdString(item.get<std::string>()));
            }
        }
        return tags;
    }

    // 波形的标签（库中取）
    QStringList tags_of(WaveLibrary& library, const char* name) {
        const Wave* wave = library.get(name);
        return wave == nullptr ? QStringList() : wave->tags;
    }

    // 标签规范化：去空白、空串丢弃、大小写不敏感去重、超长拒绝
    void test_normalize() {
        const char* name = "tags/normalize";
        const QString too_long = QStringLiteral("A").repeated(Wave::TAG_MAX_LENGTH + 1);
        const QStringList input = { QStringLiteral("  节奏  "), QStringLiteral("节奏"),
            QStringLiteral("ELEC"), QStringLiteral("elec"), QString(), too_long };
        CHECK_STRINGS(name, tag_list({ "节奏", "ELEC" }),
            Wave::normalize_tags(input));
        CHECK_INT(name, 0, static_cast<int>(Wave::normalize_tag(too_long).size()));
        CHECK_STRING(name, QStringLiteral("阈值"), Wave::normalize_tag(QStringLiteral(" 阈值 ")));

        Wave wave;
        wave.tags = tag_list({ "Bass" });
        CHECK_TRUE(name, wave.has_tag(QStringLiteral("bass")));
        CHECK_TRUE(name, !wave.has_tag(QStringLiteral("Sub")));
        std::printf("[INFO] 标签规范化：trim / 空忽略 / 大小写不敏感去重 / 超过 %d 字符拒绝\n",
            Wave::TAG_MAX_LENGTH);
    }

    // 波形上的标签随波形一起落盘并可重新读回
    void test_round_trip(WaveLibrary& library, const std::string& waves_dir) {
        const char* name = "tags/round-trip";
        Wave wave;
        wave.name = "标签波形";
        wave.frames_v3 = { QStringLiteral("0A0A0A0A00000000"), QStringLiteral("0A0A0A0A14141414") };
        wave.tags = { QStringLiteral(" 节奏 "), QStringLiteral("节奏"), QStringLiteral("ELEC") };
        CHECK_TRUE(name, library.save(wave));
        CHECK_STRINGS(name, tag_list({ "节奏", "ELEC" }), tags_of(library, "标签波形"));

        library.reload();
        CHECK_STRINGS(name, tag_list({ "节奏", "ELEC" }), tags_of(library, "标签波形"));

        const nlohmann::json stored = parse_text(read_text_file(
            QString::fromStdString(waves_dir + "/waves.json")));
        CHECK_TRUE(name, stored.contains("waves") && stored["waves"].contains("标签波形"));
        if (stored.contains("waves") && stored["waves"].contains("标签波形")) {
            const nlohmann::json& entry = stored["waves"]["标签波形"];
            CHECK_TRUE(name, entry.contains("tags") && entry["tags"].is_array());
            CHECK_INT(name, 2, entry.contains("tags") ? static_cast<int>(entry["tags"].size()) : -1);
        }
        std::printf("[INFO] 波形标签 round-trip：写盘再读回后为 [节奏, ELEC]\n");
    }

    // 旧 waves.json（条目里没有 tags 字段）读入不报错，标签按空处理
    void test_legacy_store(WaveLibrary& library, const std::string& waves_dir) {
        const char* name = "tags/legacy";
        nlohmann::json waves = nlohmann::json::object();
        waves["旧波形"] = { { "name", "旧波形" },
            { "raw", "" },
            { "frames_v2", nlohmann::json::array() },
            { "frames_v3", { "0A0A0A0A28282828" } } };
        nlohmann::json root;
        root["version"] = "1.1";
        root["current"] = { { "A", "" }, { "B", "" } };
        root["waves"] = waves;
        CHECK_TRUE(name, write_json_file(QString::fromStdString(waves_dir + "/waves.json"), root));

        library.reload();
        const Wave* legacy = library.get("旧波形");
        CHECK_TRUE(name, legacy != nullptr);
        if (legacy != nullptr) {
            CHECK_INT(name, 0, static_cast<int>(legacy->tags.size()));
            CHECK_INT(name, 1, static_cast<int>(legacy->frames_v3.size()));
        }
        CHECK_INT(name, 16, library.official_count());
        std::printf("[INFO] 旧 waves.json（无 tags 字段）读入正常：标签为空，官方波形照常 %d 个\n",
            library.official_count());
    }

    // 导入 JSON 里出现且不属于内置标签的标签名自动登记进 tags.json
    void test_import_register(WaveLibrary& library, const std::string& waves_dir,
        const std::string& temp_dir) {
        const char* name = "tags/import-register";
        library.set_builtin_tags_for_test({ QStringLiteral("官方") });

        nlohmann::json items = nlohmann::json::array();
        items.push_back({ { "name", "导入甲" },
            { "tags", { "官方", "Sub", "sub", "Bass" } },
            { "expectedV3", { "0A0A0A0A00000000" } } });
        items.push_back({ { "name", "导入乙" },
            { "labels", { "Trippy" } },
            { "expectedV3", { "0A0A0A0A14141414" } } });
        const QString source = QString::fromStdString(temp_dir + "/import_tags.json");
        CHECK_TRUE(name, write_json_file(source, items));

        const WaveImportPlan plan = library.plan_import(source);
        CHECK_TRUE(name, plan.error.isEmpty());
        CHECK_INT(name, 2, static_cast<int>(plan.waves.size()));
        const WaveImportResult result = library.apply_import(plan, WaveConflictPolicy::Skip);
        CHECK_INT(name, 2, result.added);

        CHECK_STRINGS(name, tag_list({ "官方", "Sub", "Bass" }),
            tags_of(library, "导入甲"));
        CHECK_STRINGS(name, tag_list({ "Trippy" }), tags_of(library, "导入乙"));

        // 内置标签不写入自定义库；重名（大小写不同）不重复登记
        CHECK_STRINGS(name, tag_list({ "Sub", "Bass", "Trippy" }),
            library.custom_tags());
        CHECK_STRINGS(name, tag_list({ "Sub", "Bass", "Trippy" }),
            stored_tags(waves_dir));
        const nlohmann::json tag_file = parse_text(read_text_file(
            QString::fromStdString(waves_dir + "/tags.json")));
        CHECK_TRUE(name, tag_file.is_object() && tag_file.contains("version"));
        CHECK_INT(name, 1, tag_file.is_object() && tag_file.contains("version")
                ? tag_file["version"].get<int>()
                : -1);
        std::printf("[INFO] 导入自动登记：自定义标签库 [Sub, Bass, Trippy]，内置标签「官方」不入库\n");
    }

    // 内置标签不可删除
    void test_builtin_locked(WaveLibrary& library) {
        const char* name = "tags/builtin-locked";
        CHECK_TRUE(name, library.is_builtin_tag(QStringLiteral("官方")));
        CHECK_TRUE(name, !library.is_builtin_tag(QStringLiteral("Sub")));
        QString error;
        CHECK_INT(name, -1, library.remove_custom_tag(QStringLiteral("官方"), &error));
        CHECK_TRUE(name, error.contains(QStringLiteral("内置标签")));
        CHECK_TRUE(name, library.is_builtin_tag(QStringLiteral("官方")));
        CHECK_STRING(name, QStringLiteral("官方"), library.canonical_tag(QStringLiteral(" 官方 ")));
        CHECK_TRUE(name, library.all_tags().contains(QStringLiteral("官方")));
        std::printf("[INFO] 内置标签不可删：remove_custom_tag 返回 -1（%s）\n",
            error.toStdString().c_str());
    }

    // 删除自定义标签：使用中的波形一并移除；只从波形上移除不影响标签库
    void test_remove_detach(WaveLibrary& library, const std::string& waves_dir) {
        const char* name = "tags/remove-detach";
        Wave first;
        first.name = "用甲";
        first.frames_v3 = { QStringLiteral("0A0A0A0A00000000") };
        first.tags = { QStringLiteral("Bass"), QStringLiteral("Sub") };
        Wave second;
        second.name = "用乙";
        second.frames_v3 = { QStringLiteral("0A0A0A0A14141414") };
        second.tags = { QStringLiteral("sub") };
        CHECK_TRUE(name, library.save(first));
        CHECK_TRUE(name, library.save(second));
        // 「导入甲」也带 Sub：共三个波形在用
        CHECK_INT(name, 3, library.tag_usage(QStringLiteral("sub")));

        // 从波形上移除不动标签库
        CHECK_TRUE(name, library.detach_tag("用乙", QStringLiteral("Sub")));
        CHECK_INT(name, 2, library.tag_usage(QStringLiteral("Sub")));
        CHECK_TRUE(name, library.custom_tags().contains(QStringLiteral("Sub")));

        // 删除标签库条目：使用中的波形一并移除
        QString error;
        CHECK_INT(name, 2, library.remove_custom_tag(QStringLiteral("sub"), &error));
        CHECK_TRUE(name, error.isEmpty());
        CHECK_STRINGS(name, tag_list({ "Bass" }), tags_of(library, "用甲"));
        CHECK_STRINGS(name, QStringList(), tags_of(library, "用乙"));
        CHECK_TRUE(name, !library.custom_tags().contains(QStringLiteral("Sub")));
        // Bass 只从「用甲」上留下，标签库条目仍保留（移除波形上的标签不删库）
        CHECK_TRUE(name, library.custom_tags().contains(QStringLiteral("Bass")));
        CHECK_STRINGS(name, tag_list({ "Bass", "Trippy" }), stored_tags(waves_dir));
        std::printf("[INFO] 删除自定义标签：从 2 个使用中的波形上一并移除，标签库条目同时删除\n");
    }

    // JSON 导出含 tags 字段，导出的文件能再导入并保留标签
    void test_json_export(WaveLibrary& library, const std::string& temp_dir) {
        const char* name = "tags/json-export";
        Wave wave;
        wave.name = "导出波形";
        wave.raw = "Dungeonlab+pulse:35,1,8=0,20,0,1,1";
        wave.frames_v3 = { QStringLiteral("0A0A0A0A64646464") };
        wave.tags = tag_list({ "Bass", "Trippy" });
        CHECK_TRUE(name, library.save(wave));

        QString error;
        QStringList notes;
        const QString path = QString::fromStdString(temp_dir + "/export_tags.json");
        CHECK_TRUE(name, library.export_json(path, { "导出波形" }, error, notes));
        CHECK_TRUE(name, error.isEmpty());

        const nlohmann::json exported = parse_text(read_text_file(path));
        CHECK_TRUE(name, exported.is_array() && exported.size() == 1);
        if (exported.is_array() && !exported.empty()) {
            const nlohmann::json& entry = exported.at(0);
            CHECK_TRUE(name, entry.contains("tags") && entry["tags"].is_array());
            CHECK_INT(name, 2, entry.contains("tags") ? static_cast<int>(entry["tags"].size()) : -1);
            CHECK_TRUE(name, entry.contains("name") && entry.contains("raw")
                    && entry.contains("expectedV2") && entry.contains("expectedV3"));
        }

        // 删掉再导入：标签逐条还原
        CHECK_TRUE(name, library.remove("导出波形"));
        const WaveImportPlan plan = library.plan_import(path);
        CHECK_TRUE(name, plan.error.isEmpty());
        CHECK_INT(name, 1, static_cast<int>(plan.waves.size()));
        const WaveImportResult result = library.apply_import(plan, WaveConflictPolicy::Skip);
        CHECK_INT(name, 1, result.added);
        CHECK_STRINGS(name, tag_list({ "Bass", "Trippy" }),
            tags_of(library, "导出波形"));
        std::printf("[INFO] JSON 导出含 tags 字段，再导入后标签为 [Bass, Trippy]\n");
    }

    // 标签库始终无大小写重复条目
    void test_no_duplicates(WaveLibrary& library) {
        const char* name = "tags/no-duplicates";
        const QStringList custom = library.custom_tags();
        for (int outer = 0; outer < custom.size(); ++outer) {
            for (int inner = outer + 1; inner < custom.size(); ++inner) {
                CHECK_TRUE(name, custom.at(outer).compare(custom.at(inner), Qt::CaseInsensitive) != 0);
            }
        }
        CHECK_TRUE(name, !library.custom_tags().isEmpty());
        std::printf("[INFO] 自定义标签库无大小写重复条目：共 %d 个\n",
            static_cast<int>(library.custom_tags().size()));
    }

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    // 沙箱放在测试工作目录下（系统 TEMP 在部分环境里不可用），用例结束即递归删除
    QTemporaryDir temp(QDir::currentPath() + QStringLiteral("/dglab_wave_tag_XXXXXX"));
    if (!temp.isValid()) {
        std::fprintf(stderr, "FAIL: 临时目录创建失败\n");
        return 2;
    }
    const std::string temp_dir = temp.path().toStdString();
    const std::string config_dir = temp_dir + "/config";
    const std::string waves_dir = temp_dir + "/waves";
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

    test_normalize();
    test_round_trip(library, waves_dir);
    test_legacy_store(library, waves_dir);
    test_import_register(library, waves_dir, temp_dir);
    test_builtin_locked(library);
    test_remove_detach(library, waves_dir);
    test_json_export(library, temp_dir);
    test_no_duplicates(library);

    if (failures != 0) {
        std::printf("wave_tag_test: %d assertion(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    std::printf("wave_tag_test: all assertions passed\n");
    return EXIT_SUCCESS;
}
