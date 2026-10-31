/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "OpossumFrames.h"

#include <QByteArray>
#include <QString>

#include <cstdio>

#define CHECK_INT(case_name, expected, actual) check_int((case_name), (expected), (actual), #actual, __LINE__)
#define CHECK_TEXT(case_name, expected, actual) check_text((case_name), (expected), (actual), #actual, __LINE__)

namespace {

    int failures = 0; ///< 失败断言数

    // 记录一次断言失败
    void fail(const char* case_name, const char* detail, int line) {
        std::fprintf(stderr, "FAIL %s (line %d): %s\n", case_name, line, detail);
        ++failures;
    }

    // 整型断言
    void check_int(const char* case_name, long long expected, long long actual, const char* expr, int line) {
        if (expected == actual) {
            return;
        }
        char detail[256] = {0};
        std::snprintf(detail, sizeof(detail), "%s expected %lld actual %lld", expr, expected, actual);
        fail(case_name, detail, line);
    }

    // 字节串断言（按 HEX 文本比较）
    void check_text(const char* case_name, const QString& expected, const QString& actual, const char* expr, int line) {
        if (expected == actual) {
            return;
        }
        char detail[512] = {0};
        std::snprintf(detail, sizeof(detail), "%s expected %s actual %s", expr, expected.toUtf8().constData(),
            actual.toUtf8().constData());
        fail(case_name, detail, line);
    }

    // B0 指令布局：头 + 7 字节 0 + A 4 字节 + 4 字节 0 + B 4 字节
    void test_b0_layout() {
        const QByteArray frame = opossum_frames::build_b0(QByteArray::fromHex("01020364"), QByteArray::fromHex("05060708"));
        CHECK_INT("b0 长度", 20, frame.size());
        CHECK_TEXT("b0 全帧", QStringLiteral("b000000000000000010203640000000005060708"), QString::fromLatin1(frame.toHex()));
        CHECK_INT("b0 头", 0xB0, static_cast<quint8>(frame.at(0)));
        CHECK_TEXT("b0 A 通道", QStringLiteral("01020364"), QString::fromLatin1(frame.mid(8, 4).toHex()));
        CHECK_TEXT("b0 中段占位", QStringLiteral("00000000"), QString::fromLatin1(frame.mid(12, 4).toHex()));
        CHECK_TEXT("b0 B 通道", QStringLiteral("05060708"), QString::fromLatin1(frame.mid(16, 4).toHex()));
    }

    // B0 通道越界：任一字节超出 0~100 则整条该通道弃用为 0，另一通道不受影响
    void test_b0_out_of_range() {
        const QByteArray a_bad = opossum_frames::build_b0(QByteArray::fromHex("05650164"), QByteArray::fromHex("0A0B0C0D"));
        CHECK_TEXT("b0 A 越界弃用", QStringLiteral("00000000"), QString::fromLatin1(a_bad.mid(8, 4).toHex()));
        CHECK_TEXT("b0 A 越界时 B 保留", QStringLiteral("0a0b0c0d"), QString::fromLatin1(a_bad.mid(16, 4).toHex()));

        const QByteArray b_bad = opossum_frames::build_b0(QByteArray::fromHex("0A0B0C0D"), QByteArray::fromHex("0A0B0CC9"));
        CHECK_TEXT("b0 B 越界弃用", QStringLiteral("00000000"), QString::fromLatin1(b_bad.mid(16, 4).toHex()));
        CHECK_TEXT("b0 B 越界时 A 保留", QStringLiteral("0a0b0c0d"), QString::fromLatin1(b_bad.mid(8, 4).toHex()));

        const QByteArray both_bad = opossum_frames::build_b0(QByteArray::fromHex("FFFFFFFF"), QByteArray::fromHex("FF"));
        CHECK_TEXT("b0 双通道弃用 A", QStringLiteral("00000000"), QString::fromLatin1(both_bad.mid(8, 4).toHex()));
        CHECK_TEXT("b0 双通道弃用 B", QStringLiteral("00000000"), QString::fromLatin1(both_bad.mid(16, 4).toHex()));
        CHECK_INT("b0 弃用后长度", 20, both_bad.size());
    }

    // 郊狼 V3 八字节帧转负鼠四字节强度：取第 4~7 字节，频率丢弃
    void test_v3_frame_conversion() {
        const QByteArray bytes = opossum_frames::wave_bytes_from_v3_frame(QStringLiteral("0A0A0A0A050A0F64"));
        CHECK_TEXT("v3 帧取强度", QStringLiteral("050a0f64"), QString::fromLatin1(bytes.toHex()));

        CHECK_INT("v3 帧过短", 0, opossum_frames::wave_bytes_from_v3_frame(QStringLiteral("0A0A0A0A")).size());
        CHECK_INT("v3 帧长度不足一位", 0, opossum_frames::wave_bytes_from_v3_frame(QStringLiteral("0A0A0A0A050A0F")).size());
        CHECK_INT("v3 帧过长", 0, opossum_frames::wave_bytes_from_v3_frame(QStringLiteral("0A0A0A0A050A0F6464")).size());
        CHECK_INT("v3 帧非 HEX", 0, opossum_frames::wave_bytes_from_v3_frame(QStringLiteral("0A0A0A0A050A0FZZ")).size());
        CHECK_INT("v3 帧空串", 0, opossum_frames::wave_bytes_from_v3_frame(QString()).size());
        CHECK_TEXT("v3 帧小写 HEX", QStringLiteral("050a0f64"),
            QString::fromLatin1(opossum_frames::wave_bytes_from_v3_frame(QStringLiteral("0a0a0a0a050a0f64")).toHex()));
    }

    // B3 指令：0xFF 表示该通道不改
    void test_b3_no_change() {
        CHECK_TEXT("b3 只改 A", QStringLiteral("b3a0ff"), QString::fromLatin1(opossum_frames::build_b3(160, -1).toHex()));
        CHECK_TEXT("b3 只改 B", QStringLiteral("b3ffc8"), QString::fromLatin1(opossum_frames::build_b3(-1, 200).toHex()));
        CHECK_TEXT("b3 双通道", QStringLiteral("b30a14"), QString::fromLatin1(opossum_frames::build_b3(10, 20).toHex()));
        CHECK_TEXT("b3 越界钳制", QStringLiteral("b3c800"), QString::fromLatin1(opossum_frames::build_b3(250, 0).toHex()));
        CHECK_INT("b3 长度", 3, opossum_frames::build_b3(0, 0).size());
        CHECK_INT("b3 不改字节", 0xFF, static_cast<quint8>(opossum_frames::build_b3(-1, -1).at(2)));
    }

    // B2 指令：24 字节完整形态与 20 字节截断形态
    void test_b2_forms() {
        const QByteArray full = opossum_frames::build_b2(160, 200);
        CHECK_INT("b2 完整长度", 24, full.size());
        CHECK_INT("b2 头", 0xB2, static_cast<quint8>(full.at(0)));
        CHECK_TEXT("b2 固定段", QStringLiteral("ffff00ffffffffffffffffffffffffffffffff"), QString::fromLatin1(full.mid(1, 19).toHex()));
        CHECK_TEXT("b2 固定段末尾", QStringLiteral("ff0809"), QString::fromLatin1(full.mid(19, 3).toHex()));
        CHECK_INT("b2 A 通道", 160, static_cast<quint8>(full.at(22)));
        CHECK_INT("b2 B 通道", 200, static_cast<quint8>(full.at(23)));

        const QByteArray short_form = opossum_frames::build_b2_short(160, 200);
        CHECK_INT("b2 截断长度", 20, short_form.size());
        CHECK_INT("b2 截断头", 0xB2, static_cast<quint8>(short_form.at(0)));
        CHECK_TEXT("b2 截断为完整形态前缀", QString::fromLatin1(full.left(20).toHex()), QString::fromLatin1(short_form.toHex()));
        CHECK_INT("b2 越界钳制", 200, static_cast<quint8>(opossum_frames::build_b2(255, 0).at(22)));
    }

    // 转换 + 构造串联：波形强度超出 0~100 的帧在 B0 里整通道静音
    void test_pipeline_silence() {
        const QByteArray loud = opossum_frames::wave_bytes_from_v3_frame(QStringLiteral("0A0A0A0A65646464"));
        const QByteArray silent = opossum_frames::wave_bytes_from_v3_frame(QStringLiteral("0A0A0A0A00000000"));
        const QByteArray frame = opossum_frames::build_b0(loud, silent);
        CHECK_TEXT("pipeline A 越界弃用", QStringLiteral("00000000"), QString::fromLatin1(frame.mid(8, 4).toHex()));
        CHECK_TEXT("pipeline B 静音", QStringLiteral("00000000"), QString::fromLatin1(frame.mid(16, 4).toHex()));
        const QByteArray ok = opossum_frames::build_b0(opossum_frames::wave_bytes_from_v3_frame(QStringLiteral("0A0A0A0A6423320A")),
            QByteArray());
        CHECK_TEXT("pipeline A 正常", QStringLiteral("6423320a"), QString::fromLatin1(ok.mid(8, 4).toHex()));
    }

    // 全部用例入口：返回失败断言数
    int run_all() {
        test_b0_layout();
        test_b0_out_of_range();
        test_v3_frame_conversion();
        test_b3_no_change();
        test_b2_forms();
        test_pipeline_silence();
        return failures;
    }

} // namespace

int main() {
    const int failed = run_all();
    if (failed == 0) {
        std::printf("opossum_frames_test: all cases passed\n");
        return 0;
    }
    std::fprintf(stderr, "opossum_frames_test: %d assertion(s) failed\n", failed);
    return 1;
}
