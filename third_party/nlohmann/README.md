# nlohmann/json（第三方库）

- 来源：https://github.com/nlohmann/json （v3.12.0 release 页面的单头文件 `json.hpp`）
- 版本：3.12.0（2026 年引入，锁定版本）
- 许可证：MIT（见同目录 `LICENSE.MIT`）
- 用途：配置 / 波形库 / 插件元数据等所有 JSON 读写共用的统一解析实现，源码以 `#include <nlohmann/json.hpp>` 引用。

## 文件形态

单头文件发行物（`json.hpp`），从上游 release 页面原样下载：

https://github.com/nlohmann/json/releases/download/v3.12.0/json.hpp

放置路径：`third_party/nlohmann/include/nlohmann/json.hpp`。

**未做任何本地修改**，后续也不要手工编辑该文件：升级时直接从上游对应 tag 的 release 页下载新的 `json.hpp` 覆盖即可，同时更新本 README 与根 `CMakeLists.txt` 里的版本断言。

## 构建

根 `CMakeLists.txt` 里以 INTERFACE 目标 `nlohmann_json` 引入，`target_include_directories` 指向 `third_party/nlohmann/include`，源码以 `#include <nlohmann/json.hpp>` 引用；主程序与相关测试目标都链接 `nlohmann_json`。

CMake 在配置阶段会读取头文件头部的 `NLOHMANN_JSON_VERSION_MAJOR` / `NLOHMANN_JSON_VERSION_MINOR` / `NLOHMANN_JSON_VERSION_PATCH` 宏做版本断言（必须为 3.12.0），任一不满足直接 `FATAL_ERROR`，防止有人换文件时版本漂移。

## 升级步骤

1. 从上游 release 页下载新版本 `json.hpp`，覆盖 `include/nlohmann/json.hpp`；
2. 更新本 README 的「版本」一行；
3. 更新 `CMakeLists.txt` 里三处版本断言（MAJOR / MINOR / PATCH）；
4. 跑一次完整构建与 `ctest`。
