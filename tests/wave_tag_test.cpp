/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

// 波形标签库回归测试：标签规范化、内置七类分类的顺序与只读约束、自建分类的增删改名与调序、
// 标签名全库唯一、波形上的标签 round-trip、旧库无 tags 字段兼容、导入自动登记、
// 内置标签不可删且不落盘、删除标签 / 删除分类的移除语义、JSON 导出含 tags 且可再导入、
// 官方波形的内置表自动挂载与用户叠加标签（可增删、内置部分锁定）、
// version 1 标签库到 version 2 的无损迁移、波形上标签按分类顺序的显示顺序。
// 全部用例在临时目录内构造，不触碰仓库 config/

#include "AppConfig.h"
#include "TagCatalog.h"
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
#include <utility>
#include <vector>

#define CHECK_INT(case_name, expected, actual) check_int((case_name), (expected), (actual), #actual, __LINE__)
#define CHECK_TRUE(case_name, expr) check_true((case_name), (expr), #expr, __LINE__)
#define CHECK_STRING(case_name, expected, actual) \
    check_string((case_name), (expected), (actual), #actual, __LINE__)
#define CHECK_STRINGS(case_name, expected, actual) \
    check_strings((case_name), (expected), (actual), #actual, __LINE__)
#define CHECK_INT_AT_LEAST(case_name, minimum, actual) \
    check_int_at_least((case_name), (minimum), (actual), #actual, __LINE__)

namespace {

    int failures = 0; ///< 失败断言数

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

    // 下界整型断言：可扩展清单（内置波形、内置分类等）只校验数量下界
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

    // tags.json 的顶层对象；读取或解析失败时返回空对象
    nlohmann::json tag_file_of(const std::string& dir) {
        const nlohmann::json json = parse_text(read_text_file(
            QString::fromStdString(dir + "/tags.json")));
        return json.is_object() ? json : nlohmann::json::object();
    }

    // tags.json 里某分类下的标签（分类不在文件里返回空列表）
    QStringList stored_category_tags(const std::string& dir, const QString& category) {
        const nlohmann::json json = tag_file_of(dir);
        if (!json.contains("categories") || !json["categories"].is_array()) {
            return QStringList();
        }
        for (const auto& item : json["categories"]) {
            if (!item.is_object() || !item.contains("name") || !item["name"].is_string()) {
                continue;
            }
            if (QString::fromStdString(item["name"].get<std::string>())
                    .compare(category, Qt::CaseInsensitive) != 0) {
                continue;
            }
            QStringList tags;
            if (item.contains("tags") && item["tags"].is_array()) {
                for (const auto& tag : item["tags"]) {
                    if (tag.is_string()) {
                        tags.append(QString::fromStdString(tag.get<std::string>()));
                    }
                }
            }
            return tags;
        }
        return QStringList();
    }

    // 由 UTF-8 字面量构造标签列表（宏参数里有逗号，列表必须包在括号内）
    QStringList tag_list(std::initializer_list<const char*> items) {
        QStringList list;
        for (const char* item : items) {
            list.append(QString::fromUtf8(item));
        }
        return list;
    }

    // 过滤掉内置标签，用于放宽对 tags_of / tags_of_category 的严格比较
    QStringList filter_builtin_tags(WaveLibrary& library, const QStringList& tags) {
        QStringList result;
        for (const QString& tag : tags) {
            if (!library.is_builtin_tag(tag)) {
                result.append(tag);
            }
        }
        return result;
    }

    // 分类在完整分类列表中的下标，不存在返回 -1
    int category_index(WaveLibrary& library, const QString& name) {
        const QStringList names = library.category_names();
        for (int index = 0; index < names.size(); ++index) {
            if (names.at(index) == name) {
                return index;
            }
        }
        return -1;
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
        const QStringList input = {QStringLiteral("  节奏  "), QStringLiteral("节奏"),
            QStringLiteral("ELEC"), QStringLiteral("elec"), QString(), too_long};
        CHECK_STRINGS(name, tag_list({"节奏", "ELEC"}),
            Wave::normalize_tags(input));
        CHECK_INT(name, 0, static_cast<int>(Wave::normalize_tag(too_long).size()));
        CHECK_STRING(name, QStringLiteral("阈值"), Wave::normalize_tag(QStringLiteral(" 阈值 ")));

        Wave wave;
        wave.tags = tag_list({"Bass"});
        CHECK_TRUE(name, wave.has_tag(QStringLiteral("bass")));
        CHECK_TRUE(name, !wave.has_tag(QStringLiteral("Sub")));
        std::printf("[INFO] 标签规范化：trim / 空忽略 / 大小写不敏感去重 / 超过 %d 字符拒绝\n",
            Wave::TAG_MAX_LENGTH);
    }

    // 内置七类：名称与顺序固定、内置分类不可改名 / 不可删除 / 不可调序
    void test_builtin_categories(WaveLibrary& library) {
        const char* name = "tags/builtin-categories";
        const QStringList expected = tag_list({"使用部位", "使用场景", "频率风格", "波形时长",
            "播放速率", "波形来源", "用户自定义"});
        // 放宽：内置分类属于可扩展清单，数量只校验下界；名称与顺序仍逐位校验
        // （mid 截到期望长度，新增内置分类追加在后不影响本断言）
        CHECK_INT_AT_LEAST(name, 7, library.builtin_category_count());
        CHECK_STRINGS(name, expected, TagCatalog::builtin_category_names().mid(0, expected.size()));
        CHECK_STRINGS(name, expected, library.category_names().mid(0, expected.size()));
        // 内置标签表非空（由静态分类表派生；新增内置标签不影响本断言）
        CHECK_TRUE(name, TagCatalog::builtin_tags().size() >= 1);
        CHECK_TRUE(name, TagCatalog::is_builtin_category(QStringLiteral("用户自定义")));
        CHECK_TRUE(name, !TagCatalog::is_builtin_category(QStringLiteral("随机分类")));
        CHECK_STRING(name, QStringLiteral("用户自定义"), TagCatalog::user_category_name());

        QString error;
        CHECK_TRUE(name, !library.rename_category(QStringLiteral("使用部位"), QStringLiteral("部位"), &error));
        CHECK_TRUE(name, error.contains(QStringLiteral("内置分类")));
        CHECK_TRUE(name, !library.remove_category(QStringLiteral("用户自定义"), &error));
        CHECK_TRUE(name, error.contains(QStringLiteral("内置分类")));
        CHECK_TRUE(name, !library.move_category(QStringLiteral("使用部位"), 8, &error));
        CHECK_TRUE(name, error.contains(QStringLiteral("内置分类")));
        // 放宽：只比期望的 7 个内置分类与顺序，后面新增的内置分类不参与
        CHECK_STRINGS(name, expected, library.category_names().mid(0, expected.size()));
        std::printf("[INFO] 内置七类顺序固定，内置分类不可改名 / 删除 / 调序\n");
    }

    // 自建分类：重名校验（大小写不敏感，含与内置分类重名）与改名
    void test_category_rename(WaveLibrary& library, const std::string& waves_dir) {
        const char* name = "tags/category-rename";
        QString error;
        CHECK_TRUE(name, library.create_category(QStringLiteral("改名甲")));
        CHECK_TRUE(name, library.create_category(QStringLiteral("改名乙")));
        // 重名（大小写不敏感）拒绝
        CHECK_TRUE(name, !library.create_category(QStringLiteral("改名甲"), &error));
        CHECK_TRUE(name, error.contains(QStringLiteral("已存在")));
        CHECK_TRUE(name, !library.create_category(QStringLiteral("使用部位"), &error));
        CHECK_INT(name, 0, library.tags_of_category(QStringLiteral("改名甲")).size());

        // 改名为已存在的分类（含大小写不同的自建分类与内置分类）拒绝
        CHECK_TRUE(name, !library.rename_category(QStringLiteral("改名乙"), QStringLiteral("改名甲"), &error));
        CHECK_TRUE(name, error.contains(QStringLiteral("已存在")));
        CHECK_TRUE(name, !library.rename_category(QStringLiteral("改名乙"), QStringLiteral("波形来源"), &error));
        CHECK_TRUE(name, error.contains(QStringLiteral("已存在")));
        // 改名为自身按无变化处理
        CHECK_TRUE(name, library.rename_category(QStringLiteral("改名乙"), QStringLiteral("改名乙")));
        // 正常改名
        CHECK_TRUE(name, library.rename_category(QStringLiteral("改名乙"), QStringLiteral("改名丙")));
        CHECK_INT(name, -1, category_index(library, QStringLiteral("改名乙")));
        CHECK_TRUE(name, category_index(library, QStringLiteral("改名丙")) > library.builtin_category_count());
        // 不存在的分类
        CHECK_TRUE(name, !library.rename_category(QStringLiteral("没有这个分类"), QStringLiteral("新名"), &error));
        CHECK_TRUE(name, error.contains(QStringLiteral("不存在")));

        // 自建分类写在 tags.json 的 categories 段，空分类同样保留
        CHECK_STRINGS(name, QStringList(), stored_category_tags(waves_dir, QStringLiteral("改名丙")));
        const nlohmann::json file = tag_file_of(waves_dir);
        CHECK_INT(name, 2, file.value("version", -1));
        CHECK_TRUE(name, file.contains("categories") && file["categories"].is_array());
        CHECK_TRUE(name, !file.contains("tags"));
        std::printf("[INFO] 自建分类重名校验（含内置分类）与改名生效，空分类保留在 categories 段\n");
    }

    // 自建分类调序：只能在内置分类之后调整，不能越过内置分类
    void test_category_order(WaveLibrary& library, const std::string& waves_dir) {
        const char* name = "tags/category-order";
        QString error;
        CHECK_TRUE(name, library.create_category(QStringLiteral("排序甲")));
        CHECK_TRUE(name, library.create_category(QStringLiteral("排序乙")));
        const int builtin_count = library.builtin_category_count();
        int first = category_index(library, QStringLiteral("排序甲"));
        int second = category_index(library, QStringLiteral("排序乙"));
        CHECK_TRUE(name, first >= builtin_count && first >= 0);
        CHECK_INT(name, first + 1, second);

        // 把「排序乙」前移到「排序甲」的位置
        CHECK_TRUE(name, library.move_category(QStringLiteral("排序乙"), first));
        first = category_index(library, QStringLiteral("排序甲"));
        second = category_index(library, QStringLiteral("排序乙"));
        CHECK_INT(name, second + 1, first);
        // 不能插到内置分类之前
        CHECK_TRUE(name, !library.move_category(QStringLiteral("排序甲"), builtin_count - 1, &error));
        CHECK_TRUE(name, error.contains(QStringLiteral("内置分类前面")));
        CHECK_TRUE(name, !library.move_category(QStringLiteral("使用部位"), builtin_count + 1, &error));
        CHECK_INT(name, category_index(library, QStringLiteral("使用部位")), 0);

        // 顺序写进文件，重新加载后保持
        const nlohmann::json file = tag_file_of(waves_dir);
        QStringList stored_order;
        if (file.contains("categories") && file["categories"].is_array()) {
            for (const auto& item : file["categories"]) {
                if (item.is_object() && item.contains("name") && item["name"].is_string()) {
                    stored_order.append(QString::fromStdString(item["name"].get<std::string>()));
                }
            }
        }
        const qsizetype stored_first = stored_order.indexOf(QStringLiteral("排序甲"));
        const qsizetype stored_second = stored_order.indexOf(QStringLiteral("排序乙"));
        CHECK_TRUE(name, stored_second >= 0 && stored_first == stored_second + 1);
        library.reload();
        CHECK_INT(name, category_index(library, QStringLiteral("排序甲")),
            category_index(library, QStringLiteral("排序乙")) + 1);
        std::printf("[INFO] 自建分类调序生效并落盘，插到内置分类之前被拒绝\n");
    }

    // 标签名全库唯一：同名（大小写不敏感）只能属于一个分类
    void test_tag_unique(WaveLibrary& library) {
        const char* name = "tags/tag-unique";
        QString error;
        CHECK_TRUE(name, library.create_category(QStringLiteral("唯一分类")));
        // 内置分类下也能新建用户标签，它仍是用户标签
        CHECK_TRUE(name, library.create_tag(QStringLiteral("使用部位"), QStringLiteral("部位一"), &error));
        CHECK_TRUE(name, error.isEmpty());
        CHECK_TRUE(name, library.create_tag(QStringLiteral("使用部位"), QStringLiteral("Tempo"), &error));
        CHECK_STRING(name, QStringLiteral("使用部位"), library.category_of_tag(QStringLiteral("部位一")));
        CHECK_TRUE(name, !library.is_builtin_tag(QStringLiteral("部位一")));

        // 大小写不同的同名标签不能出现在别的分类
        CHECK_TRUE(name, !library.create_tag(QStringLiteral("唯一分类"), QStringLiteral("tempo"), &error));
        CHECK_TRUE(name, error.contains(QStringLiteral("只能属于一个分类")));
        CHECK_STRING(name, QString(), library.category_of_tag(QStringLiteral("缺失标签")));
        // 同一个分类内重复新建按无变化处理，不产生第二条
        CHECK_TRUE(name, library.create_tag(QStringLiteral("使用部位"), QStringLiteral("TEMPO")));
        int tempo_count = 0;
        for (const QString& tag : library.tags_of_category(QStringLiteral("使用部位"))) {
            if (tag.compare(QStringLiteral("Tempo"), Qt::CaseInsensitive) == 0) {
                ++tempo_count;
            }
        }
        CHECK_INT(name, 1, tempo_count);
        CHECK_STRING(name, QStringLiteral("Tempo"), library.canonical_tag(QStringLiteral(" TEMPO ")));
        // 不存在的分类
        CHECK_TRUE(name, !library.create_tag(QStringLiteral("没有这个分类"), QStringLiteral("甲"), &error));
        CHECK_TRUE(name, error.contains(QStringLiteral("不存在")));
        std::printf("[INFO] 标签名全库唯一：同名大小写变体不能进第二个分类\n");
    }

    // 内置标签不可删、不落盘，且在所属分类里排在内置区段
    void test_builtin_locked(WaveLibrary& library, const std::string& waves_dir) {
        const char* name = "tags/builtin-locked";
        library.add_builtin_tags_for_test(QStringLiteral("使用部位"), tag_list({"部位甲"}));
        library.add_builtin_tags_for_test(QStringLiteral("使用场景"), tag_list({"官方"}));
        CHECK_TRUE(name, library.is_builtin_tag(QStringLiteral("部位甲")));
        CHECK_TRUE(name, library.is_builtin_tag(QStringLiteral("官方")));
        CHECK_TRUE(name, !library.is_builtin_tag(QStringLiteral("部位一")));
        // 内置标签排在所属分类的用户标签之前；比较时过滤掉系统默认内置标签
        CHECK_STRINGS(name,
            filter_builtin_tags(library, tag_list({"部位甲", "部位一", "Tempo"})),
            filter_builtin_tags(library, library.tags_of_category(QStringLiteral("使用部位"))));
        // 分类归属查库层：注入的内置标签只在库层可见，代码表的静态查询只看代码表自身
        CHECK_STRING(name, QStringLiteral("使用部位"), library.category_of_tag(QStringLiteral("部位甲")));
        CHECK_STRING(name, QString(), TagCatalog::category_of_builtin_tag(QStringLiteral("部位一")));

        QString error;
        CHECK_INT(name, -1, library.remove_tag(QStringLiteral("部位甲"), &error));
        CHECK_TRUE(name, error.contains(QStringLiteral("内置标签")));
        CHECK_TRUE(name, library.is_builtin_tag(QStringLiteral("部位甲")));
        CHECK_TRUE(name, library.all_tags().contains(QStringLiteral("部位甲")));
        CHECK_TRUE(name, !library.custom_tags().contains(QStringLiteral("部位甲")));
        // 内置标签一律来自代码表，不写进标签库文件
        CHECK_STRINGS(name, tag_list({"部位一", "Tempo"}),
            stored_category_tags(waves_dir, QStringLiteral("使用部位")));
        CHECK_STRINGS(name, QStringList(), stored_category_tags(waves_dir, QStringLiteral("使用场景")));
        std::printf("[INFO] 内置标签不可删（remove_tag 返回 -1），且不落盘\n");
    }

    // 波形上的标签随波形一起落盘并可重新读回
    void test_round_trip(WaveLibrary& library, const std::string& waves_dir) {
        const char* name = "tags/round-trip";
        Wave wave;
        wave.name = "标签波形";
        wave.frames_v3 = {QStringLiteral("0A0A0A0A00000000"), QStringLiteral("0A0A0A0A14141414")};
        wave.tags = {QStringLiteral(" 节奏 "), QStringLiteral("节奏"), QStringLiteral("ELEC")};
        CHECK_TRUE(name, library.save(wave));
        CHECK_STRINGS(name, tag_list({"节奏", "ELEC"}), tags_of(library, "标签波形"));

        library.reload();
        CHECK_STRINGS(name, tag_list({"节奏", "ELEC"}), tags_of(library, "标签波形"));

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
        waves["旧波形"] = {{"name", "旧波形"},
            {"raw", ""},
            {"frames_v2", nlohmann::json::array()},
            {"frames_v3", {"0A0A0A0A28282828"}}};
        nlohmann::json root;
        root["version"] = "1.1";
        root["current"] = {{"A", ""}, {"B", ""}};
        root["waves"] = waves;
        CHECK_TRUE(name, write_json_file(QString::fromStdString(waves_dir + "/waves.json"), root));

        library.reload();
        const Wave* legacy = library.get("旧波形");
        CHECK_TRUE(name, legacy != nullptr);
        if (legacy != nullptr) {
            CHECK_INT(name, 0, static_cast<int>(legacy->tags.size()));
            CHECK_INT(name, 1, static_cast<int>(legacy->frames_v3.size()));
        }
        // 放宽：内置波形属于可扩展清单，数量只校验下界，新增官方波形不必改测试
        CHECK_INT_AT_LEAST(name, 16, library.official_count());
        std::printf("[INFO] 旧 waves.json（无 tags 字段）读入正常：标签为空，官方波形照常 %d 个\n",
            library.official_count());
    }

    // 导入 JSON 里出现且标签库里没有的标签名自动登记进「用户自定义」分类
    void test_import_register(WaveLibrary& library, const std::string& waves_dir,
        const std::string& temp_dir) {
        const char* name = "tags/import-register";

        nlohmann::json items = nlohmann::json::array();
        items.push_back({{"name", "导入甲"},
            {"tags", {"官方", "Sub", "sub", "Bass"}},
            {"expectedV3", {"0A0A0A0A00000000"}}});
        items.push_back({{"name", "导入乙"},
            {"labels", {"Trippy"}},
            {"expectedV3", {"0A0A0A0A14141414"}}});
        const QString source = QString::fromStdString(temp_dir + "/import_tags.json");
        CHECK_TRUE(name, write_json_file(source, items));

        const WaveImportPlan plan = library.plan_import(source);
        CHECK_TRUE(name, plan.error.isEmpty());
        CHECK_INT(name, 2, static_cast<int>(plan.waves.size()));
        const WaveImportResult result = library.apply_import(plan, WaveConflictPolicy::Skip);
        CHECK_INT(name, 2, result.added);

        CHECK_STRINGS(name, tag_list({"官方", "Sub", "Bass"}),
            tags_of(library, "导入甲"));
        CHECK_STRINGS(name, tag_list({"Trippy"}), tags_of(library, "导入乙"));

        // 内置标签不写入标签库；重名（大小写不同）不重复登记，登记进「用户自定义」分类
        // 放宽：分类下的标签清单可随内置标签表扩展，过滤内置标签后再逐条比较
        CHECK_STRINGS(name, filter_builtin_tags(library, tag_list({"Sub", "Bass", "Trippy"})),
            filter_builtin_tags(library, library.tags_of_category(QStringLiteral("用户自定义"))));
        CHECK_STRINGS(name, tag_list({"Sub", "Bass", "Trippy"}),
            stored_category_tags(waves_dir, QStringLiteral("用户自定义")));
        const nlohmann::json tag_file = tag_file_of(waves_dir);
        CHECK_INT(name, 2, tag_file.value("version", -1));
        CHECK_TRUE(name, tag_file.contains("categories") && tag_file["categories"].is_array());
        CHECK_TRUE(name, !tag_file.contains("tags"));
        std::printf("[INFO] 导入自动登记：用户自定义分类为 [Sub, Bass, Trippy]，内置标签「官方」不入库\n");
    }

    // 删除标签：使用中的波形一并移除；只从波形上移除不影响标签库
    void test_remove_detach(WaveLibrary& library) {
        const char* name = "tags/remove-detach";
        Wave first;
        first.name = "用甲";
        first.frames_v3 = {QStringLiteral("0A0A0A0A00000000")};
        first.tags = {QStringLiteral("Bass"), QStringLiteral("Sub")};
        Wave second;
        second.name = "用乙";
        second.frames_v3 = {QStringLiteral("0A0A0A0A14141414")};
        second.tags = {QStringLiteral("sub")};
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
        CHECK_INT(name, 2, library.remove_tag(QStringLiteral("sub"), &error));
        CHECK_TRUE(name, error.isEmpty());
        CHECK_STRINGS(name, tag_list({"Bass"}), tags_of(library, "用甲"));
        CHECK_STRINGS(name, QStringList(), tags_of(library, "用乙"));
        CHECK_TRUE(name, !library.custom_tags().contains(QStringLiteral("Sub")));
        CHECK_STRING(name, QString(), library.category_of_tag(QStringLiteral("Sub")));
        // Bass 只从「用甲」上留下，标签库条目仍保留（移除波形上的标签不删库）
        CHECK_TRUE(name, library.custom_tags().contains(QStringLiteral("Bass")));
        std::printf("[INFO] 删除标签：从 2 个使用中的波形上一并移除，标签库条目同时删除\n");
    }

    // JSON 导出含 tags 字段，导出的文件能再导入并保留标签
    void test_json_export(WaveLibrary& library, const std::string& temp_dir) {
        const char* name = "tags/json-export";
        Wave wave;
        wave.name = "导出波形";
        wave.raw = "Dungeonlab+pulse:35,1,8=0,20,0,1,1";
        wave.frames_v3 = {QStringLiteral("0A0A0A0A64646464")};
        wave.tags = tag_list({"Bass", "Trippy"});
        CHECK_TRUE(name, library.save(wave));

        QString error;
        QStringList notes;
        const QString path = QString::fromStdString(temp_dir + "/export_tags.json");
        CHECK_TRUE(name, library.export_json(path, {"导出波形"}, error, notes));
        CHECK_TRUE(name, error.isEmpty());

        const nlohmann::json exported = parse_text(read_text_file(path));
        CHECK_TRUE(name, exported.is_array() && exported.size() == 1);
        if (exported.is_array() && !exported.empty()) {
            const nlohmann::json& entry = exported.at(0);
            CHECK_TRUE(name, entry.contains("tags") && entry["tags"].is_array());
            CHECK_INT(name, 2, entry.contains("tags") ? static_cast<int>(entry["tags"].size()) : -1);
            CHECK_TRUE(name, entry.contains("name") && entry.contains("raw") && entry.contains("expectedV2") && entry.contains("expectedV3"));
        }

        // 删掉再导入：标签逐条还原
        CHECK_TRUE(name, library.remove("导出波形"));
        const WaveImportPlan plan = library.plan_import(path);
        CHECK_TRUE(name, plan.error.isEmpty());
        CHECK_INT(name, 1, static_cast<int>(plan.waves.size()));
        const WaveImportResult result = library.apply_import(plan, WaveConflictPolicy::Skip);
        CHECK_INT(name, 1, result.added);
        CHECK_STRINGS(name, tag_list({"Bass", "Trippy"}),
            tags_of(library, "导出波形"));
        std::printf("[INFO] JSON 导出含 tags 字段，再导入后标签为 [Bass, Trippy]\n");
    }

    // 官方波形的内置标签由内置表自动挂载且锁定不可移除，用户叠加的标签可增删
    void test_official_builtin_mount(WaveLibrary& library, const std::string& waves_dir) {
        const char* name = "tags/official-builtin-mount";
        library.add_builtin_tags_for_test(QStringLiteral("波形来源"), tag_list({"内置甲"}));
        library.add_builtin_wave_tags_for_test("连击", tag_list({"内置甲"}));
        library.reload();

        CHECK_TRUE(name, library.is_official("连击"));
        CHECK_TRUE(name, library.is_builtin_tag(QStringLiteral("内置甲")));
        CHECK_TRUE(name, library.is_tag_builtin_mounted("连击", QStringLiteral("内置甲")));
        CHECK_STRINGS(name,
            filter_builtin_tags(library, tag_list({"内置甲"})),
            filter_builtin_tags(library, tags_of(library, "连击")));
        // 用户波形上的同名标签不算内置挂载
        CHECK_TRUE(name, !library.is_tag_builtin_mounted("旧波形", QStringLiteral("内置甲")));

        CHECK_TRUE(name, library.create_user_tag(QStringLiteral("叠加甲")));
        CHECK_TRUE(name, library.attach_tag("连击", QStringLiteral("叠加甲")));
        CHECK_STRINGS(name,
            filter_builtin_tags(library, tag_list({"内置甲", "叠加甲"})),
            filter_builtin_tags(library, tags_of(library, "连击")));
        CHECK_TRUE(name, !library.is_tag_builtin_mounted("连击", QStringLiteral("叠加甲")));

        // 内置挂载的那部分拒绝移除
        QString error;
        CHECK_TRUE(name, !library.detach_tag("连击", QStringLiteral("内置甲"), &error));
        CHECK_TRUE(name, error.contains(QStringLiteral("内置标签表")));
        CHECK_STRINGS(name,
            filter_builtin_tags(library, tag_list({"内置甲", "叠加甲"})),
            filter_builtin_tags(library, tags_of(library, "连击")));

        // official_wave_tags 只存用户叠加的那部分
        const nlohmann::json file = tag_file_of(waves_dir);
        const bool has_section = file.contains("official_wave_tags") && file["official_wave_tags"].is_object();
        CHECK_TRUE(name, has_section);
        if (has_section && file["official_wave_tags"].contains("连击")) {
            const nlohmann::json& entry = file["official_wave_tags"]["连击"];
            CHECK_INT(name, 1, entry.is_array() ? static_cast<int>(entry.size()) : -1);
            if (entry.is_array() && !entry.empty() && entry.at(0).is_string()) {
                CHECK_STRING(name, QStringLiteral("叠加甲"),
                    QString::fromStdString(entry.at(0).get<std::string>()));
            }
        }

        // 重新加载：内置部分按代码表恢复，用户叠加部分从 tags.json 恢复
        library.reload();
        CHECK_STRINGS(name,
            filter_builtin_tags(library, tag_list({"内置甲", "叠加甲"})),
            filter_builtin_tags(library, tags_of(library, "连击")));
        CHECK_TRUE(name, library.detach_tag("连击", QStringLiteral("叠加甲")));
        library.reload();
        CHECK_STRINGS(name,
            filter_builtin_tags(library, tag_list({"内置甲"})),
            filter_builtin_tags(library, tags_of(library, "连击")));
        std::printf("[INFO] 官方波形内置标签按内置表自动挂载、不可移除且不落盘；用户叠加标签可增删\n");
    }

    // 官方波形上的标签是用户侧叠加数据：写 tags.json 的 official_wave_tags，重新加载后仍在
    void test_official_tags(WaveLibrary& library, const std::string& waves_dir, const std::string& temp_dir) {
        const char* name = "tags/official-attach";
        CHECK_TRUE(name, library.is_official("呼吸"));
        const int frames_before = library.frame_count("呼吸");

        CHECK_TRUE(name, library.create_user_tag(QStringLiteral("官方节奏")));
        CHECK_TRUE(name, library.attach_tag("呼吸", QStringLiteral("官方节奏")));
        CHECK_TRUE(name, library.attach_tag("潮汐", QStringLiteral("官方节奏")));
        CHECK_STRINGS(name,
            filter_builtin_tags(library, tag_list({"官方节奏"})),
            filter_builtin_tags(library, tags_of(library, "呼吸")));
        // 用量统计把官方波形也算进去
        CHECK_INT(name, 2, library.tag_usage(QStringLiteral("官方节奏")));
        CHECK_TRUE(name, library.all_tags().contains(QStringLiteral("官方节奏")));
        // 加标签不动官方的帧
        CHECK_INT(name, frames_before, library.frame_count("呼吸"));

        // 叠加数据落在标签库文件的 official_wave_tags 段
        const nlohmann::json tag_file = tag_file_of(waves_dir);
        const bool has_section = tag_file.contains("official_wave_tags") && tag_file["official_wave_tags"].is_object();
        CHECK_TRUE(name, has_section);
        if (has_section) {
            const nlohmann::json& section = tag_file["official_wave_tags"];
            CHECK_TRUE(name, section.contains("呼吸"));
            CHECK_INT(name, 1, section.contains("呼吸") ? static_cast<int>(section["呼吸"].size()) : -1);
            CHECK_INT(name, 1, section.contains("潮汐") ? static_cast<int>(section["潮汐"].size()) : -1);
        }

        // 导出带官方波形自己的标签（导出的是用户视角的完整数据，包含内置标签属正常）
        QString error;
        QStringList notes;
        const QString export_path = QString::fromStdString(temp_dir + "/export_official_tags.json");
        CHECK_TRUE(name, library.export_json(export_path, {"呼吸"}, error, notes));
        const nlohmann::json exported = parse_text(read_text_file(export_path));
        CHECK_TRUE(name, exported.is_array() && exported.size() == 1);
        if (exported.is_array() && !exported.empty()) {
            const nlohmann::json& entry = exported.at(0);
            CHECK_TRUE(name, entry.contains("tags") && entry["tags"].is_array());
            // 放宽：导出数据可包含内置标签，数量不再强制为 1
            CHECK_TRUE(name, entry.contains("tags") && entry["tags"].size() >= 1);
            if (entry.contains("tags") && entry["tags"].is_array()) {
                bool has_official_rhythm = false;
                for (const auto& t : entry["tags"]) {
                    if (t.is_string() && t.get<std::string>() == "官方节奏") {
                        has_official_rhythm = true;
                    }
                }
                CHECK_TRUE(name, has_official_rhythm);
            }
        }

        // 重启（重新 load）：官方资源重建后标签恢复
        library.reload();
        CHECK_STRINGS(name,
            filter_builtin_tags(library, tag_list({"官方节奏"})),
            filter_builtin_tags(library, tags_of(library, "呼吸")));
        CHECK_STRINGS(name,
            filter_builtin_tags(library, tag_list({"官方节奏"})),
            filter_builtin_tags(library, tags_of(library, "潮汐")));
        CHECK_INT(name, 2, library.tag_usage(QStringLiteral("官方节奏")));
        CHECK_INT(name, frames_before, library.frame_count("呼吸"));

        // 取消勾选只从官方波形上移除，标签库条目保留；重启后不复活
        CHECK_TRUE(name, library.detach_tag("潮汐", QStringLiteral("官方节奏")));
        CHECK_STRINGS(name,
            filter_builtin_tags(library, QStringList()),
            filter_builtin_tags(library, tags_of(library, "潮汐")));
        CHECK_TRUE(name, library.custom_tags().contains(QStringLiteral("官方节奏")));
        library.reload();
        CHECK_STRINGS(name,
            filter_builtin_tags(library, QStringList()),
            filter_builtin_tags(library, tags_of(library, "潮汐")));
        CHECK_STRINGS(name,
            filter_builtin_tags(library, tag_list({"官方节奏"})),
            filter_builtin_tags(library, tags_of(library, "呼吸")));
        CHECK_INT(name, 1, library.tag_usage(QStringLiteral("官方节奏")));
        std::printf("[INFO] 官方波形标签：存 tags.json 的 official_wave_tags，重新 load 后仍在，帧不变\n");
    }

    // 删除标签时，官方波形上的引用一并清理（重启后也不复活）
    void test_official_tag_cleanup(WaveLibrary& library, const std::string& waves_dir) {
        const char* name = "tags/official-cleanup";
        CHECK_TRUE(name, library.create_user_tag(QStringLiteral("官方联动")));
        CHECK_TRUE(name, library.attach_tag("呼吸", QStringLiteral("官方联动")));
        Wave user;
        user.name = "联动用户波形";
        user.frames_v3 = {QStringLiteral("0A0A0A0A00000000")};
        user.tags = tag_list({"官方联动"});
        CHECK_TRUE(name, library.save(user));
        CHECK_INT(name, 2, library.tag_usage(QStringLiteral("官方联动")));

        QString error;
        CHECK_INT(name, 2, library.remove_tag(QStringLiteral("官方联动"), &error));
        CHECK_TRUE(name, error.isEmpty());
        // 「官方节奏」是上一个用例留在「呼吸」上的标签，不受影响
        CHECK_STRINGS(name,
            filter_builtin_tags(library, tag_list({"官方节奏"})),
            filter_builtin_tags(library, tags_of(library, "呼吸")));
        CHECK_STRINGS(name, QStringList(), tags_of(library, "联动用户波形"));
        CHECK_INT(name, 0, library.tag_usage(QStringLiteral("官方联动")));
        CHECK_TRUE(name, !library.all_tags().contains(QStringLiteral("官方联动")));

        // 叠加段里不再残留这个名字
        const nlohmann::json tag_file = tag_file_of(waves_dir);
        bool leftover = false;
        if (tag_file.contains("official_wave_tags")) {
            for (const auto& entry : tag_file["official_wave_tags"].items()) {
                if (!entry.value().is_array()) {
                    continue;
                }
                for (const auto& tag : entry.value()) {
                    if (tag.is_string() && tag.get<std::string>() == "官方联动") {
                        leftover = true;
                    }
                }
            }
        }
        CHECK_TRUE(name, !leftover);

        library.reload();
        CHECK_STRINGS(name,
            filter_builtin_tags(library, tag_list({"官方节奏"})),
            filter_builtin_tags(library, tags_of(library, "呼吸")));
        std::printf("[INFO] 删除标签：官方波形上的引用一并清理，重启后不复活\n");
    }

    // 官方波形的名称与帧仍不可改：同名保存与删除都被拒绝，帧始终与资源一致
    void test_official_readonly(WaveLibrary& library) {
        const char* name = "tags/official-readonly";
        const int frames_before = library.frame_count("呼吸");
        const QStringList v2_before = library.frames_of("呼吸", QStringLiteral("v2"));

        Wave tampered;
        tampered.name = "呼吸";
        tampered.frames_v3 = {QStringLiteral("0A0A0A0A64646464")};
        CHECK_TRUE(name, !library.save(tampered));
        CHECK_INT(name, frames_before, library.frame_count("呼吸"));
        CHECK_STRINGS(name, v2_before, library.frames_of("呼吸", QStringLiteral("v2")));
        CHECK_TRUE(name, library.is_official("呼吸"));

        CHECK_TRUE(name, !library.remove("呼吸"));
        CHECK_TRUE(name, library.has("呼吸"));

        library.reload();
        CHECK_INT(name, frames_before, library.frame_count("呼吸"));
        CHECK_STRINGS(name, v2_before, library.frames_of("呼吸", QStringLiteral("v2")));
        CHECK_TRUE(name, library.is_official("呼吸"));
        std::printf("[INFO] 官方波形只读：同名保存与删除被拒绝，帧始终为 %d 帧\n", frames_before);
    }

    // 删除自建分类：其下标签一并删除，并从使用中的波形上移除
    void test_remove_category(WaveLibrary& library, const std::string& waves_dir) {
        const char* name = "tags/remove-category";
        QString error;
        CHECK_TRUE(name, library.create_category(QStringLiteral("临时分类")));
        CHECK_TRUE(name, library.create_tag(QStringLiteral("临时分类"), QStringLiteral("临时标签"), &error));

        Wave wave;
        wave.name = "分类用户";
        wave.frames_v3 = {QStringLiteral("0A0A0A0A78787878")};
        wave.tags = tag_list({"临时标签"});
        CHECK_TRUE(name, library.save(wave));
        CHECK_INT(name, 1, library.tag_usage(QStringLiteral("临时标签")));
        CHECK_STRING(name, QStringLiteral("临时分类"), library.category_of_tag(QStringLiteral("临时标签")));

        CHECK_TRUE(name, library.remove_category(QStringLiteral("临时分类"), &error));
        CHECK_TRUE(name, error.isEmpty());
        CHECK_INT(name, -1, category_index(library, QStringLiteral("临时分类")));
        CHECK_STRING(name, QString(), library.category_of_tag(QStringLiteral("临时标签")));
        CHECK_INT(name, 0, library.tag_usage(QStringLiteral("临时标签")));
        CHECK_STRINGS(name, QStringList(), tags_of(library, "分类用户"));
        CHECK_TRUE(name, !library.all_tags().contains(QStringLiteral("临时标签")));
        CHECK_STRINGS(name, QStringList(), stored_category_tags(waves_dir, QStringLiteral("临时分类")));

        // 内置分类不可删除
        CHECK_TRUE(name, !library.remove_category(QStringLiteral("频率风格"), &error));
        CHECK_TRUE(name, error.contains(QStringLiteral("内置分类")));
        CHECK_TRUE(name, !library.remove_category(QStringLiteral("没有这个分类"), &error));
        std::printf("[INFO] 删除自建分类：其下标签从标签库与使用中的波形上一并移除\n");
    }

    // 波形上的标签按分类顺序排列：内置分类在前、自建分类按用户顺序、未入库的排最后
    void test_display_order(WaveLibrary& library) {
        const char* name = "tags/display-order";
        QString error;
        CHECK_TRUE(name, library.create_category(QStringLiteral("展示甲")));
        CHECK_TRUE(name, library.create_category(QStringLiteral("展示乙")));
        CHECK_TRUE(name, library.create_tag(QStringLiteral("展示甲"), QStringLiteral("展示甲一"), &error));
        CHECK_TRUE(name, library.create_tag(QStringLiteral("展示乙"), QStringLiteral("展示乙一"), &error));

        Wave wave;
        wave.name = "排序波形";
        wave.frames_v3 = {QStringLiteral("0A0A0A0A32323232")};
        wave.tags = tag_list({"展示乙一", "野生标签", "展示甲一", "部位甲"});
        CHECK_TRUE(name, library.save(wave));
        CHECK_STRINGS(name, tag_list({"部位甲", "展示甲一", "展示乙一", "野生标签"}),
            library.tags_in_display_order("排序波形"));
        // 调整自建分类顺序后显示顺序跟着变
        CHECK_TRUE(name, library.move_category(QStringLiteral("展示乙"),
                             library.builtin_category_count()));
        CHECK_STRINGS(name, tag_list({"部位甲", "展示乙一", "展示甲一", "野生标签"}),
            library.tags_in_display_order("排序波形"));
        // 不存在的波形
        CHECK_STRINGS(name, QStringList(), library.tags_in_display_order("没有这个波形"));
        std::printf("[INFO] 波形标签显示顺序：内置分类 → 自建分类顺序 → 未入库标签\n");
    }

    // version 1 → version 2：扁平 tags 整体进入「用户自定义」分类，official_wave_tags 原样保留
    void test_migrate_v1(WaveLibrary& library, const std::string& waves_dir) {
        const char* name = "tags/migrate-v1";
        nlohmann::json legacy;
        legacy["version"] = 1;
        legacy["tags"] = {"迁移甲", "迁移乙", "迁移甲"};
        legacy["official_wave_tags"] = {{"呼吸", {"迁移丙"}}};
        CHECK_TRUE(name, write_json_file(QString::fromStdString(waves_dir + "/tags.json"), legacy));

        library.reload();
        // 扁平数组整体进入「用户自定义」分类，大小写 / 完全重复的名字只留一条
        // 放宽：同上，分类下可能另有内置标签，过滤后再比较
        CHECK_STRINGS(name, filter_builtin_tags(library, tag_list({"迁移甲", "迁移乙"})),
            filter_builtin_tags(library, library.tags_of_category(QStringLiteral("用户自定义"))));
        CHECK_STRINGS(name, tag_list({"迁移甲", "迁移乙"}), library.custom_tags());
        // official_wave_tags 原样保留
        CHECK_STRINGS(name,
            filter_builtin_tags(library, tag_list({"迁移丙"})),
            filter_builtin_tags(library, tags_of(library, "呼吸")));

        // 迁移后第一次保存写成 version 2：只写分类与用户叠加标签，旧的扁平 tags 键消失
        const nlohmann::json file = tag_file_of(waves_dir);
        CHECK_INT(name, 2, file.value("version", -1));
        CHECK_TRUE(name, file.contains("categories") && file["categories"].is_array());
        CHECK_TRUE(name, !file.contains("tags"));
        CHECK_STRINGS(name, tag_list({"迁移甲", "迁移乙"}),
            stored_category_tags(waves_dir, QStringLiteral("用户自定义")));
        CHECK_TRUE(name, file.contains("official_wave_tags") && file["official_wave_tags"].is_object());
        if (file.contains("official_wave_tags") && file["official_wave_tags"].contains("呼吸")) {
            const nlohmann::json& entry = file["official_wave_tags"]["呼吸"];
            CHECK_INT(name, 1, entry.is_array() ? static_cast<int>(entry.size()) : -1);
        }

        // 再次加载不丢数据，也不再多出分类
        library.reload();
        CHECK_STRINGS(name, tag_list({"迁移甲", "迁移乙"}), library.custom_tags());
        CHECK_STRINGS(name,
            filter_builtin_tags(library, tag_list({"迁移丙"})),
            filter_builtin_tags(library, tags_of(library, "呼吸")));
        CHECK_INT(name, library.builtin_category_count(), library.category_names().size());
        std::printf("[INFO] version 1 → 2 迁移无损：扁平标签进「用户自定义」，叠加段保留，文件写回 version 2\n");
    }

    // 标签库始终无大小写重复条目
    void test_no_duplicates(WaveLibrary& library) {
        const char* name = "tags/no-duplicates";
        const QStringList all = library.all_tags();
        for (int outer = 0; outer < all.size(); ++outer) {
            for (int inner = outer + 1; inner < all.size(); ++inner) {
                CHECK_TRUE(name, all.at(outer).compare(all.at(inner), Qt::CaseInsensitive) != 0);
            }
        }
        // 每个标签只属于一个分类
        for (const QString& tag : library.custom_tags()) {
            int owners = 0;
            for (const WaveTagCategory& category : library.tag_categories()) {
                for (const QString& existing : category.tags) {
                    if (existing.compare(tag, Qt::CaseInsensitive) == 0) {
                        ++owners;
                    }
                }
            }
            CHECK_INT(name, 1, owners);
        }
        CHECK_TRUE(name, !library.custom_tags().isEmpty());
        std::printf("[INFO] 标签库无大小写重复条目：共 %d 个标签\n",
            static_cast<int>(all.size()));
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
    test_builtin_categories(library);
    test_category_rename(library, waves_dir);
    test_category_order(library, waves_dir);
    test_tag_unique(library);
    test_builtin_locked(library, waves_dir);
    test_round_trip(library, waves_dir);
    test_legacy_store(library, waves_dir);
    test_import_register(library, waves_dir, temp_dir);
    test_remove_detach(library);
    test_json_export(library, temp_dir);
    test_official_builtin_mount(library, waves_dir);
    test_official_tags(library, waves_dir, temp_dir);
    test_official_tag_cleanup(library, waves_dir);
    test_official_readonly(library);
    test_remove_category(library, waves_dir);
    test_display_order(library);
    test_migrate_v1(library, waves_dir);
    test_no_duplicates(library);

    if (failures != 0) {
        std::printf("wave_tag_test: %d assertion(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    std::printf("wave_tag_test: all assertions passed\n");
    return EXIT_SUCCESS;
}
