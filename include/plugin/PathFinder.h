// Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

// 插件 SDK 导出宏：Windows 区分导出与导入，其余平台用默认可见性
// 两个公开头各自守卫定义一份，调用方的包含顺序不影响结果
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

// SDK 的 ABI 版本：函数签名或结果结构变化时递增
#define DGLAB_SDK_VERSION 1

// dglab_find_game_dir 的返回码：正数命中，0 未命中，负数表示失败
#define DGLAB_FIND_FOUND 1
#define DGLAB_FIND_NOT_FOUND 0
// 参数无效（空指针、空标识）
#define DGLAB_SDK_ERROR_ARGUMENT (-1)
// 路径超出结果结构容量，已按容量截断写入
#define DGLAB_SDK_ERROR_TRUNCATED (-2)
// game_id 不在 SDK 支持的标识表内
#define DGLAB_SDK_ERROR_UNKNOWN_GAME (-3)
// SDK 内部错误（已捕获异常，不会穿过 ABI）
#define DGLAB_SDK_ERROR_INTERNAL (-4)

// DglabFindResult - 游戏目录查找结果
// 调用方分配结构，SDK 只写这两个定长缓冲，缓冲永远是 UTF-8 且 NUL 结尾
typedef struct DglabFindResult {
    // 1 命中 / 0 未命中
    int found;
    // 命中的绝对路径，容量不足时截断
    char path[1024];
    // 命中来源：manual / registry / steam-vdf / known-path
    char source[64];
} DglabFindResult;

extern "C" {

/// @brief 返回 SDK 的 ABI 版本
/// @return 版本号，等于编译期的 DGLAB_SDK_VERSION
DGLAB_SDK_EXPORT int dglab_sdk_version(void);

/// @brief 按游戏标识查找游戏安装目录，按内建优先级返回第一个命中
/// @param game_id 游戏标识（UTF-8），取值见 SDK 支持的标识表
/// @param out 调用方分配的查找结果，命中时填写 path 与 source
/// @return 1 命中 / 0 未命中 / 负数见 DGLAB_SDK_ERROR_* 宏
DGLAB_SDK_EXPORT int dglab_find_game_dir(const char* game_id, DglabFindResult* out);

} // extern "C"
