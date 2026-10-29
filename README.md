<div align="center">
  <img src="assets/icon/dglab-client-256.png" width="128" alt="DG-LAB-Client icon"/>
  <h1>DG-LAB-Client</h1>
  <img alt="version" src="https://img.shields.io/badge/dynamic/json?url=https%3A%2F%2Fraw.githubusercontent.com%2FCrimsonSeraph%2FDG-LAB-Client%2Fmain%2Fconfig%2Fmain.json&query=%24.app.version&label=version&color=ff4d8d">
  <img alt="GitHub License" src="https://img.shields.io/github/license/CrimsonSeraph/DG-LAB-Client">
  <img alt="GitHub Repo stars" src="https://img.shields.io/github/stars/CrimsonSeraph/DG-LAB-Client">
  <p>一个基于 Qt/QML 的 DG-LAB 桌面客户端 · 自托管 WebSocket 中转 · 郊狼 V3 蓝牙直连</p>
</div>

一个 C++20 + Qt6/QML 的 DG-LAB 桌面客户端：外部数据（游戏遥测、传感器、无线按钮）经插件接入，由可视化规则图与事件映射换算成强度与波形，再通过应用自托管的中转服务或蓝牙直连下发给设备。

> 当前版本 `v2.4.0`（唯一来源为 `config/main.json` 的 `app.version`）· 变更见 [CHANGELOG.md](CHANGELOG.md)。

## 核心特性

- **自托管中转**：应用内托管 V3 / V4 中转服务并按官方终端协议生成配对二维码，不需要 Node / Python 进程。详见 [docs/features.md](docs/features.md)。
- **蓝牙直连与多设备**：郊狼 V3 已支持；负鼠、爪印、灵猫驱动为实验性，型号识别决定能否连接。详见 [docs/devices.md](docs/devices.md)。
- **插件化数据接入**：`IPlugin` 动态库 + 侧车 `plugin.json` 展示元数据，宿主负责扫描、加载、调度与日志。详见 [docs/plugin-development.md](docs/plugin-development.md)。
- **规则引擎与规则图**：插件级规则图 + 总图，节点与连线可视化编辑，启用按通道与图级联。详见 [docs/rules.md](docs/rules.md) 与 [docs/rule-graph.md](docs/rule-graph.md)。
- **事件映射**：插件声明事件，用户映射到强度 / 百分比 / 波形，宿主每帧合成并按设备实际写入帧数推进波形。详见 [docs/event-mapping.md](docs/event-mapping.md)。
- **波形库**：16 个官方只读波形，V2 / V3 双协议帧，JSON / `.pulse` / `.zip` 导入导出与两层标签库。详见 [docs/wave-data.md](docs/wave-data.md)。
- **看板、迷你窗与托盘**：首页看板汇总设备、通道与触发流水；迷你窗置顶常驻，托盘承接退出。详见 [docs/features.md](docs/features.md)。
- **主题与令牌**：17 套预设（默认 `classic`），颜色一律走 `Theme` 令牌。详见 [docs/theme.md](docs/theme.md)。

## 快速开始

1. 准备依赖：Qt 6（Core、Gui、Network、Qml、Quick、QuickControls2、QuickDialogs2、WebSockets、Bluetooth、Widgets、Concurrent）与支持 C++20 的编译器。
2. 配置：`cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/qt`
3. 编译：`cmake --build build --config Release`
4. 运行：可执行文件在 `build/Release`；同级的 `config/`、`assets/`、`python/` 由构建自动复制。
5. 验证：`cd build && ctest -C Release --output-on-failure`；打包用 `cpack`。

> 完整依赖、自动化构建、测试目标与验收清单见 [docs/build.md](docs/build.md)。

## 文档索引

| 文档 | 内容 |
| --- | --- |
| [docs/architecture.md](docs/architecture.md) | 分层、目录、数据流、关键架构决策与开发陷阱 |
| [docs/features.md](docs/features.md) | 连接、插件中心、看板、迷你窗与托盘、波形、日志、主题、自动更新 |
| [docs/devices.md](docs/devices.md) | 郊狼 / 负鼠 / 爪印 / 灵猫支持矩阵、默认设备与每设备上下限 |
| [docs/build.md](docs/build.md) | 依赖、构建、测试、环境事实与验收清单 |
| [docs/faq.md](docs/faq.md) | 常见问题：编译运行、配置、规则引擎、配对、通用、更新 |
| [docs/screenshots.md](docs/screenshots.md) | 截图清单与 [screenshot/README.md](screenshot/README.md) 的分工 |
| [docs/plugin-development.md](docs/plugin-development.md) | 插件接口、宿主能力、侧车元数据与构建示例 |
| [docs/rules.md](docs/rules.md) | 规则文件格式与规则编辑器操作 |
| [docs/rule-graph.md](docs/rule-graph.md) | 规则图节点、求值链与启用级联 |
| [docs/event-mapping.md](docs/event-mapping.md) | 事件声明、映射、合成顺序与事件波形下发 |
| [docs/wave-data.md](docs/wave-data.md) | 波形数据结构、标签规则与导入导出 |
| [docs/theme.md](docs/theme.md) | 主题令牌、预设配色与对比度 |
| [docs/community_plugin_template/README.md](docs/community_plugin_template/README.md) | 社区插件模板 |
| [docs/ui-optimization-plan.md](docs/ui-optimization-plan.md) | 阶段三前的界面分析快照（历史，不再更新） |
| [CHANGELOG.md](CHANGELOG.md) | 更新日志 |
| [CONTRIBUTING.md](CONTRIBUTING.md)、[CodingStyle.md](CodingStyle.md)、[CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) | 贡献流程、编码规范、行为准则 |
| [screenshot/README.md](screenshot/README.md) | 截图目录、规格与重新生成步骤 |
| [config/README.md](config/README.md)、[include/README.md](include/README.md)、[src/README.md](src/README.md)、[src/ui/README.md](src/ui/README.md)、[module/README.md](module/README.md) | 各目录的字段、分层与控件契约 |
| [NextStep.md](NextStep.md) | 后续计划 |

## 许可与致谢

本项目源代码采用 **GPL-3.0-only**，详见 [LICENSE](LICENSE)。第三方组件各自适用不同许可证：Qt（LGPLv3）、nlohmann/json（MIT）、Nayuki qrcodegen（MIT）、miniz 3.0.2（MIT）、Font Awesome Free 7.3.1（图标字体，FONT LICENSE / CC BY 4.0 / MIT）。完整声明见 [NOTICE](NOTICE) 与 `licenses/` 目录。

- 作者：CrimsonSeraph
- BiliBili：[浪天幽影（UID: 1741002917）](https://space.bilibili.com/1741002917)
- X：[@CrimSeraph_QwQ](https://x.com/CrimSeraph_QwQ)
- 项目主页：<https://github.com/CrimsonSeraph/DG-LAB-Client>
- 遇到问题请先看 [docs/faq.md](docs/faq.md)，仍未解决再提 [Issue](https://github.com/CrimsonSeraph/DG-LAB-Client/issues)。
