// Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <cstddef>

// 插件 SDK 导出宏：与 PathFinder.h 同一份守卫定义
#ifndef DGLAB_SDK_EXPORT
#if defined(_WIN32)
#if defined(DGLAB_SDK_BUILD)
#define DGLAB_SDK_EXPORT __declspec(dllexport)
#else
#define DGLAB_SDK_EXPORT __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define DGLAB_SDK_EXPORT __attribute__((visibility("default")))
#else
#define DGLAB_SDK_EXPORT
#endif
#endif

// dglab_run_python 的返回码：0 成功，子进程非 0 退出时直接返回该退出码，SDK 自身的失败用负数
#define DGLAB_PYTHON_OK 0
// 参数无效（脚本路径为空、容量大于 0 但缓冲为空）
#define DGLAB_PYTHON_ERROR_ARGUMENT (-1)
// 输出超出调用方缓冲，已按容量截断写入
#define DGLAB_PYTHON_ERROR_TRUNCATED (-2)
// 超时，进程已被 kill 并回收
#define DGLAB_PYTHON_ERROR_TIMEOUT (-3)
// 解释器启动失败
#define DGLAB_PYTHON_ERROR_START (-4)
// 子进程异常退出（崩溃）
#define DGLAB_PYTHON_ERROR_CRASH (-5)
// SDK 内部错误（已捕获异常，不会穿过 ABI）
#define DGLAB_PYTHON_ERROR_INTERNAL (-6)

extern "C" {

/// @brief 用调用方提供的解释器运行一个 Python 脚本并回收它的输出
/// @param interpreter 解释器路径（UTF-8），为空时回退到 python
/// @param script_path 脚本路径（UTF-8），必须非空
/// @param args 传给脚本的参数串（UTF-8，空白分隔，双引号包住的部分保持整体），可为空
/// @param timeout_ms 超时毫秒数，小于等于 0 时取 SDK 默认值
/// @param out_stdout stdout 缓冲，容量为 0 时可传空指针
/// @param out_stdout_cap stdout 缓冲容量（含结尾 NUL 的位置）
/// @param out_stderr stderr 缓冲，容量为 0 时可传空指针
/// @param out_stderr_cap stderr 缓冲容量（含结尾 NUL 的位置）
/// @return 0 成功 / 正数子进程退出码 / 负数见 DGLAB_PYTHON_ERROR_* 宏
DGLAB_SDK_EXPORT int dglab_run_python(const char* interpreter, const char* script_path, const char* args,
                                      int timeout_ms, char* out_stdout, std::size_t out_stdout_cap,
                                      char* out_stderr, std::size_t out_stderr_cap);

} // extern "C"
