/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "EventMath.h"

#include <cstdio>
#include <cstdlib>

#define CHECK_INT(case_name, expected, actual) check_int((case_name), (expected), (actual), #actual, __LINE__)
#define CHECK_TRUE(case_name, expr) check_true((case_name), (expr), #expr, __LINE__)

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
        std::snprintf(detail, sizeof(detail), "%s expected true", expr);
        fail(case_name, detail, line);
    }

    // 打印一条通道合并记录（与 DeviceController 的 DEBUG 日志同格式）
    void print_merge(const char* channel, int event_value, int rule_value, int final_value, int dispatched) {
        std::printf("[DEBUG] DeviceController::update_channel_output 通道%s 事件 %d / 规则 %d → 最终 %d（下发 %d 次）\n",
            channel, event_value, rule_value, final_value, dispatched);
    }

    // 事件输出大于规则命令时取事件值
    void test_event_over_rule() {
        const char* name = "merge/event_over_rule";
        ChannelOutputState state;
        state.rule_value = 25;
        state.event_value = 40;
        CHECK_INT(name, 40, state.final_value(100));
        int value = 0;
        CHECK_TRUE(name, state.take_output(100, value));
        CHECK_INT(name, 40, value);
        print_merge("A", state.event_value, state.rule_value, value, 1);
    }

    // 规则命令大于事件输出时取规则值
    void test_rule_over_event() {
        const char* name = "merge/rule_over_event";
        ChannelOutputState state;
        state.rule_value = 80;
        state.event_value = 30;
        CHECK_INT(name, 80, state.final_value(100));
        int value = 0;
        CHECK_TRUE(name, state.take_output(100, value));
        CHECK_INT(name, 80, value);
        print_merge("B", state.event_value, state.rule_value, value, 1);
    }

    // 规则与事件都超上限时按通道上限钳制
    void test_clamp_to_limit() {
        const char* name = "merge/clamp_to_limit";
        ChannelOutputState state;
        state.rule_value = 260;
        state.event_value = 230;
        CHECK_INT(name, 100, state.final_value(100));
        int value = 0;
        CHECK_TRUE(name, state.take_output(100, value));
        CHECK_INT(name, 100, value);
        print_merge("A", state.event_value, state.rule_value, value, 1);

        state.event_value = 0;
        CHECK_INT(name, 100, state.final_value(100));
        // 上限为 0 时任何输入都落回 0
        state.rule_value = 50;
        state.event_value = 50;
        CHECK_INT(name, 0, state.final_value(0));
    }

    // 相同最终值不重复下发
    void test_no_duplicate_output() {
        const char* name = "merge/no_duplicate_output";
        ChannelOutputState state;
        state.rule_value = 25;
        state.event_value = 40;
        int value = 0;
        int dispatched = 0;
        for (int frame = 0; frame < 8; ++frame) {
            // 每帧都重写同一组最近值，模拟事件强度不变
            state.event_value = 40;
            state.rule_value = 25;
            if (state.take_output(100, value)) {
                ++dispatched;
            }
        }
        CHECK_INT(name, 1, dispatched);
        print_merge("A", state.event_value, state.rule_value, value, dispatched);

        // 强度变化后重新下发一次
        state.event_value = 60;
        CHECK_TRUE(name, state.take_output(100, value));
        CHECK_INT(name, 60, value);
        CHECK_INT(name, 2, ++dispatched);

        // 无输出目标时复位后，同一强度会重新下发
        state.reset_output();
        CHECK_TRUE(name, state.take_output(100, value));
        CHECK_INT(name, 60, value);
        CHECK_INT(name, 3, ++dispatched);
    }

} // namespace

int main() {
    test_event_over_rule();
    test_rule_over_event();
    test_clamp_to_limit();
    test_no_duplicate_output();
    if (failures != 0) {
        std::printf("event_merge_test: %d assertion(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    std::printf("event_merge_test: all assertions passed\n");
    return EXIT_SUCCESS;
}
