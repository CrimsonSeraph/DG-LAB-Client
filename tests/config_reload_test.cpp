/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

// 配置热重读回归测试：外部改动经 reload() 生效并通知监听器、load() 仍幂等、
// save() 不丢外部新增的键、JSON 损坏时保留上一份有效配置、文件删除后重建可再读、
// 文件监听去抖后自动重读且自身写盘不触发重读循环、AppConfig::reload_all() 走同一条重读链路、
// 保存前三方合并的四条规则（内存新增必留、磁盘新增并入、双方都改留应用值、磁盘删除按内存是否改过分流），
// 以及运行期动态创建的 app.devices.<MAC> 子树连续写入与外部整份覆盖后仍完整
// 全部用例在临时目录内构造，不触碰仓库 config/

#include "AppConfig.h"
#include "ConfigManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QTemporaryDir>
#include <QTimer>

#include <nlohmann/json.hpp>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#define CHECK_INT(case_name, expected, actual) check_int((case_name), (expected), (actual), #actual, __LINE__)
#define CHECK_TRUE(case_name, expr) check_true((case_name), (expr), #expr, __LINE__)
#define CHECK_STRING(case_name, expected, actual) check_string((case_name), (expected), (actual), #actual, __LINE__)

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
        char detail[256] = {0};
        std::snprintf(detail, sizeof(detail), "%s expected true actual false", expr);
        fail(case_name, detail, line);
    }

    // 字符串断言
    void check_string(const char* case_name, const std::string& expected, const std::string& actual,
        const char* expr, int line) {
        if (expected == actual) {
            return;
        }
        char detail[512] = {0};
        std::snprintf(detail, sizeof(detail), "%s expected [%s] actual [%s]", expr, expected.c_str(), actual.c_str());
        fail(case_name, detail, line);
    }

    // 整份写文件（覆盖）
    void write_text(const std::string& path, const std::string& text) {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out << text;
    }

    // 整份读文件
    std::string read_text(const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        std::stringstream buffer;
        buffer << in.rdbuf();
        return buffer.str();
    }

    // 合法配置骨架（含保留字段与通道上限）
    nlohmann::json base_config(int priority, int limit_a) {
        nlohmann::json config = nlohmann::json::object();
        config["__priority"] = priority;
        config["version"] = "1.0";
        config["DGLABClient"] = "DG-LAB-Client";
        config["app"]["channels"]["A"]["limit"] = limit_a;
        config["app"]["channels"]["A"]["safety"] = 150;
        return config;
    }

    // 解析文件内容（失败返回 discarded，交给调用方断言）
    nlohmann::json parse_file(const std::string& path) {
        return nlohmann::json::parse(read_text(path), nullptr, false);
    }

    // 取 app.devices.<mac> 子树（缺失时返回空对象，避免 const 下标断言）
    nlohmann::json device_section(const nlohmann::json& root, const std::string& mac) {
        if (!root.is_object() || !root.contains("app") || !root["app"].contains("devices")
            || !root["app"]["devices"].contains(mac)) {
            return nlohmann::json::object();
        }
        return root["app"]["devices"][mac];
    }

    // 推进事件循环指定毫秒（文件监听去抖需要事件循环才能到点）
    void spin(int milliseconds) {
        QEventLoop loop;
        QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
        loop.exec();
    }

    // reload() 读入外部改动并通知监听器；load() 仍然幂等
    void test_reload_picks_up_change(const std::string& dir) {
        const std::string path = dir + "/reload_main.json";
        write_text(path, base_config(0, 200).dump(4));
        ConfigManager manager(path);
        CHECK_TRUE("reload-load", manager.load());
        CHECK_INT("reload-before", 200, manager.get<int>("app.channels.A.limit", -1));

        int notified = 0;
        manager.add_listener([&notified](const nlohmann::json&) { ++notified; });

        write_text(path, base_config(0, 180).dump(4));
        CHECK_TRUE("reload-load-idempotent", manager.load());
        CHECK_INT("reload-load-no-reread", 200, manager.get<int>("app.channels.A.limit", -1));

        CHECK_TRUE("reload-success", manager.reload());
        CHECK_INT("reload-new-value", 180, manager.get<int>("app.channels.A.limit", -1));
        CHECK_INT("reload-notify-count", 1, notified);
    }

    // 保存不丢外部新增的键，同时保住内存里刚改的值
    void test_save_keeps_external_keys(const std::string& dir) {
        const std::string path = dir + "/save_main.json";
        write_text(path, base_config(0, 200).dump(4));
        ConfigManager manager(path);
        CHECK_TRUE("save-load", manager.load());

        manager.set<std::string>("app.ui.theme", std::string("dark"));
        nlohmann::json external = base_config(0, 200);
        external["external_note"]["from_editor"] = 7;
        write_text(path, external.dump(4));

        CHECK_TRUE("save-success", manager.save());
        const nlohmann::json on_disk = parse_file(path);
        CHECK_TRUE("save-parsable", !on_disk.is_discarded());
        CHECK_TRUE("save-keeps-external-key", on_disk.contains("external_note"));
        if (on_disk.contains("external_note")) {
            CHECK_INT("save-external-key-value", 7, on_disk["external_note"]["from_editor"].get<int>());
        }
        CHECK_STRING("save-keeps-memory-value", "dark",
            on_disk["app"]["ui"]["theme"].is_string() ? on_disk["app"]["ui"]["theme"].get<std::string>() : std::string());
    }

    // JSON 损坏时保留上一份有效配置，且不改写磁盘
    void test_corrupt_json_keeps_value(const std::string& dir) {
        const std::string path = dir + "/corrupt_main.json";
        write_text(path, base_config(0, 200).dump(4));
        ConfigManager manager(path);
        CHECK_TRUE("corrupt-load", manager.load());

        write_text(path, "{ this is not json ");
        CHECK_TRUE("corrupt-reload-fails", !manager.reload());
        CHECK_INT("corrupt-keeps-value", 200, manager.get<int>("app.channels.A.limit", -1));
        CHECK_STRING("corrupt-file-untouched", "{ this is not json ", read_text(path));
    }

    // 文件删除后重建仍可重读
    void test_delete_and_recreate(const std::string& dir) {
        const std::string path = dir + "/recreate_main.json";
        write_text(path, base_config(0, 200).dump(4));
        ConfigManager manager(path);
        CHECK_TRUE("recreate-load", manager.load());

        std::filesystem::remove(path);
        CHECK_TRUE("recreate-missing-fails", !manager.reload());
        CHECK_INT("recreate-keeps-value", 200, manager.get<int>("app.channels.A.limit", -1));

        write_text(path, base_config(0, 90).dump(4));
        CHECK_TRUE("recreate-reload", manager.reload());
        CHECK_INT("recreate-new-value", 90, manager.get<int>("app.channels.A.limit", -1));
    }

    // 文件监听：连续写盘去抖成一次重读；自身保存不触发重读循环；其后的外部改动仍然生效
    void test_watch_debounce_and_self_write(const std::string& dir) {
        const std::string path = dir + "/watch_main.json";
        write_text(path, base_config(0, 200).dump(4));
        ConfigManager manager(path);
        CHECK_TRUE("watch-load", manager.load());

        int notified = 0;
        manager.add_listener([&notified](const nlohmann::json&) { ++notified; });
        manager.watch_changes(true);
        CHECK_TRUE("watch-enabled", manager.watching_changes());

        write_text(path, base_config(0, 170).dump(4));
        write_text(path, base_config(0, 160).dump(4));
        spin(900);
        CHECK_INT("watch-external-value", 160, manager.get<int>("app.channels.A.limit", -1));
        CHECK_INT("watch-debounce-single-notify", 1, notified);

        CHECK_TRUE("watch-save", manager.save());
        const int after_save = notified;
        spin(800);
        CHECK_INT("watch-self-write-suppressed", after_save, notified);

        write_text(path, base_config(0, 150).dump(4));
        spin(900);
        CHECK_INT("watch-external-after-save", 150, manager.get<int>("app.channels.A.limit", -1));
        CHECK_TRUE("watch-still-enabled", manager.watching_changes());

        manager.watch_changes(false);
        CHECK_TRUE("watch-disabled", !manager.watching_changes());
    }

    // AppConfig::reload_all() 走同一条重读链路，保存后外部新增的键仍在
    void test_app_config_reload_all(const std::string& dir) {
        const std::string config_dir = dir + "/app_config";
        std::filesystem::create_directories(config_dir);
        CHECK_TRUE("appconfig-init", AppConfig::instance().initialize(config_dir));

        const std::string user_path = config_dir + "/user.json";
        nlohmann::json user = parse_file(user_path);
        CHECK_TRUE("appconfig-user-parsable", !user.is_discarded());
        if (user.is_discarded()) {
            return;
        }
        user["app"]["channels"]["A"]["limit"] = 165;
        user["external_note"]["from_editor"] = 11;
        write_text(user_path, user.dump(4));

        AppConfig::instance().reload_all();
        CHECK_INT("appconfig-after-reload", 165, AppConfig::instance().get_value<int>("app.channels.A.limit", -1));

        CHECK_TRUE("appconfig-save", AppConfig::instance().save_all());
        const nlohmann::json saved = parse_file(user_path);
        CHECK_TRUE("appconfig-save-parsable", !saved.is_discarded());
        if (!saved.is_discarded()) {
            CHECK_TRUE("appconfig-save-keeps-external-key", saved.contains("external_note"));
            CHECK_INT("appconfig-save-keeps-limit", 165, saved["app"]["channels"]["A"]["limit"].get<int>());
        }
        AppConfig::instance().shutdown();
    }


    // 设备动态子树连续写入：逐键持久化后整棵子树完整，通道级配置不受影响
    // （app.devices.<MAC> 是运行期才创建的子树，基准里本来没有它）
    void test_device_subtree_sequential_writes(const std::string& dir) {
        const std::string path = dir + "/devices_main.json";
        write_text(path, base_config(0, 200).dump(4));
        ConfigManager manager(path);
        CHECK_TRUE("devices-load", manager.load());

        manager.set<std::string>("app.devices.AA:BB:CC:DD:EE:FF.model", std::string("coyote"));
        CHECK_TRUE("devices-save-model", manager.save());
        manager.set<int>("app.devices.AA:BB:CC:DD:EE:FF.limit_a", 50);
        CHECK_TRUE("devices-save-limit", manager.save());
        manager.set<int>("app.devices.AA:BB:CC:DD:EE:FF.safety_a", 120);
        CHECK_TRUE("devices-save-safety", manager.save());

        const nlohmann::json memory = manager.raw();
        const nlohmann::json on_disk = parse_file(path);
        CHECK_TRUE("devices-disk-parsable", !on_disk.is_discarded());
        std::printf("devices-subtree memory=%s\n",
            memory.contains("app") && memory["app"].contains("devices")
                ? memory["app"]["devices"].dump().c_str() : "<missing>");
        std::printf("devices-subtree disk  =%s\n",
            on_disk.contains("app") && on_disk["app"].contains("devices")
                ? on_disk["app"]["devices"].dump().c_str() : "<missing>");

        if (!on_disk.is_discarded()) {
            const nlohmann::json section = on_disk["app"]["devices"]["AA:BB:CC:DD:EE:FF"];
            CHECK_STRING("devices-disk-model", "coyote",
                section.contains("model") && section["model"].is_string()
                    ? section["model"].get<std::string>() : std::string());
            CHECK_INT("devices-disk-limit", 50, section.value("limit_a", -1));
            CHECK_INT("devices-disk-safety", 120, section.value("safety_a", -1));
            CHECK_INT("devices-channels-untouched", 200, on_disk["app"]["channels"]["A"]["limit"].get<int>());
            CHECK_INT("devices-channels-safety-untouched", 150,
                on_disk["app"]["channels"]["A"]["safety"].get<int>());
        }
        const nlohmann::json memory_section = memory["app"]["devices"]["AA:BB:CC:DD:EE:FF"];
        CHECK_STRING("devices-memory-model", "coyote",
            memory_section.value("model", std::string()));
        CHECK_INT("devices-memory-limit", 50, memory_section.value("limit_a", -1));
        CHECK_INT("devices-memory-safety", 120, memory_section.value("safety_a", -1));
        CHECK_INT("devices-memory-channels-untouched", 200,
            memory["app"]["channels"]["A"]["limit"].get<int>());
    }

    // 外部整份覆盖（旧快照没有动态设备子树）之后保存：应用自己写的设备键必须保留
    // 复现口径：合并基准取「上次写出的内容」时，这里会把 model 判成「磁盘删除」而丢掉
    void test_device_subtree_survives_stale_write(const std::string& dir) {
        const std::string mac = "AA:BB:CC:DD:EE:FF";
        // 变体一：外部快照只缺 model（子树形状仍在）
        {
            const std::string path = dir + "/devices_stale_key.json";
            write_text(path, base_config(0, 200).dump(4));
            ConfigManager manager(path);
            CHECK_TRUE("stale-key-load", manager.load());
            manager.set<std::string>("app.devices." + mac + ".model", std::string("coyote"));
            manager.set<int>("app.devices." + mac + ".limit_a", 50);
            CHECK_TRUE("stale-key-save", manager.save());

            nlohmann::json stale = manager.raw();
            stale["app"]["devices"][mac].erase("model");
            stale["external_note"]["from_editor"] = 7;
            write_text(path, stale.dump(4));

            manager.set<std::string>("app.ui.theme", std::string("dark"));
            CHECK_TRUE("stale-key-save-after", manager.save());
            const nlohmann::json on_disk = parse_file(path);
            CHECK_TRUE("stale-key-parsable", !on_disk.is_discarded());
            const nlohmann::json section = device_section(on_disk, mac);
            std::printf("stale-key disk devices=%s\n",
                on_disk.contains("app") && on_disk["app"].contains("devices")
                    ? on_disk["app"]["devices"].dump().c_str() : "<missing>");
            CHECK_STRING("stale-key-model-kept", "coyote", section.value("model", std::string()));
            CHECK_INT("stale-key-limit-kept", 50, section.value("limit_a", -1));
            CHECK_TRUE("stale-key-external-merged", on_disk.contains("external_note"));
            CHECK_INT("stale-key-channels-untouched", 200,
                on_disk["app"]["channels"]["A"]["limit"].get<int>());
        }
        // 变体二：外部快照整棵设备子树都没有
        {
            const std::string path = dir + "/devices_stale_tree.json";
            write_text(path, base_config(0, 200).dump(4));
            ConfigManager manager(path);
            CHECK_TRUE("stale-tree-load", manager.load());
            manager.set<std::string>("app.devices." + mac + ".model", std::string("coyote"));
            manager.set<int>("app.devices." + mac + ".limit_a", 50);
            manager.set<int>("app.devices." + mac + ".safety_a", 120);
            CHECK_TRUE("stale-tree-save", manager.save());

            nlohmann::json stale = base_config(0, 200);
            stale["external_note"]["from_editor"] = 9;
            write_text(path, stale.dump(4));

            manager.set<std::string>("app.ui.theme", std::string("light"));
            CHECK_TRUE("stale-tree-save-after", manager.save());
            const nlohmann::json on_disk = parse_file(path);
            CHECK_TRUE("stale-tree-parsable", !on_disk.is_discarded());
            const nlohmann::json section = device_section(on_disk, mac);
            std::printf("stale-tree disk devices=%s\n",
                on_disk.contains("app") && on_disk["app"].contains("devices")
                    ? on_disk["app"]["devices"].dump().c_str() : "<missing>");
            CHECK_STRING("stale-tree-model-kept", "coyote", section.value("model", std::string()));
            CHECK_INT("stale-tree-limit-kept", 50, section.value("limit_a", -1));
            CHECK_INT("stale-tree-safety-kept", 120, section.value("safety_a", -1));
            CHECK_TRUE("stale-tree-external-merged", on_disk.contains("external_note"));
            CHECK_INT("stale-tree-channels-untouched", 200,
                on_disk["app"]["channels"]["A"]["limit"].get<int>());
        }
    }

    // 三方合并的四条规则：内存新增必留、磁盘新增并入、双方都改留应用值、磁盘删除按内存是否改过分流
    void test_merge_rules(const std::string& dir) {
        // 规则一 / 二 / 四前半：基准 = 首次读盘内容
        const std::string path = dir + "/merge_rules_a.json";
        nlohmann::json initial = base_config(0, 200);
        initial["app"]["devices"] = nlohmann::json::object();
        initial["app"]["devices"]["MAC"] = nlohmann::json::object();
        initial["app"]["devices"]["MAC"]["limit_a"] = 50;
        write_text(path, initial.dump(4));
        ConfigManager manager(path);
        CHECK_TRUE("rules-load", manager.load());

        // 规则一：只在内存新增的键（基准与磁盘都没有）必须保留
        manager.set<std::string>("app.devices.MAC.model", std::string("coyote"));
        nlohmann::json external = base_config(0, 200);
        external["app"]["devices"] = nlohmann::json::object();
        external["app"]["devices"]["MAC"] = nlohmann::json::object();
        // 规则二：只在磁盘新增的键 → 并入内存
        external["app"]["devices"]["MAC"]["limit_b"] = 60;
        // 规则四前半：磁盘删除、内存未改 → 跟随删除（limit_a 来自磁盘且未改动）
        write_text(path, external.dump(4));
        CHECK_TRUE("rules-save-1", manager.save());

        const nlohmann::json on_disk = parse_file(path);
        CHECK_TRUE("rules-parsable-1", !on_disk.is_discarded());
        const nlohmann::json section = device_section(on_disk, "MAC");
        std::printf("merge-rules disk devices=%s\n",
            on_disk.is_object() && on_disk.contains("app") && on_disk["app"].contains("devices")
                ? on_disk["app"]["devices"].dump().c_str() : "<missing>");
        CHECK_STRING("rules-memory-only-kept", "coyote", section.value("model", std::string()));
        CHECK_TRUE("rules-disk-new-merged", section.contains("limit_b"));
        CHECK_INT("rules-disk-new-value", 60, section.value("limit_b", -1));
        CHECK_TRUE("rules-external-delete-followed", !section.contains("limit_a"));
        CHECK_TRUE("rules-memory-delete-followed", !device_section(manager.raw(), "MAC").contains("limit_a"));

        // 规则三 / 四后半：这两个键来自磁盘（基准里有），改内存后才有意义
        const std::string path2 = dir + "/merge_rules_b.json";
        nlohmann::json seeded = base_config(0, 200);
        seeded["app"]["devices"] = nlohmann::json::object();
        seeded["app"]["devices"]["MAC"] = nlohmann::json::object();
        seeded["app"]["devices"]["MAC"]["safety_a"] = 90;
        seeded["app"]["devices"]["MAC"]["limit_b"] = 60;
        write_text(path2, seeded.dump(4));
        ConfigManager manager2(path2);
        CHECK_TRUE("rules-load-2", manager2.load());

        // 规则三：双方都改同一个键 → 保留应用值
        manager2.set<int>("app.devices.MAC.safety_a", 130);
        nlohmann::json external2 = seeded;
        external2["app"]["devices"]["MAC"]["safety_a"] = 95;
        write_text(path2, external2.dump(4));
        CHECK_TRUE("rules-save-2", manager2.save());
        CHECK_INT("rules-both-changed-keeps-app", 130,
            device_section(parse_file(path2), "MAC").value("safety_a", -1));

        // 规则四后半：磁盘删除、内存也改过 → 保留内存值（同时记一条 WARN）
        manager2.set<int>("app.devices.MAC.limit_b", 70);
        nlohmann::json external3 = parse_file(path2);
        external3["app"]["devices"]["MAC"].erase("limit_b");
        write_text(path2, external3.dump(4));
        CHECK_TRUE("rules-save-3", manager2.save());
        CHECK_INT("rules-delete-conflict-keeps-memory", 70,
            device_section(parse_file(path2), "MAC").value("limit_b", -1));
    }

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    // 沙箱放在测试工作目录下（系统 TEMP 在部分环境里不可用），用例结束即递归删除
    QTemporaryDir temp(QDir::currentPath() + QStringLiteral("/dglab_config_reload_XXXXXX"));
    if (!temp.isValid()) {
        std::fprintf(stderr, "FAIL: 临时目录创建失败\n");
        return 2;
    }
    const std::string dir = temp.path().toStdString();

    test_reload_picks_up_change(dir);
    test_save_keeps_external_keys(dir);
    test_corrupt_json_keeps_value(dir);
    test_delete_and_recreate(dir);
    test_watch_debounce_and_self_write(dir);
    test_app_config_reload_all(dir);
    test_device_subtree_sequential_writes(dir);
    test_device_subtree_survives_stale_write(dir);
    test_merge_rules(dir);

    if (failures != 0) {
        std::printf("config_reload_test: %d assertion(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    std::printf("config_reload_test: all assertions passed\n");
    return EXIT_SUCCESS;
}
