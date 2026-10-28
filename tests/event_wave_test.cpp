/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "EventMath.h"

#include <QStringList>

#include <cstdio>
#include <cstdlib>

#define CHECK_INT(case_name, expected, actual) check_int((case_name), (expected), (actual), #actual, __LINE__)
#define CHECK_TEXT(case_name, expected, actual) check_text((case_name), (expected), (actual), #actual, __LINE__)
#define CHECK_TRUE(case_name, expr) check_true((case_name), (expr), #expr, __LINE__)
#define CHECK_ACTION(case_name, expected, actual) \
    CHECK_TEXT((case_name), action_name(expected), action_name(actual))

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

    // 文本断言
    void check_text(const char* case_name, const QString& expected, const QString& actual, const char* expr,
        int line) {
        if (expected == actual) {
            return;
        }
        char detail[512] = {0};
        std::snprintf(detail, sizeof(detail), "%s expected %s actual %s", expr, expected.toUtf8().constData(),
            actual.toUtf8().constData());
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

    // 动作名（打印与断言共用）
    const char* action_name(EventWaveAction action) {
        switch (action) {
        case EventWaveAction::NONE: return "NONE";
        case EventWaveAction::START: return "START";
        case EventWaveAction::REPLACE: return "REPLACE";
        case EventWaveAction::RESTORE: return "RESTORE";
        case EventWaveAction::REFUSE: return "REFUSE";
        case EventWaveAction::ABORT: return "ABORT";
        }
        return "UNKNOWN";
    }

    // 打印一条状态机推进记录（与 DeviceController 的 INFO 日志同形）
    void print_step(const char* case_name, int frame, EventWaveAction action, const EventWaveState& state) {
        std::printf("[STEP] %s 帧 %d 动作 %s → 播放中 %d 波形《%s》 剩余 %d 帧 接管前波形 %d\n", case_name, frame,
            action_name(action), state.active ? 1 : 0, state.wave_name.toUtf8().constData(),
            state.remaining_frames, state.prior_active ? 1 : 0);
    }

    // 生成 count 帧占位帧（帧内容不参与状态机判断）
    QStringList sample_frames(int count) {
        QStringList frames;
        for (int index = 0; index < count; ++index) {
            frames.append(QStringLiteral("0A0A0A0A0000000%1").arg(index));
        }
        return frames;
    }

    // 推进一帧并打印
    EventWaveAction step(const char* case_name, int frame, EventWaveState& state, const QString& wave_name,
        int frame_count, bool output_ready, const QStringList& prior_frames, int prior_seconds,
        int frames_elapsed = 1) {
        const EventWaveAction action = advance_event_wave(state, wave_name, frame_count, output_ready, prior_frames,
            prior_seconds, frames_elapsed);
        print_step(case_name, frame, action, state);
        return action;
    }

    // 首次触发从空闲态启动播放
    void test_start_from_idle() {
        const char* name = "wave/start_from_idle";
        EventWaveState state;
        const EventWaveAction action = step(name, 0, state, QStringLiteral("呼吸"), 12, true, QStringList(), 0);
        CHECK_ACTION(name, EventWaveAction::START, action);
        CHECK_TRUE(name, state.active);
        CHECK_TEXT(name, QStringLiteral("呼吸"), state.wave_name);
        CHECK_INT(name, 12, state.remaining_frames);
        CHECK_TRUE(name, !state.prior_active);
    }

    // 连续多帧触发同一波形不重启，只把剩余帧数续满
    void test_dedupe_same_wave() {
        const char* name = "wave/dedupe_same_wave";
        EventWaveState state;
        int starts = 0;
        int replaces = 0;
        for (int frame = 0; frame < 6; ++frame) {
            // 第 0 帧与第 3 帧各触发一次，其余帧无事件波形
            const bool triggered = (frame == 0 || frame == 3);
            const QString wave_name = triggered ? QStringLiteral("呼吸") : QString();
            const int frame_count = triggered ? 12 : 0;
            const EventWaveAction action = step(name, frame, state, wave_name, frame_count, true, QStringList(), 0);
            if (action == EventWaveAction::START) {
                ++starts;
            }
            if (action == EventWaveAction::REPLACE) {
                ++replaces;
            }
        }
        CHECK_INT(name, 1, starts);
        CHECK_INT(name, 0, replaces);
        CHECK_TRUE(name, state.active);
        CHECK_TEXT(name, QStringLiteral("呼吸"), state.wave_name);
    }

    // 播完回到空闲并交还接管前的波形
    void test_play_to_end_restore() {
        const char* name = "wave/play_to_end_restore";
        EventWaveState state;
        const QStringList prior = sample_frames(3);
        CHECK_ACTION(name, EventWaveAction::START,
            step(name, 0, state, QStringLiteral("潮汐"), 5, true, prior, 4));
        CHECK_TRUE(name, state.prior_active);
        CHECK_INT(name, 3, state.prior_frames.size());
        CHECK_INT(name, 4, state.prior_seconds);
        for (int frame = 1; frame < 5; ++frame) {
            CHECK_ACTION(name, EventWaveAction::NONE, step(name, frame, state, QString(), 0, true, prior, 4));
        }
        // 第 5 帧播完最后一帧，本帧收口
        CHECK_ACTION(name, EventWaveAction::RESTORE, step(name, 5, state, QString(), 0, true, prior, 4));
        CHECK_TRUE(name, !state.active);
        CHECK_INT(name, 0, state.remaining_frames);
        CHECK_TRUE(name, state.prior_active);
        CHECK_INT(name, 3, state.prior_frames.size());
        CHECK_INT(name, 4, state.prior_seconds);
        // 收口后的下一帧清掉接管前信息
        CHECK_ACTION(name, EventWaveAction::NONE, step(name, 6, state, QString(), 0, true, prior, 4));
        CHECK_TRUE(name, !state.prior_active);
    }

    // 被另一个事件波形替换：接管前波形不变，播放从头开始
    void test_replace_by_new_wave() {
        const char* name = "wave/replace_by_new_wave";
        EventWaveState state;
        const QStringList prior = sample_frames(2);
        CHECK_ACTION(name, EventWaveAction::START,
            step(name, 0, state, QStringLiteral("呼吸"), 12, true, prior, 3));
        CHECK_ACTION(name, EventWaveAction::NONE, step(name, 1, state, QString(), 0, true, prior, 3));
        CHECK_ACTION(name, EventWaveAction::REPLACE,
            step(name, 2, state, QStringLiteral("潮汐"), 23, true, prior, 3));
        CHECK_TEXT(name, QStringLiteral("潮汐"), state.wave_name);
        CHECK_INT(name, 23, state.remaining_frames);
        CHECK_TRUE(name, state.prior_active);
        CHECK_INT(name, 2, state.prior_frames.size());
    }

    // 缺帧拒绝：空闲态不发也不改状态；播放中保持原波形
    void test_refuse_missing_frames() {
        const char* name = "wave/refuse_missing_frames";
        EventWaveState state;
        CHECK_ACTION(name, EventWaveAction::REFUSE,
            step(name, 0, state, QStringLiteral("呼吸"), 0, true, QStringList(), 0));
        CHECK_TRUE(name, !state.active);
        CHECK_TEXT(name, QString(), state.wave_name);
        CHECK_ACTION(name, EventWaveAction::START,
            step(name, 1, state, QStringLiteral("呼吸"), 12, true, QStringList(), 0));
        CHECK_ACTION(name, EventWaveAction::NONE, step(name, 2, state, QString(), 0, true, QStringList(), 0));
        CHECK_ACTION(name, EventWaveAction::REFUSE,
            step(name, 3, state, QStringLiteral("潮汐"), 0, true, QStringList(), 0));
        CHECK_TEXT(name, QStringLiteral("呼吸"), state.wave_name);
        CHECK_INT(name, 11, state.remaining_frames);
    }

    // 通道关闭 / 设备断开：中止并清空，不恢复接管前波形
    void test_abort_on_channel_closed() {
        const char* name = "wave/abort_on_channel_closed";
        EventWaveState state;
        const QStringList prior = sample_frames(2);
        CHECK_ACTION(name, EventWaveAction::NONE,
            step(name, 0, state, QStringLiteral("呼吸"), 12, false, QStringList(), 0));
        CHECK_TRUE(name, !state.active);
        CHECK_ACTION(name, EventWaveAction::START,
            step(name, 1, state, QStringLiteral("呼吸"), 12, true, prior, 2));
        CHECK_ACTION(name, EventWaveAction::ABORT, step(name, 2, state, QString(), 0, false, prior, 2));
        CHECK_TRUE(name, !state.active);
        CHECK_TEXT(name, QString(), state.wave_name);
        CHECK_INT(name, 0, state.remaining_frames);
        CHECK_TRUE(name, !state.prior_active);
        CHECK_INT(name, 0, state.prior_frames.size());
    }

    // 无接管前波形时播完只收口、不恢复
    void test_restore_without_prior() {
        const char* name = "wave/restore_without_prior";
        EventWaveState state;
        CHECK_ACTION(name, EventWaveAction::START,
            step(name, 0, state, QStringLiteral("呼吸"), 2, true, QStringList(), 0));
        CHECK_TRUE(name, !state.prior_active);
        CHECK_ACTION(name, EventWaveAction::NONE, step(name, 1, state, QString(), 0, true, QStringList(), 0));
        CHECK_ACTION(name, EventWaveAction::RESTORE, step(name, 2, state, QString(), 0, true, QStringList(), 0));
        CHECK_TRUE(name, !state.active);
        CHECK_TRUE(name, !state.prior_active);
    }


    // 设备没写下一帧时播放进度不推进，一次写入跨两帧时按两帧推进
    void test_device_writes_drive_progress() {
        const char* name = "wave/device_writes_drive_progress";
        EventWaveState state;
        CHECK_ACTION(name, EventWaveAction::START,
            step(name, 0, state, QStringLiteral("呼吸"), 4, true, QStringList(), 0, 0));
        // 设备尚未写入：剩余帧数不变
        CHECK_ACTION(name, EventWaveAction::NONE,
            step(name, 1, state, QString(), 0, true, QStringList(), 0, 0));
        CHECK_INT(name, 4, state.remaining_frames);
        CHECK_ACTION(name, EventWaveAction::NONE,
            step(name, 2, state, QString(), 0, true, QStringList(), 0, 1));
        CHECK_INT(name, 3, state.remaining_frames);
        // 两次写在两帧之间到达：一次推进两帧
        CHECK_ACTION(name, EventWaveAction::NONE,
            step(name, 3, state, QString(), 0, true, QStringList(), 0, 2));
        CHECK_INT(name, 1, state.remaining_frames);
        CHECK_ACTION(name, EventWaveAction::RESTORE,
            step(name, 4, state, QString(), 0, true, QStringList(), 0, 2));
        CHECK_TRUE(name, !state.active);
    }

    // 写入帧数多于剩余帧数时直接收口，不出现负剩余
    void test_writes_overflow_remaining() {
        const char* name = "wave/writes_overflow_remaining";
        EventWaveState state;
        CHECK_ACTION(name, EventWaveAction::START,
            step(name, 0, state, QStringLiteral("呼吸"), 2, true, QStringList(), 0));
        CHECK_ACTION(name, EventWaveAction::RESTORE,
            step(name, 1, state, QString(), 0, true, QStringList(), 0, 5));
        CHECK_TRUE(name, !state.active);
        CHECK_INT(name, 0, state.remaining_frames);
    }
} // namespace

int main() {
    test_start_from_idle();
    test_dedupe_same_wave();
    test_play_to_end_restore();
    test_replace_by_new_wave();
    test_refuse_missing_frames();
    test_abort_on_channel_closed();
    test_restore_without_prior();
    test_device_writes_drive_progress();
    test_writes_overflow_remaining();
    if (failures != 0) {
        std::printf("event_wave_test: %d assertion(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    std::printf("event_wave_test: all assertions passed\n");
    return EXIT_SUCCESS;
}
