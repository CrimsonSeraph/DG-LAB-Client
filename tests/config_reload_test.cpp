/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

// 配置热重读回归测试：外部改动经 reload() 生效并通知监听器、load() 仍幂等、
// save() 不丢外部新增的键、JSON 损坏时保留上一份有效配置、文件删除后重建可再读、
// 文件监听去抖后自动重读且自身写盘不触发重读循环、AppConfig::reload_all() 走同一条重读链路
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

    if (failures != 0) {
        std::printf("config_reload_test: %d assertion(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    std::printf("config_reload_test: all assertions passed\n");
    return EXIT_SUCCESS;
}
