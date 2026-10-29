# Changelog

本文档记录 DG-LAB-Client 所有 notable 的版本变更。

版本号格式遵循 [语义化版本 2.0.0](https://semver.org/lang/zh-CN/)

> **注意**: 当前版本为 v2.4.0（版本号唯一来源为 `config/main.json` 的 `app.version`），正在逐步修复、完善功能。

## [Unreleased]

> **变动**: [v2.4.0...HEAD](https://github.com/CrimsonSeraph/DG-LAB-Client/compare/v2.4.0...HEAD)

### Added

- **规则图通道节点改为输出端点并按设备下发**: 通道 A / B 节点从数据源变成数据汇——只有一个输入端口，上游任意节点可连入，由它负责下发；节点新增目标设备字段 `targetDevice`（空 = 跟随默认设备，非空 = 指定 MAC，旧规则文件缺该字段时按跟随默认设备读入且不改写文件）。同一设备同一通道只允许一条入边，连入新连线会自动断开该设备该通道上原有的入边并记一条 INFO 日志，不同设备的同名通道互不影响。求值出通道值时命令带 `device` 字段，由 `DeviceController::send_device_strength()` 解析后下发：默认设备沿用「规则 + 事件取较大值 → 通道上限」的合并路径，其它在线设备按该设备上下限直接下发；目标设备离线 / 不存在 / 无默认设备时不下发并记一条限流 WARN（不弹 Toast，避免每帧刷屏）。编辑器右侧属性面板改为按节点类型用 `Loader` 只构建适用控件（通道节点只显示「目标设备」下拉，设备列表与设备管理页上下限下拉同一份 `outputDeviceTargets()`，离线项标注「离线」），节点高度改由桥接给出的 `rowCount` 统一。
- **基础组件库与取色对话框**: 新增 `AppSwitch` / `AppSlider` / `AppTabs` / `AppChip` / `AppCard` / `AppField` / `AppSparkline` / `AppStatusButton` / `AppSegment` 九个基础组件与 `AppColorDialog`，自定义主题选色改为应用内对话框（HEX 输入与色相 / 饱和度 / 明度三条滑杆，非法 HEX 禁用「确定」），不再弹出系统取色框。
- **迷你窗与托盘常驻**: 新增无边框置顶迷你窗（A/B 强度大数字与占比、通道启停、透明度与三档大小、近 5 条触发流水与 A/B 滚动波形）与托盘菜单（显示主窗口、显示迷你窗、退出），关闭主窗口改为隐藏到托盘，首次运行还会提示托盘图标可能被 Windows 11 收进折叠区。
- **插件中心与侧车展示元数据**: 插件中心提供搜索、分类过滤、网格 / 列表切换与已启用栏，插件详情分「模式与输出 / 触发事件 / 数值」三页签，通道上限与安全上限一律取通道级全局配置（未启用插件不再显示 `0/0`）；插件可随 `plugin.json` 提供中文展示名、作者、描述、标签、头图与事件声明，界面显示名与模块标识解耦。
- **插件内容扩充**: CS2 插件数值 23 → 25、事件 3 → 31 并补数值表与事件表，新增战争雷霆遥测插件（12 个数值、8 个事件），示例插件改为仓库内的社区模板。
- **事件引擎**: 由侧车事件声明驱动五类比较、通道合并与波形包络，事件可按强度 / 百分比 / 波形三种数据类型输出，支持同通道互斥、模块级默认输出通道与容量 200 的触发流水，波形类事件播完恢复接管前的波形，插件卸载后不再残留其事件状态。
- **规则编辑器**: 新增插件级规则图与输出节点，总图左栏改为插件选择器，插件输出节点可点入其插件图并跳回总图，连线可增删而默认输出节点不可删除。
- **波形库与导入导出**: 官方 16 个波形作为只读内置资源并同时保存 V2 / V3 两套帧，发送按 `app.wave.protocol` 取帧、缺帧时明确报错并拒发；波形支持单条与 zip 批量导入导出（含路径穿越与体积上限防护），内置标签与自定义标签分开存放，官方波形也可打标且重建波形库后不丢。
- **设备型号识别与默认设备**: 扫描结果按型号表区分已支持 / 实验性 / 驱动未实现，名称未知的设备按地址记忆与连接后首帧回推型号，默认设备在可输出设备间按郊狼优先评选且可在首页下拉覆盖，通道上限与安全上限支持按设备设置。
- **负鼠、爪印与灵猫设备（实验性）**: 新增负鼠振动控制器输出，以及爪印无线按钮与灵猫气压传感器的输入接入，按键、气压与电量作为模块数值进入事件与规则。
- **图标与双连接状态**: 入库 Font Awesome Free 7.3.1 并新增 `AppIcon`，左侧导航改用图标，连接状态统一为「已连接设备 ×N + 蓝牙 + App」口径。
- **可配置文件日志**: 日志按天写入 `<exe>/log/dglab-YYYYMMDD.log` 并按大小滚动，新增 `app.log.file_enabled` / `file_level` / `file_max_mb` 三个配置键。
- **网卡 IP 选择**: 中转连接下方新增「选择网卡 IP」，可手动指定有效 IPv4 并按黑白名单过滤，选择结果随配置持久化。
- **输入设备多实例（爪印 / 灵猫）**: 输入设备不再各有一个固定实例——扫描入口把型号识别结果交给 `BleDeviceManager::open_input_link()`，每台设备按 MAC 建一个驱动实例并以 MAC 登记，断开或自行掉线时 `close_input_link()` 注销并销毁该实例；每台实例一份 `DeviceValueBridge` 与一个独立数值模块，模块名按 MAC 后四位生成 `paw_<后四位>` / `civet_<后四位>`（撞车追加 `_2` / `_3` 并记 WARN），实例显示名形如「爪印 430C」。内置事件声明以实例模块名为模块标识注册，断开时按「注销数值模块 → 注销内置声明（新增 `EventRuntime::unregister_builtin_events()`）→ 清引擎运行期状态」的顺序清理，重连后重新出现；键 / 参数阈值、D0 默认不落值、气压 epsilon 抑制三处语义未改。旧配置里 `app.plugins.paw` / `app.plugins.civet` 的映射在首台实例连上时自动迁移到实例模块键并留 `module_migrated_to` 标记（见 [docs/devices.md](docs/devices.md) 的「输入设备实例名与旧配置迁移」）。
- **设备管理页**: 新增左侧导航「设备管理」（`AppBridge::Page::DevicePage = 8`，追加在枚举末尾，`ModulePage = 2` 的占位槽与既有页索引不变），四张卡——蓝牙扫描结果（蓝牙名 / MAC / 型号 / 支持三态 + 原因，行尾「连接」/「断开」可多台同时连接）、已连接链路（`ble.connectedLinks` 的 MAC / 型号 / 电量 / 连接序号，逐台断开）、默认设备（只列在线输出设备，与首页下拉同一份评选与写入）、通道上限与安全上限（先选设备再分别编辑 A / B，写入 `app.devices.<MAC>.{limit_a,safety_a,limit_b,safety_b}` 并显示生效值 `min(上限, 安全上限)`，未设置的通道标「默认」）。

### Changed

- **规则编辑器跨图暂存与全量保存**: 编辑器打开期间在内存里维护总规则图与所有已载入插件规则图的草稿，切图只切换视图——插件图里改完切到总图再切回来不再丢改动，也不再在切图时重读磁盘（此前切图会重新载入，插件图改动丢失）。脏标记改为按图粒度（每张图各自脏，保存按钮与关闭确认按图汇总），保存按钮一次写盘全部有改动的图：顺序为总规则图 → 插件图按标识排序，全部写完后统一重建引用列表，某张失败不中断其余并保留该图的草稿与脏标记（Toast 汇总失败与已保存张数）。编辑器从固定尺寸弹窗改成可自由调整大小的窗口（初始 1100×700、最小 900×600，右下角自绘手柄拖动，画布随窗口放大、左右栏保持令牌宽度）。关闭时若有改动弹三选「保存并关闭 / 放弃改动 / 取消」（新增 `dialogs/RuleDiscardDialog.qml`，继承 `AppDialog`；Esc 与关闭按钮走同一判定，无改动直接关闭）。
- **设置页瘦身、蓝牙操作集中到设备管理页**: 设置页删除整块「蓝牙直连（郊狼 V3 / 负鼠）」子区（扫描 / 断开 / 设备列表 / 强度增减 / 屏幕同步提示 / 电量）与对应 objectName，只保留中转连接与主题；`ConfigPage.qml` 不再引用任何 `ble.` 属性，`UiConnector` 对 `bleScanButton` / `bleDisconnectButton` / `bleStrength*` / `bleDeviceList` 的连接一并移除，没有悬空连接。`CoyoteBleController` 新增 `has_link(address)` 与 `disconnectDeviceByAddress(address)`（按 MAC 断开，其它设备不受影响），`DeviceController` 新增可写入口 `setDeviceLimit(mac, channel, kind, value)` 与回读 `limitsFor(mac, channel)`（含 `device_limit` / `device_safety` 来源标记），`limits_for()` 的回退链保持唯一实现。
- **蓝牙多链路（第一阶段）**: 蓝牙输出设备不再互相排斥。扫描与链路管理拆开——`CoyoteBleController` 只负责发现与型号识别（结果按 MAC 去重），新增 `BleDeviceManager` 按识别出的型号为每台设备创建驱动实例（郊狼 `CoyoteBleDevice` / 负鼠 `OpossumBleController`）并以 MAC 为键登记；原先「连接负鼠前先断开郊狼」这类互斥代码全部移除，任意数量的郊狼与负鼠可以同时在线，断开一台只销毁该 MAC 的实例。每台设备独立维护连接状态、电量、强度回执与通道波形进度，界面上的连接状态 / 电量 / 强度改为读**默认设备**的只读快照，新增 `ble.connectedLinks` 给出全部已连接设备（MAC / 型号 / 电量 / 连接序号）。默认设备评选抽成 `pick_default_device()` 单一实现（用户覆盖 → 郊狼 > 负鼠 → 同类型先连接者优先），`DeviceController` 与界面快照共用；新增单元测试 `device_link_test`（MAC 登记表增删、默认设备评选、断开一台不影响其它，用实现 `IOutputDevice` 的合成设备驱动）。输入设备（爪印 / 灵猫）本阶段仍是各一个固定实例，多实例与设备管理页留到第二阶段。
- **构建默认不编译单元测试**: 新增 CMake 开关 `DGLAB_BUILD_TESTS`（默认 OFF），普通构建与发布构建不再编 14 个测试目标；测试产物改落构建目录的 `tests/` 下，不再与 `DG-LAB-Client` / 插件混在同一输出目录，手工压缩输出目录发布也不会带出测试程序；三个平台 CI 改为带开关构建并执行 `ctest`（此前测试只编译、从不运行）。
- **看板与波形标签布局、插件卡头图圆角**: 首页看板改为三行——设备状态卡与「已启用插件 + 近期触发」同行（右侧内容超出时在区域内滚动）、通道 A 与通道 B 卡各与自己的波形卡同行，波形卡由一张 A/B 并排卡拆成 `dashboardWaveCardA` / `dashboardWaveCardB` 两张单通道卡（下发帧、强度与空态各自独立）；波形设置页的标签从波形名上方移到同一行靠右，标签总宽超限时裁剪并可用滚轮或按住拖动横向查看；插件卡头图常态即按卡片圆角裁切，与焦点态一致。
- **配置文件外部修改即时生效**: 手动编辑 `main.json` / `system.json` / `user.json` 后约 0.4 s 自动重读并通知各模块，JSON 损坏或校验失败时保留上一份有效配置并记 WARN；保存时与磁盘做三方合并，外部新增的键不再被内存旧值覆盖，设备上下限也不再靠 2 秒轮询刷新。
- **首页看板与页面精简**: 主窗口改为窄列表导航加左下角常驻面板，首页收敛为设备状态、A/B 通道卡、已启用插件、近期触发与波形卡，强度调节与波形选择改到迷你窗与波形设置页；配置页只保留连接与主题，模块页并入插件中心，关于页改为面向普通用户的中文介绍。
- **更新入口独立成对话框**: 版本与更新说明、错误行、下载进度与安装入口收进独立对话框，由左下角「检查更新」打开，关于页不再有软件更新卡片。
- **默认主题改为 classic**: 默认配置、出厂 `user.json` 与主题回退值统一为 `classic`，未知主题也落到该预设。
- **自动更新链路补全**: 检查、下载、SHA256 校验与脚本安装重启链路补全，客户端拉取按平台生成并内嵌下载地址的 manifest，可跳转发布页查看完整说明。
- **「已连接设备 ×N」改为真实链路数**: 新增 `ble.connectedLinkCount`（输出链路逐台累加 + 已连上的输入设备），看板设备卡、左下角面板与状态栏三处共用一份口径，蓝牙分项统一为「蓝牙链路 N 条」；设备管理页「已连接设备」列表同样把输入设备实例并入 `ble.connectedLinks`（行内带实例模块名），条数与计数同源。
- **上下限设备下拉支持离线设备**: 设备管理页「通道上限与安全上限」的目标下拉改为「在线输出设备 + 已记忆设备」（`app.devices` 里出现过的 MAC，纯输入设备除外），离线项行名带「离线」且可选中读改——上下限写的是 `app.devices.<MAC>` 配置键，不要求设备在线；默认设备下拉不变，仍只列在线输出设备。

### Fixed

- **退出崩溃**: 输出设备先于控制器销毁、以及 `main` 返回后配置保存仍写日志这两条回调路径都不再指向已销毁对象，空闲 / 设备在线 / 连接后断开三种退出均正常退出。
- **蓝牙强度回显与电量**: 连接后强度回执接到界面反馈，电量特征读取与周期刷新补齐、不再恒为 -1，断开信号晚到导致的重连失败一并修复并在重连后重发上次强度。
- **规则命令通道解析**: 规则写字符串 `A` / `B` 而设备侧按整数解析，导致 B 通道强度落到 A，已改为按字符串解析。
- **配对二维码载荷**: 配对二维码改为官网下载地址 + `DGLAB-SOCKET` 标签 + V3 服务地址的官方格式，第三方 App 可正常识别，界面不再显示整串地址。
- **界面回显与细节**: 查询周期下拉回显实际周期、各值不一致时给出提示，数值页签补常驻滚动条，下拉展开时不再有委托类型告警，蓝牙直连状态行不再显示两行重复文字，近黑主题色块在 125% 缩放下也能看清边界，波形选择对话框在经典主题下不再沿用浅色底。
- **规则编辑器右键菜单**: 只有命中连线时才出现「删除连线」项，未命中不再占菜单空槽位。
- **设备配置键偶发丢失**: 保存前的三方合并改用「最后一次读盘内容」作基准（此前用的是上次写出的内容），运行期才创建的 `app.devices.<MAC>` 子树的键不会再被外部整份覆盖判成「磁盘删除」而静默清掉；`model` 与 `limit_a` / `safety_a` 连续写入后三者同时存在，磁盘删除与内存改动冲突时保留内存值并记 WARN，保存后回读校验不通过时不再把该内容记成已同步。

### Removed

- **模块页与遗留面板**: 删除 `ModulePage.qml` 与已无引用的 `ChannelPanel.qml`，模块数值改在插件中心与插件详情里查看。
- **事件启用开关**: 移除事件映射中的 `enabled` 与事件通道设置，事件是否启用只由规则级联决定。
- **内置默认规则与占位波形**: 移除内置默认规则，改为总图加每插件默认输出节点；两个占位波形由官方 16 个波形取代。

## [v2.4.0] - 2026-10-24

> **变动**: [v2.3.0...v2.4.0](https://github.com/CrimsonSeraph/DG-LAB-Client/compare/v2.3.0...v2.4.0)

### Added

- **自动更新**: 新增 `UpdateManager` / `UpdateBridge` 与关于页「软件更新」卡片，支持检查 GitHub Releases 清单、按平台下载并校验 SHA256、生成脚本替换文件并自动重启（安装经确认弹层二次确认）。脚本按主程序 PID 精确等待旧进程退出，并通过 `--updated` 启动后的健康标记判定成功；macOS / Linux 失败自动回滚，Windows 仅记录失败标记。

### Changed

- **打包内容添加默认提供的插件**: 防止无插件可用，含：
    - `dgl_example_plugin` 演示用插件。
    - `dgl_gsi_plugin` CSGO2 相关插件。
- **关闭强制开启的控制台**: 关闭在测试配置系统时强制启用的控制台，改为通过配置文件决定启用与否。
- **发布包精简**: Linux 与 macOS 发布包不再包含 `qmltooling` 调试插件与 Qt 自带翻译，减小体积并避免暴露调试能力；Linux 工作流缓存 AppImage runtime，降低 CI 网络依赖。

### Fixed

- **修复下拉菜单无法显示文本**: 使用 `ComboBox` 内置的 `textAt(int index)` 直接根据索引获取显示文本，完全绕开委托内部的数据访问问题。
- **规则图保存日志输出错误**: `RuleGraph::save` 的日志三元表达式未加括号，`<<` 优先级高于 `?:`，实际输出布尔值而非中文结果消息。
- **编译警告清理**: `ModuleManager::set_value` 补 `override`；`RuleGraph` / `UiConnector` / `DataListener` 中 `indexOf()` 返回值改用 `qsizetype`，消除 64→32 位截断警告；CMake 显式设置 macOS 最低系统版本，消除 `LSMinimumSystemVersion` 为空警告。

## [v2.3.0] - 2026-10-23

> **变动**: [v2.2.0...v2.3.0](https://github.com/CrimsonSeraph/DG-LAB-Client/compare/v2.2.0...v2.3.0)

### Added

- **设计令牌体系扩充**: 在 `Theme` 中新增焦点、禁用、选中文字等 13 个颜色令牌，并按 WCAG 对比度逐主题校正；`Metrics` 补充组件度量（控件高度、圆角、描边宽度等），界面不再散落魔法数。
- **页面骨架与一次性反馈**: 新增 `PageHeader`（页面标题 + 说明 + 右侧操作区）与 `Toast`（3 秒自动消失，按级别着色），页面标题与反馈文案不再各写各的。
- **通用确认弹层 `ConfirmDialog`**: 危险操作统一走同一套确认弹层，按钮文案、层级与遮罩一致。
- **应用图标与多尺寸资源**: 新增源矢量与 16 至 1024px 全套 PNG、Windows 多尺寸 ICO，窗口图标与可执行文件图标同源导出。
- **状态徽标 `StatusBadge`**: 连接、加载、通道等状态统一为徽标样式，替代纯文字提示。
- **`AppButton` 描边态**: 次级操作改为描边按钮，主次操作层级在视觉上可区分。
- **`AppSpinBox` 可收起指示器**: 数值输入框的上下步进按钮可按需隐藏，窄宽度下不再挤占文本区。

### Changed

- **首页与配置页重排**: 卡片内层去掉多余描边，启停开关移入卡片头部，连接卡改为两列布局，信息阅读顺序自上而下。
- **导航栏取消折叠**: 侧边导航常驻完整标签，不再有折叠/展开态与其带来的状态残留。
- **状态栏改常驻摘要**: 状态栏只显示连接与通道的常驻摘要，一次性消息统一交给 `Toast`。
- **A/B 通道等宽对齐**: 两通道卡片宽度、内边距与控件基线严格对齐。
- **对话框统一**: 对话框统一 footer 结构与遮罩令牌，高度自适应，不再各写一套按钮排布。
- **Esc / Enter 键位语义**: Esc 关闭当前层、Enter 确认当前操作，弹层与对话框行为一致。
- **版本号改为单一来源**: 版本号统一由 `config/main.json` 的 `app.version` 提供，CMake 读取后生成 `AppVersion.h` 注入 C++，QML 与文档不再写死版本号。

### Fixed

- **规则编辑器画布死锁**: 连线完成后画布不再卡死，节点拖动与连线可继续操作。
- **日志面板滚动**: 修复日志面板无法向上滚动、且新日志实际未贴底的问题。
- **强度输入回灌**: 修复外部同步值回灌覆盖用户正在输入的强度值。
- **波形发送误报**: 未连接设备时发送波形不再谎报"已发送"，改为明确提示未连接。
- **禁用态无视觉**: 按钮、输入框、下拉框补齐禁用态外观，禁用控件不再与可用控件难以区分。
- **通道卡与模块页溢出**: 修复长名称、多设备与窄窗口下卡片内容溢出边界。
- **对话框最小窗口溢出**: 窗口缩到最小尺寸时对话框内容不再被截断。
- **未保存改动静默丢弃**: 存在未保存改动时切换规则文件/关闭编辑会先确认。
- **配置页 B 预览塌陷**: 修复配置页 B 通道预览区域高度塌陷。

### Removed

- **`Typography` 过渡字号别名**: 移除过渡期的字号别名，字号统一走语义令牌。
- **`ComponentStyle` 死令牌**: 移除无消费者的组件样式令牌。
- **旧 Widgets 界面遗留截图**: 删除 `screenshot/others/` 与 `screenshot/themes/`（均为已废弃的 Widgets 界面截图）。
- **第三方壁纸素材**: 移除 `assets/normal_image/`，导航栏左上角改复用应用图标的 128px 版本。

## [v2.2.0] - 2026-09-30

> **变动**: [v2.1.0...v2.2.0](https://github.com/CrimsonSeraph/DG-LAB-Client/compare/v2.1.0...v2.2.0)

### Added

- **蓝牙直连输出与波形单次静音**: 蓝牙已连接时，规则输出与波形发送优先经 `CoyoteBleController` 直连下发，否则回落内置中转服务；新增按通道的单次定时器，在持续时长到达后静音。相关文档已更新。

### Changed

- **规则命令输出链路**: 恢复 `RuleManager::rule_command_ready` 到 `DeviceController::send_command` 的连接，确保规则计算出的通道指令能够正常下发。
- **代码整理**: 统一源文件函数定义顺序与头文件声明顺序；优化代码注释与日志输出；清理过期文档与注释。

### Fixed

- **规则命令无接收方**: 修复自 `58528b3` 起规则计算出的通道指令被丢弃的问题。
- 修复 CMake 中关于 `Qt QML` 打包相关问题。

### Removed

- **无用配置字段**: 移除 `python.bridge_path` 字段，该键已无任何消费者；同步更新配置示例与文档。

## [v2.1.0] - 2026-09-29

> **变动**: [v2.0.0...v2.1.0](https://github.com/CrimsonSeraph/DG-LAB-Client/compare/v2.0.0...v2.1.0)

### Added

- **配置页滚动支持**: 配置页长度超出窗口时，自动启用向下滚动，避免内容被截断。
- **蓝牙扫描列表优化**: 蓝牙扫描结果列表改为动态高度——无设备时收起，有设备时自动增加高度，设备较多时在最大高度（约 10 个设备）内内部滚动，避免撑爆页面。

### Changed

- **蓝牙连接稳定性**: 缓存完整的 `QBluetoothDeviceInfo`，防止 Android 平台丢失 LE 标志导致无法连接。
- **蓝牙设备发现**: 移除硬编码的 `47L` 名称过滤，改为按服务 UUID（`0x180C`）扫描或让用户手动选择设备，解决 Android 13+ 设备名称为空导致搜索不到的问题。
- **蓝牙调试日志**: 增加扫描、连接、服务发现过程中的详细日志与错误提示，便于排查连接问题。
- **规则编辑器布局**: 改用 `SplitView` 布局，优化界面空间分配。

### Fixed

- **蓝牙权限问题**: 修复 Qt 6.9 中 `requestPermission` 非静态调用及权限状态枚举（`Qt::PermissionStatus`）的编译错误。
- **蓝牙扫描崩溃**: 修复权限授予后递归调用 `startScan` 导致栈溢出的问题。
- **规则编辑器交互**: 修复无法拖动节点画布及节点无法拖拽的问题。

## [v2.0.0] - 2026-09-22

> **变动**: [v1.0.0...v2.0.0](https://github.com/CrimsonSeraph/DG-LAB-Client/compare/v1.0.0...v2.0.0)

### Added

- **QML 界面层**: 新增 `Dglab` QML 模块（`MainWindow` / 页面 / 组件 / `style` 单例），以 `qt_add_qml_module` 接入构建；界面采用左侧导航 + 页面栈 + 状态栏结构（原生标题栏）。
- **主题令牌系统**: 新增 `ThemeManager`（注册为 QML 单例 `Theme`），提供 16 套预设主题与自定义主/副色的语义化颜色令牌，自定义色持久化到 `user.json`（`app.ui.custom.primary` / `app.ui.custom.secondary`）。
- **界面桥接对象**: 新增 `AppBridge`（应用信息与页面导航）、`DeviceController`（连接状态、地址读写、二维码、强度/波形/清除指令与设备回传解析）、`HomeBridge`（A/B 通道强度、上限、启用状态、模块摘要与规则摘要）。
- **首页 QML 通道面板**: A/B 通道面板包含强度（可编辑目标值、± 快捷、上限显示）、模块摘要、规则摘要（最近计算值）、波形入口与启停按钮。
- **配置页连接与主题卡片**: 连接卡片（IP / 端口 / 连接断开 / 二维码）；主题卡片合并为单卡，展示当前主题名称、主色与副色，支持预设主题网格与 `ColorDialog` 自定义取色。
- **配置页日志卡片**: 新增日志视图（`LogBridge` 级别过滤、自动滚动到底部）与导出 / 清空按钮，导出目录由 `LogExporter` 提供。
- **集中式 QML↔C++ 连接**: `UiConnector` 按 `objectName` 统一建立交互连接；列表/中继器动态委托通过可视子树扫描连接。
- **波形库与波形编辑器**: 新增 `Wave`/`WaveLibrary`（`config/waves/waves.json`，内置三种波形）与 `WaveBridge`；QML 提供 `WavePreview`（Shape 强度/频率双曲线）、`WaveSelectDialog`（"确定波形"改为"选择波形"）与 `WaveEditorDialog`（段落+关键帧、原始 V3 帧、实时预览、保存/发送）。
- **应用内 WebSocket 中转服务**: 新增 `DglabRelayServer`，由应用直接托管 V3（9999）与 V4（9998）中转服务（配对、心跳、强度/波形/清除、V4 设备与 `device.op`），不再依赖 Node 官方后端与 Python `Bridge.py`/`WebSocketCore.py`。
- **内置二维码**: 内嵌 Nayuki `qrcodegen`（MIT，`third_party/qrcodegen`）与 `QrImageProvider`，配对二维码由应用自身生成。
- **规则可视化编辑器**: 新增 `RuleGraph`（规则/模块源/运算符/高级/通道输出节点与连线模型、规则 JSON 双向转换、侧车文件保存位置）与 `RuleGraphBridge`；QML 提供节点画布（平移/缩放、拖拽移动、拖拽连线、右键建节点、Ctrl+C/V、Delete）、模块数值面板与节点检查器；首页与配置页入口打开编辑器。
- **蓝牙直连（郊狼 V3）**: 新增 `CoyoteBleController`，按官方 V3 蓝牙协议扫描设备（47L121000 / 47L120100）、连接、每 100ms 写 B0（强度 + 双通道波形）、写 BF 软上限与平衡参数、解析 B1 强度回传与电量；配置页新增蓝牙卡片（扫描/连接/设备列表/电量/强度增减）。
- **主题化输入控件**: 新增 `AppSpinBox` / `AppTextField` / `AppTextArea` / `AppComboBox` / `AppCheckBox`（`src/ui/qml/components/`），输入控件的文字、占位符、选区、弹层与选中态全部改用 `Theme` 令牌；通道面板、配置页、模块页、规则编辑器与波形编辑器均已替换。
- **多主题配色与对比度保障**: `ThemeManager` 预设扩展至 16 套（新增「午夜蓝」「森野绿」），并按 WCAG 逐主题校正文字、边框与语义色——`textPrimary` 对 `surface` ≥ 7:1，`textSecondary` / `textMuted` 对 `surface` / `surfaceAlt` ≥ 4.5:1；令牌清单、预设色值与扩展方式见 [docs/theme.md](docs/theme.md)。

### Changed

- 程序入口由 `QApplication` + `DGLABClient` 改为 `QGuiApplication` + `QQmlApplicationEngine`；界面统一使用 `Basic` 控件样式，外观由样式令牌控制。
- 数值模块与规则引擎改为在启动时初始化并加载默认规则文件。
- 依赖调整：新增 Qt WebSockets / Bluetooth，移除 Qt Widgets；新增内嵌 `qrcodegen`；不再需要 Python `websockets` / `qrcode`。
- CMake 在 Windows 构建后自动调用 `windeployqt` 把 Qt 运行时部署到输出目录（原先仅在打包阶段执行）。
- QML 输入控件统一使用 `App*` 封装（`src/ui/qml/components/`），不再直接使用 Qt 原生控件；尺寸常量集中到 `ComponentStyle`。

### Removed

- **Qt Widgets 旧界面**: 移除 `DGLABClient`（含 `.ui`、`_impl`、`_utils`）与配套控件 `ThemeSelectorDialog`、`EditableLabel`、`StyledComboBox`、`SampledWaveformWidget`、`IpSelector`。
- **旧规则编辑 UI**: 移除 `FormulaBuilderDialog`、`ParentEditDialog`、`ComboBoxDelegate`、`ValueModeDelegate`（由可视化规则图取代）。
- **旧对话框**: 移除 `ModuleValuesDialog`、`LogExportSettingsDialog`（日志导出位置/保留数量/大小上限改为直接编辑 `user.json`）。
- **Python WebSocket 通信层**: 移除 `PythonSubprocessManager`、`python/Bridge.py`、`python/WebSocketCore.py`（保留 `python/PathFinder.py` 供 GSI 插件使用）。
- **qcss 主题样式表**: 移除 `qcss/` 目录及其资源、拷贝与安装规则（主题改由 `ThemeManager` 提供）。

### Fixed

- 修复 QML 主界面加载失败与 `style/` 设计令牌单例失效的问题。
- 修复浅色主题下 `textSecondary` / `textMuted` 因向白色混合导致文字几乎不可见的问题；背景与文字统一按 WCAG 对比度下限校正，`border` / `divider` 在 `surface` 与 `surfaceAlt` 上均可辨。
- 修复原生 `SpinBox` / `TextField` / `TextArea` / `ComboBox` / `CheckBox` 未跟随主题（白底黑字或黑底白字）的问题。
- 规则编辑器画布、节点卡片、端口、连线与检查器全面改用 `Theme` 令牌，对话框背景显式使用 `Theme.surface`。

## [v1.0.0] - 2026-08-26

> **变动**: [v0.6.0...v1.0.0](https://github.com/CrimsonSeraph/DG-LAB-Client/compare/v0.6.0...v1.0.0)

### Added

- **数值模块系统**: 新增 `ModuleValue`/`Module`/`ModuleManager` 数据模型与模块页面，支持周期查询调度（最小 250ms 基准轮询）与数值变化检测推送；数值支持可选最小/最大值（写入自动钳制到范围，弹窗显示当前值/最值）。
- **插件系统**: 新增 `IPlugin` 插件接口、`PLUGIN_EXPORT` 导出宏与 API 版本校验；`ModuleManager` 重构为插件宿主（扫描/加载/卸载/状态管理）；新增示例插件 `module/example` 与插件开发指南。
- **CS2 GSI 数据接入**: 新增 CS2 GSI 模块与 `CS2GsiPlugin` 插件——监听 GSI 端口接收 CS2 游戏数据，注册 23 项数值（个人状态 17 项 + 团队/地图 6 项）并实现自身/队友归属区分；配套新增 `ProcessChecker` 进程检查工具与 `PathFinder.py` 路径查找工具。
- **通用数据接收器 (DataListener)**: 将 `GsiServer` 重构为通用数据接收器，支持 TCP/UDP、来源信封分发与解析器抽象（`IDataParser`）。
- **规则引擎增强**: 规则新增启用状态、多父级（通道 A/B 与规则引用）与唯一序号；值模式支持 `{id:xxx(名称)}`/`{rule:xx}` 占位符与空值语义；实现规则间引用、级联触发与深度保护；规则页面新增启用列、父级编辑对话框与模式列灰显。
- **首页通道面板**: 首页 A/B 通道卡片新增模块信息与规则最近计算结果显示（实时刷新）。
- **日志导出**: 自动日志（分片轮转、数量/大小限制）与手动日志（导出按钮、不受限制）分离，设置持久化到 `user.json`。
- **UI 改进**: 统一下拉框控件 `StyledComboBox`、勾选框样式、表格编辑显示与首页卡片布局修复。

### Changed

- **插件宿主接口**: `IPluginHost` 新增数据接收、配置读写、基础周期与用户通知能力；`IPlugin` 增加周期变化通知。
- **CS2 GSI 插件化**: 原静态 CS2 GSI 模块改造为符合 `IPlugin` 接口的动态库插件（`module/gsi/`）。
- **规则文件格式**: 新增 `enabled` 与 `parents` 字段，兼容旧 `channel` 字段；规则按序号排序保存。
- 源码按功能分类整理（`include/`、`src/` 子目录），统一 Doxygen 中文注释与 Prettier/clang-format 格式规范。
- Windows 构建优化 Python 标准库 zip 打包（configure 耗时由数十分钟降至数秒）。

### Fixed

- 修复新版 macOS SDK 缺失 AGL.framework 导致的链接失败。
- 修复托盘图标野指针、下拉框黑色边缘/菜单重叠、表格编辑显示不全等 UI 问题。
- 修复日志清理分组、规则值模式编辑未应用、Python 日志换行与 `WebSocketCore.py` 导入缺失等问题。
- 修复数据源为空时查询返回 0 导致的显示失真、首次写入不推送 `value_changed` 等问题。

## [v0.6.0] - 2026-05-03

> **变动**: [v0.5.1...v0.6.0](https://github.com/CrimsonSeraph/DG-LAB-Client/compare/v0.5.1...v0.6.0)

### Added

- **IP 选择器** (`IpSelector`): 单例类，支持基于黑白名单关键词自动匹配可用 IPv4 地址（自动过滤虚拟网卡），并提供图形化对话框让用户编辑黑白名单并手动选择 IP。
- **可编辑标签控件** (`EditableLabel`): 继承自 `QLabel`，支持双击进入编辑模式，内嵌 `QLineEdit` 并支持任意 `QValidator` 验证器，编辑完成发出 `text_edited` 信号。
- **主题更新**: 更新全新 UI 布局与主题，所有主题样式表统一使用 `rgba` 颜色格式，遵循 6:3:1 主副点缀色比例原则。
- 为 `ThemeSelectorDialog` 添加网格卡片式主题预览（显示中文名、英文模式名及主色块），提升用户选择体验。

### Changed

- **样式系统全面翻新**: 所有 QSS 文件转换为纯 `rgba` 颜色值，并按照主色 60%、副色 30%、点缀色 10% 的比例重新调配，使界面色彩更加和谐统一。

### Deprecated

- 计划移除使用 Python 模块 `WebSocketCore.py` 实现 WebSocket 相关功能，转向使用 Qt 提供的 Qt WebSocket 库（该计划尚未完成，仍处于过渡阶段）。

### Fixed

- 修复 `IpSelector` 无法自定义选择 IP 问题。

## [v0.5.1] - 2026-04-19

> **变动**: [v0.5.0...v0.5.1](https://github.com/CrimsonSeraph/DG-LAB-Client/compare/v0.5.0...v0.5.1)

### Added

- 波形控件 `SampledWaveformWidget` 支持多监听器（多通道）实时显示。
    - 新增 `add_listener(name, color)`、`remove_listener(name)`、`set_listener_color(name, color)` 等接口。
    - 每个监听器独立采样缓冲区（环形，默认200点）和独立颜色。
    - 提供默认监听器 `"default"`（绿色），兼容旧版单曲线接口。
    - 提供为每个监听器设置输入范围 `set_input_range()` 的两个重载，经过统一化为 0~1 归一化数据输入。
- 增加最大监听器数量限制（默认16，可通过 `set_max_listeners()` 调整）。
- 所有监听器操作及数据输入均添加日志记录，便于调试。
- 为按键添加 `btn_size` 属性 `small` 用于显示小号按键。

### Changed

- 优化 `paintEvent` 绘制性能: 先拷贝监听器快照再绘制，避免长时间持有锁。
- 完善 Doxygen 注释，头文件与源文件分类注释格式规范化。
- 优化 UI 布局。

### Deprecated

- 计划移除使用 Python 模块 `WebSocketCore.py` 实现 websocket 相关功能，转向使用 Qt 提供的 Qt WebSocket 库。

## [v0.5.0] - 2026-04-19

> **变动**: [v0.4.0...v0.5.0](https://github.com/CrimsonSeraph/DG-LAB-Client/compare/v0.4.0...v0.5.0)

### Added

- 新增实时显示强度，提供手动调节、锁定调节按键。

### Deprecated

- 计划移除使用 Python 模块 `WebSocketCore.py` 实现 websocket 相关功能，转向使用 Qt 提供的 Qt WebSocket 库。

### Fixed

- 修复 Python 模块日志格式错误。

## [v0.4.0] - 2026-04-17

> **变动**: [v0.3.0...v0.4.0](https://github.com/CrimsonSeraph/DG-LAB-Client/compare/v0.3.0...v0.4.0)

### Added

- 新增 12 种主题，详细见 [qcss/README.md](qcss/README.md)。
- 样式系统再次重构: 使用 `type` 和 `theme` 属性选择器实现主题切换。
- 添加主题选择器 `ThemeSelectorDialog.h/cpp`，支持主题预览与选择。

### Changed

- 为 CMake 构建流程中 `Python 运行时和第三方包安装` 添加检查条件，防止重复打包。
- `user.json` 中关于主题的参数 `app.ui.is_light_mode` 改成 `app.ui.theme`，参数为主题英文名，采用 **全小写+下划线** 形式。

### Deprecated

- 计划移除使用 Python 模块 `WebSocketCore.py` 实现 websocket 相关功能，转向使用 Qt 提供的 Qt WebSocket 库。

### Removed

- 移除原本的主题切换按键。

## [v0.3.0] - 2026-04-16

> **变动**: [v0.2.1...v0.3.0](https://github.com/CrimsonSeraph/DG-LAB-Client/compare/v0.2.1...v0.3.0)

### Added

- 新增实时波形采样控件（`SampledWaveformWidget`），支持连续输入 0~1 归一化数据并以滚动折线图显示，可调节采样间隔和最大振幅比例。
- 样式系统全面重构: 使用 `type` 和 `mode` 属性选择器实现精细控件分类（导航按钮、操作按钮、标题、标签、输入框等），支持亮色/暗色主题一键切换。
- 新增 `apply_widget_properties()` 和 `apply_inline_styles()` 方法，统一为控件设置样式属性和内联样式，提高代码可维护性。
- 添加贡献指南 `CONTRIBUTING.md`。

### Changed

- 完善 `DGLABClient` 的样式管理逻辑，`setup_widget_properties()` 和 `change_theme()` 方法大幅优化，主题切换更流畅。
- 整理所有源文件和头文件的 `#include` 顺序，按自定义 > 第三方 > 标准库分组，提升代码规范性。
- 为多个模块添加更详细的注释，特别是配置系统、规则引擎和 Python 子进程管理部分。
- 减少冗余调试信息输出，仅保留关键状态日志，降低日志噪音。
- 样式表文件 `style_light.qcss` 和 `style_night.qcss` 完全重写，视觉效果现代化。
- 项目协议从 `MIT` 更换为 `GPL-v3.0`，完善第三方的开源信息。

### Fixed

- 修复 Linux 系统下 Python 子进程启动时路径解析错误的问题（改用 `QCoreApplication::applicationDirPath()` 拼接绝对路径）。

## [v0.2.1] - 2026-04-12

> **变动**: [v0.2.0...v0.2.1](https://github.com/CrimsonSeraph/DG-LAB-Client/compare/v0.2.0...v0.2.1)

### Changed

- 创建规则添加反馈信息。
- 减少值模式编辑下过多的调试信息输出。

### Fixed

- 统一规则编辑窗口（`添加规则` 和 `编辑规则` 触发的值模式编辑统一使用新编辑窗口）。
- 创建规则文件时检查是否包含关键字，未包含则强制包含关键字。

## [v0.2.0] - 2026-04-08

> **变动**: [v0.1.0...v0.2.0](https://github.com/CrimsonSeraph/DG-LAB-Client/compare/v0.1.0...v0.2.0)

### Added

- 规则引擎: 支持从 JSON 文件加载带 `{}` 占位符的运算规则。
- 规则表格高级编辑: 通道/模式列使用下拉框，值模式列提供可视化公式构建器（括号检查、符号插入）。
- 通过启用 Python 子进程通过 TCP 通信支持异步调用（线程池 + 信号槽）。
- GitHub Actions 自动化构建新增不包含 Python 标准运行库与第三方包的精简版本（`-without-Python`），适用于本地已有 Python 环境的用户。
- GitHub Actions 自动将更新日志发布到 GitHub Releases 页面。

### Changed

- 完善配置页面内容，添加配置文件的显示、编辑、保存等功能。
- 日志模块添加支持多个输出接收器（控制台、Qt UI）。
- 添加 Python 模块日志输出支持，日志等级与主程序同步。
- 控制台显示格式优化。
- 从 `build.yml` 移除上传构建产物到 GitHub Releases 的步骤，改为单独的 `release.yml` 处理发布流程。

### Removed

- 移除旧版内嵌 Python 解释器。

### Fixed

- 修复配置系统的加载、覆盖等问题。

## [v0.1.0] - 2026-01-10

### Added

- 初始版本（未发布版本）。
- 基础 Qt 界面（主页、配置页等）。
- 完善的配置系统，支持 JSON 文件读写、监听器通知、默认配置生成等功能。
- 完善的日志系统，支持模块化日志等级控制线程安全写入。
- 清晰的控制台输出格式以及可以限制的日志等级输出。
- 内嵌 Python 解释器执行 Python 模块。
- 用于与 DG-LAB 官方提供的 websocket 服务通讯的 Python 模块: `WebSocketCore.py`。
- GitHub Actions 自动化构建（Windows / Linux / macOS）。

### Deprecated

- 将不再使用内嵌 Python 解释器，转用 Qt 提供的方式调用 Python 子进程并通过 TCP 本地通讯。

## 其他

**变更分类**:

- `Added` – 新增功能
- `Changed` – 现有功能变更
- `Deprecated` – 标记即将移除的功能
- `Removed` – 移除功能
- `Fixed` – Bug 修复
- `Security` – 安全相关修复
