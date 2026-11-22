// Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
// SPDX-License-Identifier: GPL-3.0-only

#include "PythonRunner.h"

#include <QByteArray>
#include <QChar>
#include <QProcess>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <cstddef>
#include <cstring>

namespace {

// 调用方传 0 或负值时的默认超时
constexpr int kDefaultTimeoutMs = 30000;
// 解释器启动自身的等待上限
constexpr int kStartTimeoutMs = 5000;
// kill 之后回收进程的等待上限
constexpr int kKillTimeoutMs = 2000;

// 参数串切分：空白分隔，双引号包住的部分整体作为一个参数
QStringList split_arguments(const QString& text) {
    QStringList arguments;
    QString current;
    bool in_quote = false;
    bool has_token = false;
    for (const QChar ch : text) {
        if (ch == QLatin1Char('"')) {
            in_quote = !in_quote;
            has_token = true;
            continue;
        }
        if (!in_quote && ch.isSpace()) {
            if (has_token) {
                arguments.append(current);
                current.clear();
                has_token = false;
            }
            continue;
        }
        current.append(ch);
        has_token = true;
    }
    if (has_token) arguments.append(current);
    return arguments;
}

// 按容量写缓冲区：始终 NUL 结尾，容量为 0 或指针为空时不写任何字节
void write_buffer(char* buffer, std::size_t capacity, const QByteArray& data, bool* truncated) {
    if (truncated != nullptr) *truncated = false;
    const std::size_t size = static_cast<std::size_t>(data.size());
    if (buffer == nullptr || capacity == 0) {
        if (truncated != nullptr) *truncated = size > 0;
        return;
    }
    const std::size_t copied = std::min(capacity - 1, size);
    if (copied > 0) std::memcpy(buffer, data.constData(), copied);
    buffer[copied] = '\0';
    if (truncated != nullptr) *truncated = size > copied;
}

// 容量大于 0 时缓冲必须非空，否则无处可写
bool buffer_ok(const char* buffer, std::size_t capacity) {
    return capacity == 0 || buffer != nullptr;
}

void write_message(char* buffer, std::size_t capacity, const QByteArray& message) {
    write_buffer(buffer, capacity, message, nullptr);
}

} // namespace

int dglab_run_python(const char* interpreter, const char* script_path, const char* args, int timeout_ms,
                     char* out_stdout, std::size_t out_stdout_cap, char* out_stderr,
                     std::size_t out_stderr_cap) {
    if (out_stdout != nullptr && out_stdout_cap > 0) out_stdout[0] = '\0';
    if (out_stderr != nullptr && out_stderr_cap > 0) out_stderr[0] = '\0';
    if (!buffer_ok(out_stdout, out_stdout_cap) || !buffer_ok(out_stderr, out_stderr_cap)) {
        return DGLAB_PYTHON_ERROR_ARGUMENT;
    }
    if (script_path == nullptr || script_path[0] == '\0') {
        write_message(out_stderr, out_stderr_cap,
                      QByteArrayLiteral("dglab_run_python: script_path 为空"));
        return DGLAB_PYTHON_ERROR_ARGUMENT;
    }

    try {
        const QString program = (interpreter != nullptr && interpreter[0] != '\0')
            ? QString::fromUtf8(interpreter)
            : QStringLiteral("python");
        QStringList arguments =
            split_arguments(args != nullptr ? QString::fromUtf8(args) : QString());
        arguments.prepend(QString::fromUtf8(script_path));

        QProcess process;
        process.setProgram(program);
        process.setArguments(arguments);
        process.setProcessChannelMode(QProcess::SeparateChannels);
        process.start();
        if (!process.waitForStarted(kStartTimeoutMs)) {
            const QByteArray reason = (QStringLiteral("dglab_run_python: 无法启动解释器 ") + program +
                                       QStringLiteral("：") + process.errorString())
                                          .toUtf8();
            write_message(out_stderr, out_stderr_cap, reason);
            process.kill();
            process.waitForFinished(kKillTimeoutMs);
            return DGLAB_PYTHON_ERROR_START;
        }

        const int effective_timeout = timeout_ms > 0 ? timeout_ms : kDefaultTimeoutMs;
        if (!process.waitForFinished(effective_timeout)) {
            // 超时：kill 之后再等一次回收，避免留下僵尸进程
            process.kill();
            process.waitForFinished(kKillTimeoutMs);
            const QByteArray partial_out = process.readAllStandardOutput();
            const QByteArray partial_err = process.readAllStandardError();
            QByteArray note = QByteArrayLiteral("dglab_run_python: 超时 ");
            note += QByteArray::number(effective_timeout);
            note += QByteArrayLiteral(" ms，进程已终止");
            if (!partial_err.isEmpty()) {
                note += QByteArrayLiteral("\n");
                note += partial_err;
            }
            write_buffer(out_stdout, out_stdout_cap, partial_out, nullptr);
            write_message(out_stderr, out_stderr_cap, note);
            return DGLAB_PYTHON_ERROR_TIMEOUT;
        }

        const QByteArray standard_out = process.readAllStandardOutput();
        const QByteArray standard_err = process.readAllStandardError();
        bool stdout_truncated = false;
        bool stderr_truncated = false;
        write_buffer(out_stdout, out_stdout_cap, standard_out, &stdout_truncated);
        write_buffer(out_stderr, out_stderr_cap, standard_err, &stderr_truncated);
        if (process.exitStatus() == QProcess::CrashExit) return DGLAB_PYTHON_ERROR_CRASH;
        if (process.exitCode() != 0) return process.exitCode();
        if (stdout_truncated || stderr_truncated) return DGLAB_PYTHON_ERROR_TRUNCATED;
        return DGLAB_PYTHON_OK;
    } catch (...) {
        write_message(out_stderr, out_stderr_cap,
                      QByteArrayLiteral("dglab_run_python: SDK 内部错误"));
        return DGLAB_PYTHON_ERROR_INTERNAL;
    }
}
