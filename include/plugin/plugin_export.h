/*
 * Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

// ============================================
// 插件 API 版本：主程序加载插件时校验（get_plugin_api_version 返回值需等于本值）
// 接口不兼容变更时递增；插件与主程序必须使用同一 API 版本
// ============================================
#define PLUGIN_API_VERSION 1

// ============================================
// PLUGIN_EXPORT - 动态库导出宏（统一跨平台符号导出）
// 编译插件动态库时定义 PLUGIN_BUILD，类接口自动标记导出
// ============================================
#if defined(_WIN32)
// Windows：__declspec(dllexport)
#define PLUGIN_EXPORT __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
// GCC / Clang（Linux/macOS/MinGW）：默认可见性导出
#define PLUGIN_EXPORT __attribute__((visibility("default")))
#else
#define PLUGIN_EXPORT
#endif

#ifdef PLUGIN_BUILD
// 构建插件动态库：类接口标记为导出（保证 vtable 随 DLL 导出）
#define PLUGIN_API PLUGIN_EXPORT
#else
// 宿主侧：接口仅用于声明，无需导入标记（宿主通过 QLibrary 解析导出函数）
#define PLUGIN_API
#endif
