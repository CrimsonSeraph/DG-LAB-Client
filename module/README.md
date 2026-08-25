# 插件目录（module）

本目录存放 **动态库插件** 的源码。插件是符合 `IPlugin` 接口的动态库模块，由主程序（`ModuleManager`）在启动时扫描并动态加载/卸载，实现数值模块的热插拔扩展，无需重新编译主程序。

> 运行时扫描目录为 **可执行文件旁的 `module/` 目录**（不是本源码目录）；构建后插件 DLL 由 CMake `POST_BUILD` 自动复制到该目录。扫描路径可通过 `config/main.json` 的 `app.module.path` 更改。

---

## 目录结构

```
module/
├── example/            # 示例空壳插件（验证接口与导出约定，注册 1 个演示数值）
├── gsi/                # CS2 GSI 插件（完整功能示例：路径查找、配置生成、SteamID 归属区分）
└── README.md           # 本说明文件
```

---

## 插件快速上手

### 1. 源码组织

每个插件一个子目录，包含：

| 文件 | 说明 |
| --- | --- |
| `XxxPlugin.h` | 插件类声明（类名以 `Plugin` 结尾，继承 `IPlugin`） |
| `XxxPlugin.cpp` | 实现 + `extern "C"` 导出三件套（`get_plugin_api_version`/`create_plugin`/`destroy_plugin`） |

### 2. 接口与宿主能力

- **插件接口**: `include/plugin/IPlugin.h` —— 生命周期（`initialize`/`uninitialize`/`cleanup`/`can_unload`）、自描述（`name`/`version`/`api_version`/`capabilities`/`dependencies`）、线程安全声明、`on_host_period_changed` 周期通知。
- **宿主上下文**: `include/plugin/PluginHost.h` —— `IPluginHost` 提供数值注册/写入、数据接收（`listen_data`/`register_data_handler`）、配置读写（`get_config_value`/`set_config_value`）、基础周期与用户通知（`notify`）。
- **导出宏**: `include/plugin/plugin_export.h` —— `PLUGIN_EXPORT`/`PLUGIN_API` 与 `PLUGIN_API_VERSION`。

### 3. 最小示例（骨架）

```cpp
// MyPlugin.h
#include "PluginHost.h"

class MyPlugin : public IPlugin {
public:
    std::string name() const override { return "我的插件"; }
    std::string version() const override { return "0.1.0"; }
    PluginError initialize() override;   // 注册数值、注册数据处理器
    void uninitialize() override;        // 注销数值与处理器
};

// MyPlugin.cpp
PluginError MyPlugin::initialize() {
    if (host_) {
        std::vector<ModuleValue> values;
        values.emplace_back("my_value", "我的数值", QueryPeriod::SECOND, "field");
        host_->register_module_values(name(), values, {"A", "B"});
    }
    PLUGIN_LOG(this, PluginLogLevel::Info, "插件初始化完成");
    return PluginError::Ok;
}

extern "C" {
PLUGIN_EXPORT int get_plugin_api_version() { return PLUGIN_API_VERSION; }
PLUGIN_EXPORT IPlugin* create_plugin() { return new MyPlugin(); }
PLUGIN_EXPORT void destroy_plugin(IPlugin* plugin) { delete plugin; }
}
```

### 4. CMake 构建（参考根 CMakeLists.txt）

```cmake
add_library(my_plugin SHARED
    module/my/MyPlugin.h
    module/my/MyPlugin.cpp
    src/module/ModuleValue.cpp        # ModuleValue 为纯 C++ 模型，编入插件以构造数值
)
target_compile_definitions(my_plugin PRIVATE PLUGIN_BUILD)
target_include_directories(my_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/plugin
    ${CMAKE_CURRENT_SOURCE_DIR}/include/module
)
target_link_libraries(my_plugin PRIVATE Qt::Core Qt::Network)  # 按需
if(WIN32)
    set_target_properties(my_plugin PROPERTIES PREFIX "")
endif()
add_custom_command(TARGET my_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:my_plugin>"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/module"
)
```

### 5. 扫描与加载

- 主程序扫描 `*.dll`/`*.so`/`*.dylib` 文件作为插件候选，**默认只扫描不加载**（模块页显示“暂未加载”，右侧“启用”按钮加载）。
- `app.module.scan_load=true` 时扫描即加载（调试用）。
- 加载流程: `QLibrary` → API 版本校验 → 依赖校验 → `create_plugin` → `attach_host` + `set_log_callback` → `initialize()`。
- 卸载流程: `can_unload()` → `uninitialize()` → `cleanup()` → `destroy_plugin()` → `QLibrary::unload`。

---

## 规范要求

- **日志**: 插件内部使用 `PLUGIN_LOG(this, level, ...)` 宏（不直接使用 `LOG_MODULE`），由宿主统一记录，类名/方法名自动为插件名称与函数名。
- **内存隔离**: 插件实例在插件内 `new`，宿主仅调用 `destroy_plugin`；禁止跨模块 `new`/`delete`、禁止静态全局变量。
- **错误处理**: 失败操作返回 `PluginError` 错误码，不抛异常。
- **ABI 兼容**: 必须与主程序使用同一工具链（MinGW g++ + Qt）构建。
- **API 版本**: 接口变更时递增 `PLUGIN_API_VERSION`。

---

## 详细说明

完整的插件接口说明、导出约定、宿主能力表格、构建示例与加载流程，请参阅根目录 [README.md](../README.md) 的“六.12 插件开发指南”。
