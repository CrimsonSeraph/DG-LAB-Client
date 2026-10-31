/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "EventConflict.h"
#include "EventMath.h"

#include <QString>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#define CHECK_INT(case_name, expected, actual) check_int((case_name), (expected), (actual), #actual, __LINE__)
#define CHECK_DOUBLE(case_name, expected, actual) check_double((case_name), (expected), (actual), #actual, __LINE__)
#define CHECK_STRING(case_name, expected, actual) check_string((case_name), (expected), (actual), #actual, __LINE__)
#define CHECK_TRUE(case_name, expr) check_true((case_name), (expr), #expr, __LINE__)

namespace {

    const double EPSILON = 1e-9; ///< 浮点比较容差

    int failures = 0; ///< 失败断言数

    // 记录一次断言失败
    void fail(const char* case_name, const char* detail, int line) {
        std::fprintf(stderr, "FAIL %s (line %d): %s\n", case_name, line, detail);
        ++failures;
    }

    // 整型断言：形参用 long long 与 %lld 直配，避免 qsizetype / size_t 传入时的收窄告警
    void check_int(const char* case_name, long long expected, long long actual, const char* expr, int line) {
        if (expected == actual) {
            return;
        }
        char detail[256] = {0};
        std::snprintf(detail, sizeof(detail), "%s expected %lld actual %lld", expr, expected, actual);
        fail(case_name, detail, line);
    }

    // 浮点断言
    void check_double(const char* case_name, double expected, double actual, const char* expr, int line) {
        if (std::fabs(expected - actual) <= EPSILON) {
            return;
        }
        char detail[256] = {0};
        std::snprintf(detail, sizeof(detail), "%s expected %.9f actual %.9f", expr, expected, actual);
        fail(case_name, detail, line);
    }

    // 字符串断言
    void check_string(const char* case_name, const std::string& expected, const std::string& actual,
        const char* expr, int line) {
        if (expected == actual) {
            return;
        }
        char detail[512] = {0};
        std::snprintf(detail, sizeof(detail), "%s expected \"%s\" actual \"%s\"", expr, expected.c_str(),
            actual.c_str());
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

    // 构造 set 输出
    OptionOutput make_set_output(const std::string& channel, std::int64_t edited_at) {
        OptionOutput output;
        output.channel = channel;
        output.operation = EventOperation::SET;
        output.edited_at = edited_at;
        return output;
    }

    // 构造 increase 输出
    OptionOutput make_increase_output(const std::string& channel) {
        OptionOutput output;
        output.channel = channel;
        output.operation = EventOperation::INCREASE;
        return output;
    }

    // 构造子选项条目
    OptionBinding make_binding(const std::string& event_id, const std::string& option_id,
        const OptionOutput& output) {
        OptionBinding binding;
        binding.key.event_id = event_id;
        binding.key.option_id = option_id;
        binding.output = output;
        return binding;
    }

    // 被关闭项的首个展示标识（空列表返回空串）
    std::string first_disabled(const std::vector<SetConflictLoss>& losses) {
        return losses.empty() ? std::string() : option_key_display(losses.front().key);
    }

    // 被关闭项的首个落败通道数（空列表返回 0）
    int first_channel_count(const std::vector<SetConflictLoss>& losses) {
        return losses.empty() ? 0 : static_cast<int>(losses.front().channels.size());
    }

    // 冲突条目的首个展示标识（空列表返回空串）
    std::string first_conflict(const std::vector<OptionBinding>& conflicts) {
        return conflicts.empty() ? std::string() : option_key_display(conflicts.front().key);
    }

    void test_clamp_strength() {
        CHECK_INT("clamp_strength/negative", 0, clamp_strength(-5, 200, 150));
        CHECK_INT("clamp_strength/limit_below_safety", 150, clamp_strength(180, 150, 200));
        CHECK_INT("clamp_strength/safety_below_limit", 150, clamp_strength(180, 200, 150));
        CHECK_INT("clamp_strength/within_bounds", 120, clamp_strength(120, 200, 150));
        CHECK_INT("clamp_strength/zero_bounds", 0, clamp_strength(10, 0, 0));
        CHECK_INT("clamp_strength/negative_limit", 0, clamp_strength(10, -10, 50));
    }

    void test_apply_merge() {
        CHECK_DOUBLE("apply_merge/empty", 0.0, apply_merge(MergeRule::MULTIPLY, {}, {}));
        CHECK_DOUBLE("apply_merge/multiply", 0.2, apply_merge(MergeRule::MULTIPLY, {0.5, 0.4}, {}));
        CHECK_DOUBLE("apply_merge/max", 0.5, apply_merge(MergeRule::MAX, {0.5, 0.4}, {}));
        CHECK_DOUBLE("apply_merge/min", 0.4, apply_merge(MergeRule::MIN, {0.5, 0.4}, {}));
        CHECK_DOUBLE("apply_merge/weighted", 0.425, apply_merge(MergeRule::WEIGHTED, {0.5, 0.4}, {1.0, 3.0}));
        CHECK_DOUBLE("apply_merge/weighted_zero_sum", 0.45, apply_merge(MergeRule::WEIGHTED, {0.5, 0.4}, {0.0, 0.0}));
    }

    void test_normalize_value() {
        CHECK_DOUBLE("normalize_value/below_min", 0.0, normalize_value(-5, 0, 10));
        CHECK_DOUBLE("normalize_value/above_max", 1.0, normalize_value(15, 0, 10));
        CHECK_DOUBLE("normalize_value/linear", 0.5, normalize_value(5, 0, 10));
        CHECK_DOUBLE("normalize_value/zero_span", 0.0, normalize_value(5, 10, 10));
        CHECK_DOUBLE("normalize_value/inverted_span", 0.0, normalize_value(5, 10, 0));
    }

    void test_wave_envelope_percent() {
        CHECK_INT("wave_envelope_percent/full", 100, wave_envelope_percent(QStringLiteral("0A0A0A0A64646464")));
        CHECK_INT("wave_envelope_percent/zero", 0, wave_envelope_percent(QStringLiteral("0000000000000000")));
        CHECK_INT("wave_envelope_percent/mean", 75, wave_envelope_percent(QStringLiteral("0A0A0A0A32326464")));
        CHECK_INT("wave_envelope_percent/short", 0, wave_envelope_percent(QStringLiteral("0A0A0A0A")));
        CHECK_INT("wave_envelope_percent/empty", 0, wave_envelope_percent(QString()));
        CHECK_INT("wave_envelope_percent/non_hex", 0, wave_envelope_percent(QStringLiteral("0A0A0A0A6464ZZ64")));
        CHECK_INT("wave_envelope_percent/non_hex_tail", 0, wave_envelope_percent(QStringLiteral("0A0A0A0A6464646G")));
    }

    void test_option_key_display() {
        OptionKey key;
        key.event_id = "gift";
        key.option_id = "rocket";
        CHECK_STRING("option_key_display/pair", "gift/rocket", option_key_display(key));
    }

    void test_detect_set_conflicts() {
        const char* name = "detect_set_conflicts/channel_a";
        std::vector<OptionBinding> bindings;
        bindings.push_back(make_binding("gift_a", "x", make_set_output("a", 100)));
        bindings.push_back(make_binding("gift_b", "x", make_set_output("B", 100)));
        bindings.push_back(make_binding("gift_c", "x", make_increase_output("A")));
        const std::vector<OptionBinding> conflicts = detect_set_conflicts("A", "AB", bindings);
        CHECK_INT(name, 1, static_cast<int>(conflicts.size()));
        CHECK_STRING(name, "gift_a/x", first_conflict(conflicts));
    }

    void test_resolve_keeps_newest() {
        const char* name = "resolve_set_conflicts/keep_newest";
        std::vector<OptionBinding> bindings;
        bindings.push_back(make_binding("gift_a", "x", make_set_output("A", 100)));
        bindings.push_back(make_binding("gift_b", "x", make_set_output("A", 200)));
        const std::vector<SetConflictLoss> disabled = resolve_set_conflicts("A", "AB", bindings);
        CHECK_INT(name, 1, static_cast<int>(disabled.size()));
        CHECK_STRING(name, "gift_a/x", first_disabled(disabled));
        CHECK_TRUE(name, !bindings[0].enabled);
        CHECK_TRUE(name, bindings[1].enabled);
    }

    void test_resolve_tie_event_id() {
        const char* name = "resolve_set_conflicts/tie_event_id";
        std::vector<OptionBinding> bindings;
        bindings.push_back(make_binding("gift_b", "x", make_set_output("A", 100)));
        bindings.push_back(make_binding("gift_a", "x", make_set_output("A", 100)));
        const std::vector<SetConflictLoss> disabled = resolve_set_conflicts("A", "AB", bindings);
        CHECK_INT(name, 1, static_cast<int>(disabled.size()));
        CHECK_STRING(name, "gift_b/x", first_disabled(disabled));
        CHECK_TRUE(name, bindings[1].enabled);
    }

    void test_resolve_tie_option_id() {
        const char* name = "resolve_set_conflicts/tie_option_id";
        std::vector<OptionBinding> bindings;
        bindings.push_back(make_binding("gift", "z", make_set_output("A", 100)));
        bindings.push_back(make_binding("gift", "a", make_set_output("A", 100)));
        const std::vector<SetConflictLoss> disabled = resolve_set_conflicts("A", "AB", bindings);
        CHECK_INT(name, 1, static_cast<int>(disabled.size()));
        CHECK_STRING(name, "gift/z", first_disabled(disabled));
        CHECK_TRUE(name, bindings[1].enabled);
    }

    void test_resolve_other_channel() {
        const char* name = "resolve_set_conflicts/other_channel";
        std::vector<OptionBinding> bindings;
        bindings.push_back(make_binding("gift_a", "x", make_set_output("A", 100)));
        bindings.push_back(make_binding("gift_b", "x", make_set_output("B", 200)));
        const std::vector<SetConflictLoss> disabled = resolve_set_conflicts("a", "AB", bindings);
        CHECK_INT(name, 0, static_cast<int>(disabled.size()));
        CHECK_TRUE(name, bindings[0].enabled);
        CHECK_TRUE(name, bindings[1].enabled);
    }

    void test_resolve_non_set() {
        const char* name = "resolve_set_conflicts/non_set";
        std::vector<OptionBinding> bindings;
        bindings.push_back(make_binding("gift_a", "x", make_set_output("A", 100)));
        bindings.push_back(make_binding("gift_b", "x", make_increase_output("A")));
        bindings[1].output.edited_at = 500;
        const std::vector<SetConflictLoss> disabled = resolve_set_conflicts("A", "AB", bindings);
        CHECK_INT(name, 0, static_cast<int>(disabled.size()));
        CHECK_TRUE(name, bindings[0].enabled);
        CHECK_TRUE(name, bindings[1].enabled);
    }

    void test_resolve_idempotent() {
        const char* name = "resolve_set_conflicts/idempotent";
        std::vector<OptionBinding> bindings;
        bindings.push_back(make_binding("gift_a", "x", make_set_output("A", 100)));
        bindings.push_back(make_binding("gift_b", "x", make_set_output("A", 200)));
        const std::vector<SetConflictLoss> first = resolve_set_conflicts("A", "AB", bindings);
        const std::vector<SetConflictLoss> second = resolve_set_conflicts("A", "AB", bindings);
        CHECK_INT(name, 1, static_cast<int>(first.size()));
        CHECK_INT(name, 0, static_cast<int>(second.size()));
        CHECK_TRUE(name, bindings[1].enabled);
    }

    void test_detect_follow_default() {
        const char* name = "detect_set_conflicts/follow_default";
        std::vector<OptionBinding> bindings;
        bindings.push_back(make_binding("gift_a", "x", make_set_output("", 100)));
        const std::vector<OptionBinding> on_a = detect_set_conflicts("A", "AB", bindings);
        const std::vector<OptionBinding> on_b = detect_set_conflicts("B", "ab", bindings);
        CHECK_INT(name, 1, static_cast<int>(on_a.size()));
        CHECK_STRING(name, "gift_a/x", first_conflict(on_a));
        CHECK_INT(name, 1, static_cast<int>(on_b.size()));
        CHECK_STRING(name, "gift_a/x", first_conflict(on_b));
        const std::vector<OptionBinding> default_a = detect_set_conflicts("A", "A", bindings);
        const std::vector<OptionBinding> default_a_on_b = detect_set_conflicts("B", "A", bindings);
        CHECK_INT(name, 1, static_cast<int>(default_a.size()));
        CHECK_INT(name, 0, static_cast<int>(default_a_on_b.size()));
        const std::vector<OptionBinding> invalid_channel = detect_set_conflicts("C", "AB", bindings);
        CHECK_INT(name, 0, static_cast<int>(invalid_channel.size()));
    }

    void test_resolve_follow_default_ab() {
        const char* name = "resolve_set_conflicts/follow_default_ab";
        std::vector<OptionBinding> on_a;
        on_a.push_back(make_binding("gift_a", "x", make_set_output("", 100)));
        on_a.push_back(make_binding("gift_b", "x", make_set_output("", 200)));
        const std::vector<SetConflictLoss> losses_a = resolve_set_conflicts("A", "AB", on_a);
        CHECK_INT(name, 1, static_cast<int>(losses_a.size()));
        CHECK_STRING(name, "gift_a/x", first_disabled(losses_a));
        CHECK_INT(name, 2, first_channel_count(losses_a));
        CHECK_TRUE(name, !on_a[0].enabled);
        CHECK_TRUE(name, on_a[1].enabled);

        std::vector<OptionBinding> on_b;
        on_b.push_back(make_binding("gift_a", "x", make_set_output("", 100)));
        on_b.push_back(make_binding("gift_b", "x", make_set_output("", 200)));
        const std::vector<SetConflictLoss> losses_b = resolve_set_conflicts("B", "AB", on_b);
        CHECK_INT(name, 1, static_cast<int>(losses_b.size()));
        CHECK_STRING(name, "gift_a/x", first_disabled(losses_b));
        CHECK_INT(name, 2, first_channel_count(losses_b));
        CHECK_TRUE(name, !on_b[0].enabled);
        CHECK_TRUE(name, on_b[1].enabled);
    }

    void test_resolve_follow_default_a() {
        const char* name = "resolve_set_conflicts/follow_default_a";
        std::vector<OptionBinding> on_b;
        on_b.push_back(make_binding("gift_a", "x", make_set_output("", 100)));
        on_b.push_back(make_binding("gift_b", "x", make_set_output("", 200)));
        const std::vector<SetConflictLoss> none_on_b = resolve_set_conflicts("B", "A", on_b);
        CHECK_INT(name, 0, static_cast<int>(none_on_b.size()));
        CHECK_TRUE(name, on_b[0].enabled);
        CHECK_TRUE(name, on_b[1].enabled);

        std::vector<OptionBinding> on_a;
        on_a.push_back(make_binding("gift_a", "x", make_set_output("", 100)));
        on_a.push_back(make_binding("gift_b", "x", make_set_output("", 200)));
        const std::vector<SetConflictLoss> losses = resolve_set_conflicts("A", "A", on_a);
        CHECK_INT(name, 1, static_cast<int>(losses.size()));
        CHECK_STRING(name, "gift_a/x", first_disabled(losses));
        CHECK_INT(name, 1, first_channel_count(losses));
        CHECK_TRUE(name, on_a[1].enabled);
    }

    void test_resolve_follow_default_tie() {
        const char* name = "resolve_set_conflicts/follow_default_tie";
        std::vector<OptionBinding> bindings;
        bindings.push_back(make_binding("gift_b", "x", make_set_output("", 100)));
        bindings.push_back(make_binding("gift_a", "x", make_set_output("", 100)));
        const std::vector<SetConflictLoss> losses = resolve_set_conflicts("A", "AB", bindings);
        CHECK_INT(name, 1, static_cast<int>(losses.size()));
        CHECK_STRING(name, "gift_b/x", first_disabled(losses));
        CHECK_TRUE(name, bindings[1].enabled);
    }

    void test_resolve_explicit_vs_follow_default() {
        const char* name = "resolve_set_conflicts/explicit_vs_follow";
        std::vector<OptionBinding> on_a;
        on_a.push_back(make_binding("gift_a", "x", make_set_output("A", 100)));
        on_a.push_back(make_binding("gift_b", "x", make_set_output("", 200)));
        const std::vector<SetConflictLoss> losses_a = resolve_set_conflicts("A", "AB", on_a);
        CHECK_INT(name, 1, static_cast<int>(losses_a.size()));
        CHECK_STRING(name, "gift_a/x", first_disabled(losses_a));
        CHECK_INT(name, 1, first_channel_count(losses_a));
        CHECK_TRUE(name, on_a[1].enabled);

        std::vector<OptionBinding> on_b;
        on_b.push_back(make_binding("gift_a", "x", make_set_output("A", 100)));
        on_b.push_back(make_binding("gift_b", "x", make_set_output("", 200)));
        const std::vector<SetConflictLoss> losses_b = resolve_set_conflicts("B", "AB", on_b);
        CHECK_INT(name, 0, static_cast<int>(losses_b.size()));
        CHECK_TRUE(name, on_b[0].enabled);
        CHECK_TRUE(name, on_b[1].enabled);
    }

} // namespace

int main() {
    test_clamp_strength();
    test_apply_merge();
    test_normalize_value();
    test_wave_envelope_percent();
    test_option_key_display();
    test_detect_set_conflicts();
    test_resolve_keeps_newest();
    test_resolve_tie_event_id();
    test_resolve_tie_option_id();
    test_resolve_other_channel();
    test_resolve_non_set();
    test_resolve_idempotent();
    test_detect_follow_default();
    test_resolve_follow_default_ab();
    test_resolve_follow_default_a();
    test_resolve_follow_default_tie();
    test_resolve_explicit_vs_follow_default();
    if (failures != 0) {
        std::printf("event_engine_test: %d assertion(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    std::printf("event_engine_test: all assertions passed\n");
    return EXIT_SUCCESS;
}
