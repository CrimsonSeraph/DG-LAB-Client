/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

// 波形 zip 批量互操作回归测试：多波形导出 zip 后重新导入（冲突策略跳过）、
// 归档内混入非 .pulse 条目与损坏条目、路径穿越条目名被拒绝、单文件与总量解压上限，
// 以及 store 与 deflate 两种压缩方式的读取。
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

#include <miniz.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#define CHECK_INT(case_name, expected, actual) check_int((case_name), (expected), (actual), #actual, __LINE__)
#define CHECK_TRUE(case_name, expr) check_true((case_name), (expr), #expr, __LINE__)
#define CHECK_STRING(case_name, expected, actual) check_string((case_name), (expected), (actual), #actual, __LINE__)

namespace {

    int failures = 0; ///< 失败断言数

    /// @brief 测试用的合法 .pulse 文本
    const char* const PULSE_A = "Dungeonlab+pulse:35,1,8=0,20,0,1,1";

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
        fail(case_name,
            QStringLiteral("%1 expected %2 actual %3").arg(expr, expected, actual).toStdString().c_str(),
            line);
    }

    // 构造只有原始串的测试波形
    Wave make_wave(const char* name, const char* raw) {
        Wave wave;
        wave.name = name;
        wave.raw = raw;
        return wave;
    }

    // 以二进制写入文件
    bool write_file_bytes(const QString& path, const QByteArray& data) {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return false;
        }
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

    // 库中指定波形的 raw 原文
    QString raw_of(WaveLibrary& library, const char* name) {
        const Wave* wave = library.get(name);
        return wave == nullptr ? QString() : QString::fromStdString(wave->raw);
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

    // 用给定条目构造 zip 归档字节；level 取 0 为 store，6 为 deflate
    bool make_zip(const std::vector<std::pair<QString, QByteArray>>& items, int level, QByteArray& out) {
        out.clear();
        mz_zip_archive zip_archive;
        std::memset(&zip_archive, 0, sizeof(zip_archive));
        if (!mz_zip_writer_init_heap(&zip_archive, 0, 64 * 1024)) {
            return false;
        }
        for (const auto& item : items) {
            const QByteArray archive_name = item.first.toUtf8();
            const QByteArray& data = item.second;
            if (!mz_zip_writer_add_mem(&zip_archive, archive_name.constData(), data.constData(),
                    static_cast<size_t>(data.size()), static_cast<mz_uint>(level))) {
                mz_zip_writer_end(&zip_archive);
                return false;
            }
        }
        void* buffer = nullptr;
        size_t size = 0;
        if (!mz_zip_writer_finalize_heap_archive(&zip_archive, &buffer, &size) || buffer == nullptr) {
            mz_zip_writer_end(&zip_archive);
            return false;
        }
        out = QByteArray(static_cast<const char*>(buffer), static_cast<qsizetype>(size));
        mz_free(buffer);
        mz_zip_writer_end(&zip_archive);
        return true;
    }

    // 归档第一个条目的本地头压缩方式（0 = store，8 = deflate）
    int first_entry_method(const QByteArray& archive) {
        if (archive.size() < 10) {
            return -1;
        }
        const auto low = static_cast<unsigned char>(archive.at(8));
        const auto high = static_cast<unsigned char>(archive.at(9));
        return low | (high << 8);
    }

    // 导出全部波形为 zip → 清空用户库 → 重新导入（跳过）→ 数量与 raw 一致
    void test_zip_export_round_trip(WaveLibrary& library, const std::string& waves_dir,
        const QString& temp_dir) {
        const char* name = "zip/round_trip";
        CHECK_TRUE(name, reset_library(library, waves_dir));
        CHECK_TRUE(name, library.save(make_wave("测试甲", "Dungeonlab+pulse:AAA")));
        CHECK_TRUE(name, library.save(make_wave("快/速", "Dungeonlab+pulse:BBB")));
        CHECK_TRUE(name, library.save(make_wave("快\\速", "Dungeonlab+pulse:CCC")));

        const QString path = temp_dir + QStringLiteral("/wave_bundle.zip");
        QString error;
        QStringList notes;
        const std::vector<std::string> exported = { "测试甲", "快/速", "快\\速" };
        CHECK_TRUE(name, library.export_zip(path, exported, error, notes));
        CHECK_TRUE(name, error.isEmpty());
        CHECK_TRUE(name, notes.isEmpty());
        CHECK_TRUE(name, QFile::exists(path));

        // 归档结构：三个 .pulse（非法字符替换为下划线并去重）+ index.json 清单
        std::vector<wave_io::ZipEntry> entries;
        QStringList skipped;
        const QByteArray archive = read_file_bytes(path);
        CHECK_TRUE(name, wave_io::parse_zip(archive, entries, skipped, error));
        CHECK_INT(name, 4, static_cast<int>(entries.size()));
        CHECK_TRUE(name, skipped.isEmpty());
        CHECK_INT(name, 8, first_entry_method(archive));
        QStringList entry_names;
        int pulse_count = 0;
        for (const wave_io::ZipEntry& entry : entries) {
            entry_names.append(entry.name);
            if (entry.name.endsWith(QStringLiteral(".pulse"))) {
                ++pulse_count;
            }
        }
        CHECK_TRUE(name, entry_names.contains(QStringLiteral("测试甲.pulse")));
        CHECK_TRUE(name, entry_names.contains(QStringLiteral("快_速.pulse")));
        CHECK_TRUE(name, entry_names.contains(QStringLiteral("快_速 (2).pulse")));
        CHECK_TRUE(name, entry_names.contains(QStringLiteral("index.json")));
        CHECK_INT(name, 3, pulse_count);

        // 重新导入：名称由 index.json 清单恢复，raw 与原条目逐条一致
        CHECK_TRUE(name, reset_library(library, waves_dir));
        const WaveImportPlan plan = library.plan_import(path);
        CHECK_TRUE(name, plan.error.isEmpty());
        CHECK_INT(name, 3, static_cast<int>(plan.waves.size()));
        CHECK_TRUE(name, plan.skipped.isEmpty());
        CHECK_TRUE(name, plan.conflicts.isEmpty());
        const WaveImportResult result = library.apply_import(plan, WaveConflictPolicy::Skip);
        CHECK_INT(name, 3, result.added);
        CHECK_INT(name, 0, result.skipped);
        CHECK_STRING(name, QStringLiteral("Dungeonlab+pulse:AAA"), raw_of(library, "测试甲"));
        CHECK_STRING(name, QStringLiteral("Dungeonlab+pulse:BBB"), raw_of(library, "快/速"));
        CHECK_STRING(name, QStringLiteral("Dungeonlab+pulse:CCC"), raw_of(library, "快\\速"));
        std::printf("[INFO] zip 导出 %d 个波形，重新导入后新增 %d 条且 raw 逐条一致（%s）\n", pulse_count,
            result.added, result.summary().toStdString().c_str());

        // 范围「全部波形」：库中每个波形一个 .pulse，另加一份 index.json 清单
        const QString all_path = temp_dir + QStringLiteral("/wave_bundle_all.zip");
        CHECK_TRUE(name, library.export_zip(all_path, std::vector<std::string>(), error, notes));
        std::vector<wave_io::ZipEntry> all_entries;
        QStringList all_skipped;
        CHECK_TRUE(name, wave_io::parse_zip(read_file_bytes(all_path), all_entries, all_skipped, error));
        CHECK_INT(name, static_cast<int>(library.names().size()) + 1,
            static_cast<int>(all_entries.size()));
        std::printf("[INFO] 全部波形导出：库中 %d 个波形 → 归档 %d 个条目（含 index.json）\n",
            static_cast<int>(library.names().size()), static_cast<int>(all_entries.size()));
    }

    // 混入非 .pulse 条目与损坏的 index.json：只跳过非波形文件，目录内条目按文件名入库
    void test_zip_skips_non_pulse(WaveLibrary& library, const std::string& waves_dir,
        const QString& temp_dir) {
        const char* name = "zip/non_pulse";
        CHECK_TRUE(name, reset_library(library, waves_dir));
        QByteArray archive;
        CHECK_TRUE(name, make_zip({ { QStringLiteral("好波形.pulse"), QByteArray(PULSE_A) },
                                      { QStringLiteral("说明.txt"), QByteArray("hello") },
                                      { QStringLiteral("waves/子目录波形.pulse"), QByteArray(PULSE_A) },
                                      { QStringLiteral("index.json"), QByteArray("{ 不是合法 JSON") } },
                         6, archive));
        const QString path = temp_dir + QStringLiteral("/mixed.zip");
        CHECK_TRUE(name, write_file_bytes(path, archive));

        const WaveImportPlan plan = library.plan_import(path);
        CHECK_TRUE(name, plan.error.isEmpty());
        CHECK_INT(name, 2, static_cast<int>(plan.waves.size()));
        CHECK_INT(name, 1, static_cast<int>(plan.skipped.size()));
        CHECK_TRUE(name, plan.skipped.join(QStringLiteral(" | ")).contains(QStringLiteral("说明.txt")));
        bool has_folder_wave = false;
        for (const Wave& wave : plan.waves) {
            if (wave.name == "子目录波形") {
                has_folder_wave = true;
            }
        }
        CHECK_TRUE(name, has_folder_wave);
        const WaveImportResult result = library.apply_import(plan, WaveConflictPolicy::Skip);
        CHECK_INT(name, 2, result.added);
        CHECK_INT(name, 1, result.skipped);
        std::printf("[INFO] 非 .pulse 条目跳过并计数：%s\n",
            plan.skipped.join(QStringLiteral(" | ")).toStdString().c_str());
    }

    // 单个条目损坏不影响其它条目
    void test_zip_corrupt_entry(WaveLibrary& library, const std::string& waves_dir,
        const QString& temp_dir) {
        const char* name = "zip/corrupt_entry";
        CHECK_TRUE(name, reset_library(library, waves_dir));
        QByteArray archive;
        CHECK_TRUE(name, make_zip({ { QStringLiteral("损坏.pulse"), QByteArray("这不是脉冲文本") },
                                      { QStringLiteral("正常.pulse"), QByteArray(PULSE_A) } },
                         6, archive));
        const QString path = temp_dir + QStringLiteral("/corrupt.zip");
        CHECK_TRUE(name, write_file_bytes(path, archive));

        const WaveImportPlan plan = library.plan_import(path);
        CHECK_TRUE(name, plan.error.isEmpty());
        CHECK_INT(name, 1, static_cast<int>(plan.waves.size()));
        CHECK_INT(name, 1, static_cast<int>(plan.skipped.size()));
        CHECK_TRUE(name, plan.skipped.join(QStringLiteral(" | ")).contains(QStringLiteral("导入失败")));
        if (!plan.waves.empty()) {
            CHECK_STRING(name, QString::fromLatin1(PULSE_A), QString::fromStdString(plan.waves.front().raw));
        }
        std::printf("[INFO] 损坏条目已跳过，其余条目照常导入：%s\n",
            plan.skipped.join(QStringLiteral(" | ")).toStdString().c_str());
    }

    // 路径穿越条目名一律拒绝
    void test_zip_path_traversal(WaveLibrary& library, const std::string& waves_dir,
        const QString& temp_dir) {
        const char* name = "zip/path_traversal";
        CHECK_TRUE(name, reset_library(library, waves_dir));

        // miniz 写入侧会拒绝非法条目名：先写等长占位名，再把条目名替换成穿越名（只测读取侧）
        const QString placeholder_up = QStringLiteral("AAAAAAAAAAAAA");     // 13 字节
        const QString placeholder_abs = QStringLiteral("BBBBBBBBBB");      // 10 字节
        const QString placeholder_nested = QStringLiteral("CCCCCCCCCCCCCCCCC"); // 17 字节
        const QString placeholder_drive = QStringLiteral("DDDDDDDDDDDDDD"); // 14 字节
        QByteArray archive;
        CHECK_TRUE(name, make_zip({ { placeholder_up, QByteArray(PULSE_A) },
                                      { placeholder_abs, QByteArray(PULSE_A) },
                                      { placeholder_nested, QByteArray(PULSE_A) },
                                      { placeholder_drive, QByteArray(PULSE_A) },
                                      { QStringLiteral("正常.pulse"), QByteArray(PULSE_A) } },
                         6, archive));
        archive.replace(placeholder_up.toUtf8(), QByteArray("../evil.pulse"));
        archive.replace(placeholder_abs.toUtf8(), QByteArray("/abs.pulse"));
        archive.replace(placeholder_nested.toUtf8(), QByteArray("sub/../../x.pulse"));
        archive.replace(placeholder_drive.toUtf8(), QByteArray("C:/drive.pulse"));
        const QString path = temp_dir + QStringLiteral("/traversal.zip");
        CHECK_TRUE(name, write_file_bytes(path, archive));

        const WaveImportPlan plan = library.plan_import(path);
        CHECK_TRUE(name, plan.error.isEmpty());
        CHECK_INT(name, 1, static_cast<int>(plan.waves.size()));
        if (!plan.waves.empty()) {
            CHECK_STRING(name, QStringLiteral("正常"), QString::fromStdString(plan.waves.front().name));
        }
        CHECK_INT(name, 4, static_cast<int>(plan.skipped.size()));
        const QString reasons = plan.skipped.join(QStringLiteral(" | "));
        CHECK_TRUE(name, reasons.contains(QStringLiteral("绝对路径或含 ..")));
        CHECK_TRUE(name, reasons.contains(QStringLiteral("../evil.pulse")));
        CHECK_TRUE(name, reasons.contains(QStringLiteral("/abs.pulse")));
        CHECK_TRUE(name, reasons.contains(QStringLiteral("C:/drive.pulse")));
        CHECK_TRUE(name, !QFile::exists(temp_dir + QStringLiteral("/evil.pulse")));
        std::printf("[INFO] 4 个路径穿越条目全部拒绝：%s\n", reasons.toStdString().c_str());
    }

    // 解压大小上限：单文件超过 4 MiB 与总量超过 32 MiB 都报错并拒绝整个归档
    void test_zip_size_limits(WaveLibrary& library, const std::string& waves_dir,
        const QString& temp_dir) {
        const char* name = "zip/size_limit";
        CHECK_TRUE(name, reset_library(library, waves_dir));

        // 单文件超限：5 MiB（deflate 后归档很小，用来验证压缩炸弹拦截）
        QByteArray archive;
        CHECK_TRUE(name,
            make_zip({ { QStringLiteral("巨无霸.pulse"), QByteArray(5 * 1024 * 1024, 'A') } }, 6, archive));
        const QString big_path = temp_dir + QStringLiteral("/big_entry.zip");
        CHECK_TRUE(name, write_file_bytes(big_path, archive));
        CHECK_TRUE(name, archive.size() < 1024 * 1024);
        WaveImportPlan plan = library.plan_import(big_path);
        CHECK_TRUE(name, !plan.error.isEmpty());
        CHECK_TRUE(name, plan.error.contains(QStringLiteral("上限")));
        CHECK_TRUE(name, plan.error.contains(QStringLiteral("4 MiB")));
        CHECK_TRUE(name, plan.waves.empty());
        std::printf("[INFO] 单文件超限：%s（归档 %d 字节）\n", plan.error.toStdString().c_str(),
            static_cast<int>(archive.size()));

        // 总量超限：9 个 4 MiB 条目（每个都不超单文件上限，合计 36 MiB）
        QByteArray total_archive;
        std::vector<std::pair<QString, QByteArray>> items;
        for (int index = 0; index < 9; ++index) {
            items.push_back({ QStringLiteral("块%1.pulse").arg(index), QByteArray(4 * 1024 * 1024, 'B') });
        }
        CHECK_TRUE(name, make_zip(items, 6, total_archive));
        const QString total_path = temp_dir + QStringLiteral("/big_total.zip");
        CHECK_TRUE(name, write_file_bytes(total_path, total_archive));
        plan = library.plan_import(total_path);
        CHECK_TRUE(name, !plan.error.isEmpty());
        CHECK_TRUE(name, plan.error.contains(QStringLiteral("32 MiB")));
        CHECK_TRUE(name, plan.waves.empty());
        std::printf("[INFO] 总量超限：%s\n", plan.error.toStdString().c_str());
    }

    // 压缩方式：导出用 deflate，读取同时支持 store 与 deflate
    void test_zip_compression_methods(WaveLibrary& library, const std::string& waves_dir,
        const QString& temp_dir) {
        const char* name = "zip/compression";
        CHECK_TRUE(name, reset_library(library, waves_dir));

        // 导出侧：deflate（本地头压缩方式 = 8），读回内容一致
        QByteArray deflate_archive;
        QString build_error;
        const std::vector<wave_io::ZipEntry> written
            = { { QStringLiteral("压缩.pulse"), QByteArray(PULSE_A) } };
        CHECK_TRUE(name, wave_io::build_zip(written, deflate_archive, build_error));
        CHECK_TRUE(name, build_error.isEmpty());
        CHECK_INT(name, 8, first_entry_method(deflate_archive));
        std::vector<wave_io::ZipEntry> entries;
        QStringList skipped;
        QString error;
        CHECK_TRUE(name, wave_io::parse_zip(deflate_archive, entries, skipped, error));
        CHECK_INT(name, 1, static_cast<int>(entries.size()));
        if (!entries.empty()) {
            CHECK_STRING(name, QString::fromLatin1(PULSE_A), QString::fromUtf8(entries.front().data));
        }

        // 读取侧：store（压缩方式 = 0）的归档照常导入
        QByteArray store_archive;
        CHECK_TRUE(name,
            make_zip({ { QStringLiteral("不压缩.pulse"), QByteArray(PULSE_A) } }, 0, store_archive));
        CHECK_INT(name, 0, first_entry_method(store_archive));
        const QString store_path = temp_dir + QStringLiteral("/store.zip");
        CHECK_TRUE(name, write_file_bytes(store_path, store_archive));
        const WaveImportPlan plan = library.plan_import(store_path);
        CHECK_TRUE(name, plan.error.isEmpty());
        CHECK_INT(name, 1, static_cast<int>(plan.waves.size()));
        if (!plan.waves.empty()) {
            CHECK_STRING(name, QStringLiteral("不压缩"), QString::fromStdString(plan.waves.front().name));
        }
        std::printf("[INFO] 导出压缩方式 = deflate(8)，读取同时支持 store(0) 与 deflate(8)\n");
    }

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    // 沙箱放在测试工作目录下（系统 TEMP 在部分环境里不可用），用例结束即递归删除
    QTemporaryDir temp(QDir::currentPath() + QStringLiteral("/dglab_wave_zip_XXXXXX"));
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

    test_zip_export_round_trip(library, waves_dir, temp_dir);
    test_zip_skips_non_pulse(library, waves_dir, temp_dir);
    test_zip_corrupt_entry(library, waves_dir, temp_dir);
    test_zip_path_traversal(library, waves_dir, temp_dir);
    test_zip_size_limits(library, waves_dir, temp_dir);
    test_zip_compression_methods(library, waves_dir, temp_dir);

    if (failures != 0) {
        std::printf("wave_zip_test: %d assertion(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    std::printf("wave_zip_test: all assertions passed\n");
    return EXIT_SUCCESS;
}
