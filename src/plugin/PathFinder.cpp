// Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
// SPDX-License-Identifier: GPL-3.0-only

#include "PathFinder.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#if defined(_WIN32)
// 注册表读取是 Windows 专有，全部隔离在本文件的 win 分支里
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

namespace fs = std::filesystem;

// 写入用的容量上限，直接取自结果结构的字段大小
constexpr std::size_t kPathCapacity = sizeof(DglabFindResult::path);
constexpr std::size_t kSourceCapacity = sizeof(DglabFindResult::source);

// GameProfile - 一个游戏标识对应的查找参数
struct GameProfile {
    // 对外暴露的 game_id
    const char* id;
    // Steam 库 steamapps/common 下的目录名
    const char* steam_common;
    // 非 Steam 安装时的常见目录名，没有就传空
    const char* standalone_folder;
};

// 与旧版脚本的默认值一致：CS 系列的 Steam 目录名是 Counter-Strike Global Offensive
const GameProfile kGameProfiles[] = {
    {"cs2", "Counter-Strike Global Offensive", nullptr},
    {"csgo", "Counter-Strike Global Offensive", nullptr},
    {"warthunder", "War Thunder", "War Thunder"},
    {"war-thunder", "War Thunder", "War Thunder"},
};

// RootCandidate - 一个 Steam 根目录候选以及它的来源标签
struct RootCandidate {
    std::string path;
    const char* source;
};

std::string to_lower_ascii(std::string text) {
    for (char& ch : text) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return text;
}

// 路径转 UTF-8：C++20 起 u8string 返回 char8_t 串
std::string to_utf8(const fs::path& path) {
#if defined(__cpp_lib_char8_t)
    const std::u8string text = path.u8string();
    return std::string(text.begin(), text.end());
#else
    return path.u8string();
#endif
}

bool path_is_directory(const fs::path& path) {
    std::error_code ec;
    const bool ok = fs::is_directory(path, ec);
    return ok && !ec;
}

std::string env_value(const char* name) {
    const char* value = std::getenv(name);
    return value != nullptr ? std::string(value) : std::string();
}

std::string home_directory() {
    std::string home = env_value("HOME");
    if (home.empty()) home = env_value("USERPROFILE");
    return home;
}

#if defined(_WIN32)

std::string wide_to_utf8(const std::wstring& text) {
    if (text.empty()) return std::string();
    const int length = static_cast<int>(text.size());
    const int size = ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(), length, nullptr, 0, nullptr, nullptr);
    if (size <= 0) return std::string();
    std::string result(static_cast<std::size_t>(size), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(), length, result.data(), size, nullptr, nullptr);
    return result;
}

// 读注册表字符串值：键不存在或类型不符都返回空串，不抛异常
std::string registry_string_value(HKEY root, const wchar_t* subkey, const wchar_t* value_name) {
    HKEY key = nullptr;
    if (::RegOpenKeyExW(root, subkey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
        return std::string();
    }
    DWORD type = 0;
    DWORD bytes = 0;
    std::string result;
    if (::RegQueryValueExW(key, value_name, nullptr, &type, nullptr, &bytes) == ERROR_SUCCESS &&
        (type == REG_SZ || type == REG_EXPAND_SZ) && bytes >= sizeof(wchar_t)) {
        std::wstring buffer(static_cast<std::size_t>(bytes) / sizeof(wchar_t) + 1, L'\0');
        if (::RegQueryValueExW(key, value_name, nullptr, &type, reinterpret_cast<LPBYTE>(buffer.data()),
                               &bytes) == ERROR_SUCCESS) {
            buffer.resize(static_cast<std::size_t>(bytes) / sizeof(wchar_t));
            while (!buffer.empty() && buffer.back() == L'\0') buffer.pop_back();
            result = wide_to_utf8(buffer);
        }
    }
    ::RegCloseKey(key);
    return result;
}

// 与旧版脚本一致：先读 WOW6432Node 的 32 位视图，再补一次 64 位视图
std::string registry_steam_install_path() {
    const wchar_t* const subkeys[] = {
        L"SOFTWARE\\WOW6432Node\\Valve\\Steam",
        L"SOFTWARE\\Valve\\Steam",
    };
    for (const wchar_t* subkey : subkeys) {
        const std::string value = registry_string_value(HKEY_LOCAL_MACHINE, subkey, L"InstallPath");
        if (!value.empty()) return value;
    }
    return std::string();
}

// Program Files 一类的安装根：非 Steam 游戏与传统安装目录都在这些根下
std::vector<std::string> program_files_roots() {
    std::vector<std::string> roots;
    const std::string pf86 = env_value("ProgramFiles(x86)");
    const std::string pf = env_value("ProgramFiles");
    if (!pf86.empty()) roots.push_back(pf86);
    if (!pf.empty()) roots.push_back(pf);
    // 环境变量缺失时的兜底，与脚本里硬编码的那条一致
    roots.push_back("C:\\Program Files (x86)");
    roots.push_back("C:\\Program Files");
    return roots;
}

#endif // _WIN32

// Steam 根目录候选：注册表优先，其后是常见安装路径，整段只做目录存在判断
std::vector<RootCandidate> steam_root_candidates() {
    std::vector<RootCandidate> roots;
#if defined(_WIN32)
    const std::string from_registry = registry_steam_install_path();
    if (!from_registry.empty()) roots.push_back({from_registry, "registry"});
    for (const std::string& base : program_files_roots()) {
        roots.push_back({to_utf8(fs::path(base) / "Steam"), "known-path"});
    }
#elif defined(__APPLE__)
    roots.push_back({"/Applications/Steam.app", "known-path"});
    const std::string home = home_directory();
    if (!home.empty()) {
        roots.push_back({to_utf8(fs::path(home) / "Library" / "Application Support" / "Steam"),
                         "known-path"});
    }
#else
    const std::string home = home_directory();
    if (!home.empty()) {
        roots.push_back({to_utf8(fs::path(home) / ".steam" / "steam"), "known-path"});
        roots.push_back({to_utf8(fs::path(home) / ".local" / "share" / "Steam"), "known-path"});
    }
#endif
    return roots;
}

// 非 Steam 安装的根目录：只有 Windows 有传统安装目录，其它平台留空
std::vector<std::string> standalone_root_candidates() {
    std::vector<std::string> roots;
#if defined(_WIN32)
    roots = program_files_roots();
#endif
    return roots;
}

// 轻量 VDF 词法切分：只认引号字符串、花括号与注释，够解析 libraryfolders.vdf
std::vector<std::string> vdf_tokens(const std::string& text) {
    std::vector<std::string> tokens;
    std::size_t index = 0;
    while (index < text.size()) {
        const char ch = text[index];
        if (std::isspace(static_cast<unsigned char>(ch))) {
            ++index;
            continue;
        }
        // 行注释 // 一直到行尾
        if (ch == '/' && index + 1 < text.size() && text[index + 1] == '/') {
            while (index < text.size() && text[index] != '\n') ++index;
            continue;
        }
        if (ch == '{' || ch == '}') {
            tokens.emplace_back(1, ch);
            ++index;
            continue;
        }
        if (ch == '"') {
            ++index;
            std::string value;
            while (index < text.size() && text[index] != '"') {
                // VDF 只把 \\ 与 \" 当转义，其它反斜杠原样保留
                if (text[index] == '\\' && index + 1 < text.size()) {
                    const char next = text[index + 1];
                    if (next == '\\' || next == '"') {
                        value.push_back(next);
                        index += 2;
                        continue;
                    }
                }
                value.push_back(text[index]);
                ++index;
            }
            ++index;
            tokens.push_back(value);
            continue;
        }
        std::string bare;
        while (index < text.size() && !std::isspace(static_cast<unsigned char>(text[index])) &&
               text[index] != '"' && text[index] != '{' && text[index] != '}') {
            bare.push_back(text[index]);
            ++index;
        }
        tokens.push_back(bare);
    }
    return tokens;
}

// 解析 libraryfolders.vdf 里登记的库路径，与脚本一样只取 "path" 的值
std::vector<std::string> parse_library_paths(const fs::path& vdf_path) {
    std::vector<std::string> paths;
    std::ifstream stream(vdf_path, std::ios::binary);
    if (!stream) return paths;
    const std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    const std::vector<std::string> tokens = vdf_tokens(text);
    for (std::size_t index = 0; index + 1 < tokens.size(); ++index) {
        if (to_lower_ascii(tokens[index]) == "path") paths.push_back(tokens[index + 1]);
    }
    return paths;
}

// 某个 Steam 根目录登记的附加库，主库不重复返回，只收真实存在的目录
std::vector<std::string> extra_library_folders(const std::string& steam_root) {
    std::vector<std::string> libraries;
    const std::vector<std::string> declared =
        parse_library_paths(fs::path(steam_root) / "steamapps" / "libraryfolders.vdf");
    const std::string root_lower = to_lower_ascii(steam_root);
    for (const std::string& entry : declared) {
        if (entry.empty() || to_lower_ascii(entry) == root_lower) continue;
        if (!path_is_directory(fs::path(entry))) continue;
        libraries.push_back(entry);
    }
    return libraries;
}

// 在 <库>/steamapps/common 下按目录名不区分大小写找游戏目录
std::string find_steam_game_dir(const std::string& library_root, const char* game_name) {
    const fs::path common = fs::path(library_root) / "steamapps" / "common";
    std::error_code ec;
    fs::directory_iterator iterator(common, ec);
    if (ec) return std::string();
    const std::string wanted = to_lower_ascii(game_name);
    for (const fs::directory_iterator end; iterator != end; iterator.increment(ec)) {
        if (ec) return std::string();
        const fs::path entry = iterator->path();
        if (!path_is_directory(entry)) continue;
        if (to_lower_ascii(to_utf8(entry.filename())) == wanted) return to_utf8(entry);
    }
    return std::string();
}

const GameProfile* find_profile(const std::string& game_id) {
    const std::string wanted = to_lower_ascii(game_id);
    for (const GameProfile& profile : kGameProfiles) {
        if (wanted == profile.id) return &profile;
    }
    return nullptr;
}

// 把命中结果写进调用方结构，路径过长时截断并给出可识别码
int write_result(DglabFindResult* out, const std::string& path, const char* source) {
    std::snprintf(out->path, kPathCapacity, "%s", path.c_str());
    std::snprintf(out->source, kSourceCapacity, "%s", source);
    out->found = 1;
    return std::strlen(path.c_str()) < kPathCapacity ? DGLAB_FIND_FOUND : DGLAB_SDK_ERROR_TRUNCATED;
}

// manual 覆盖用的环境变量名：DGLAB_GAME_DIR_<大写下划线标识>
std::string override_variable_name(const char* game_id) {
    std::string name = "DGLAB_GAME_DIR_";
    for (const char* ch = game_id; ch != nullptr && *ch != '\0'; ++ch) {
        const unsigned char value = static_cast<unsigned char>(*ch);
        name.push_back(std::isalnum(value) ? static_cast<char>(std::toupper(value)) : '_');
    }
    return name;
}

} // namespace

int dglab_sdk_version(void) {
    return DGLAB_SDK_VERSION;
}

int dglab_find_game_dir(const char* game_id, DglabFindResult* out) {
    if (out == nullptr) return DGLAB_SDK_ERROR_ARGUMENT;
    out->found = 0;
    out->path[0] = '\0';
    out->source[0] = '\0';
    if (game_id == nullptr || game_id[0] == '\0') return DGLAB_SDK_ERROR_ARGUMENT;

    try {
        const GameProfile* profile = find_profile(std::string(game_id));
        if (profile == nullptr) return DGLAB_SDK_ERROR_UNKNOWN_GAME;

        // 一档：显式覆盖，等价于原脚本的 manual 模式
        const std::string override_path = env_value(override_variable_name(profile->id).c_str());
        if (!override_path.empty() && path_is_directory(fs::path(override_path))) {
            return write_result(out, override_path, "manual");
        }

        // 二档：Steam，注册表与常见路径给的根优先，随后是 vdf 登记的附加库
        for (const RootCandidate& root : steam_root_candidates()) {
            const std::string hit = find_steam_game_dir(root.path, profile->steam_common);
            if (!hit.empty()) return write_result(out, hit, root.source);
            for (const std::string& library : extra_library_folders(root.path)) {
                const std::string library_hit = find_steam_game_dir(library, profile->steam_common);
                if (!library_hit.empty()) return write_result(out, library_hit, "steam-vdf");
            }
        }

        // 三档：非 Steam 安装，传统安装根下的同名目录
        if (profile->standalone_folder != nullptr) {
            for (const std::string& base : standalone_root_candidates()) {
                const fs::path candidate = fs::path(base) / profile->standalone_folder;
                if (path_is_directory(candidate)) {
                    return write_result(out, to_utf8(candidate), "known-path");
                }
            }
        }
    } catch (...) {
        out->found = 0;
        out->path[0] = '\0';
        out->source[0] = '\0';
        return DGLAB_SDK_ERROR_INTERNAL;
    }
    return DGLAB_FIND_NOT_FOUND;
}