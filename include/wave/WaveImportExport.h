/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "Wave.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <vector>

// 波形导入导出：JSON（单个波形对象或波形数组）、.pulse（单波形纯文本）与 zip（多波形归档）
// 三套格式的解析与生成。本模块只做文本 / 字节与数据结构之间的转换，不读写文件、不判定名称冲突；
// 文件读写与冲突策略落在 WaveLibrary（plan_import / apply_import / export_json / export_pulse / export_zip）。
namespace wave_io {

    /// @brief zip 归档内的一个文件条目（条目名 + 解压后的内容）
    struct ZipEntry {
        QString name;    ///< 归档内条目名（可含目录段，原样保留）
        QByteArray data; ///< 解压后的内容
    };

    /// @brief 单个条目的解压后上限（4 MiB）：超过即拒绝整个归档
    constexpr qint64 ZIP_MAX_ENTRY_BYTES = 4 * 1024 * 1024;
    /// @brief 归档内的条目数上限：超过即拒绝整个归档
    constexpr int ZIP_MAX_ENTRIES = 512;
    /// @brief 归档解压后的总字节上限（32 MiB）：超过即拒绝整个归档
    constexpr qint64 ZIP_MAX_TOTAL_BYTES = 32 * 1024 * 1024;

    /// @brief 导出 JSON 的数组文本；缺某一版本帧时写空数组并把说明追加到 notes
    /// @param waves 待导出的波形
    /// @param notes 逐条补充说明（缺 V2 / V3 帧）
    /// @return JSON 数组文本（4 空格缩进）
    QString build_json(const std::vector<Wave>& waves, QStringList& notes);

    /// @brief 解析导入用的 JSON 文本（接受单个对象或数组）
    /// @param text JSON 文本
    /// @param waves 解析成功的波形（来源一律按用户波形）
    /// @param skipped 逐条跳过原因
    /// @param error 致命错误（文本不是合法 JSON、顶层既非对象也非数组、没有任何条目）
    /// @return 是否解析成功；单条失败不影响其它条目
    bool parse_json(const QString& text, std::vector<Wave>& waves, QStringList& skipped, QString& error);

    /// @brief 解析 zip 归档字节（store 与 deflate 均支持）：拒绝路径穿越与超限条目
    /// @param archive 归档字节
    /// @param entries 解析成功的条目（含非 .pulse 条目，由调用方按后缀筛选）
    /// @param skipped 逐条跳过原因（路径穿越、压缩方式不支持、条目数据损坏）
    /// @param error 致命错误（不是 zip、条目数或解压大小超过上限）
    /// @return 是否解析成功；单条目损坏不影响其它条目
    bool parse_zip(const QByteArray& archive, std::vector<ZipEntry>& entries, QStringList& skipped,
        QString& error);

    /// @brief 校验 .pulse 文本并以给定名称生成波形
    /// @param text 文件内容
    /// @param name 波形名称（取自文件名，可被冲突弹窗改写）
    /// @param wave 生成的波形（raw 为内容原文，帧为空）
    /// @param error 失败原因
    /// @return 是否为有效的 .pulse 文本
    bool parse_pulse(const QString& text, const QString& name, Wave& wave, QString& error);

    /// @brief 生成单个波形的 .pulse 文本（raw 原文，不改写、不换行包裹）
    /// @param wave 待导出的波形
    /// @param text 输出的文本内容
    /// @param error 失败原因（raw 为空时拒绝导出）
    /// @return 是否生成成功
    bool build_pulse(const Wave& wave, QString& text, QString& error);

    /// @brief 生成 zip 归档字节（每个条目独立 deflate 压缩，文件名按 UTF-8 写入）
    /// @param entries 待写入条目
    /// @param out 归档字节
    /// @param error 失败原因（没有条目、写入失败）
    /// @return 是否生成成功
    bool build_zip(const std::vector<ZipEntry>& entries, QByteArray& out, QString& error);

} // namespace wave_io
