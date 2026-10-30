# 常见问题（FAQ）

按主题分组；构建、环境与验收口径见 [build.md](build.md)，功能操作见 [features.md](features.md)。

## 编译与运行

<details>
<summary><b>Q1: CMake 配置时找不到 Qt</b></summary>

**现象**:

```
Could not find a package configuration file provided by "Qt6" or "Qt5"
```

**解决方案**:

- 确保 Qt 已正确安装，并且 `CMAKE_PREFIX_PATH` 指向 Qt 的安装目录（例如 `C:/Qt/6.5.0/msvc2019_64`）。
- 或者设置环境变量 `Qt6_DIR` / `Qt5_DIR`。
- 在 Linux 上可以通过包管理器安装 Qt 开发包（如 `qt6-base-dev`），CMake 通常能自动找到。

</details>

<details>
<summary><b>Q2: 提示找不到 Python 解释器或模块</b></summary>

**现象**: 启动后日志提示找不到 Python 解释器或模块。

**解决方案**:

- 主程序自身不再运行 Python：WebSocket 通信与二维码生成均已改为应用内实现，`Bridge.py` / `WebSocketCore.py` 已移除，因此不再需要 `pip install websockets qrcode[pil]`。
- 只有 CS2 GSI 插件需要系统 Python：确认 `python`（Linux/macOS 下为 `python3`）在 PATH 中，或通过配置项 `python.path` 指定解释器路径。`python/PathFinder.py` 只依赖标准库，无需安装第三方包。 </details>

<details>
<summary><b>Q3: nlohmann/json 下载失败</b></summary>

**现象**: CMake 配置过程中报错，无法从 GitHub 下载 `json.hpp`。

**解决方案**:

- 检查网络连接，确保能够访问 `raw.githubusercontent.com`。
- 手动下载 [json.hpp](https://github.com/nlohmann/json/releases/download/v3.12.0/json.hpp) 并放入构建目录的 `/include/nlohmann/` 下（根据 CMake 输出路径调整）。
- 或者修改 `CMakeLists.txt`，改用本地已下载的头文件路径。

</details>

<details>
<summary><b>Q4: 编译时出现 C++20 相关语法错误</b></summary>

**现象**: 编译器报错如 `'std::span' is not a member of 'std'` 或要求 C++20 标准。

**解决方案**:

- 确认编译器版本: GCC ≥10、Clang ≥12、MSVC ≥2022。
- 在 CMake 配置时显式指定 C++ 标准: `-DCMAKE_CXX_STANDARD=20`。
- 若使用旧版 IDE（如 VS2019），需要升级到 VS2022 或安装支持 C++20 的工具集。

</details>

## 配置问题

<details>
<summary><b>Q5: 配置文件修改后不生效</b></summary>

**现象**: 修改了 `user.json` 中的某个值，但程序运行时使用的还是旧值。

**解决方案**:

- 确认修改的文件是正确的（注意优先级: `user.json` > `system.json` > `main.json`）。如果 `system.json` 或 `main.json` 中定义了相同路径的配置，它们会被覆盖，但不会删除。
- 重新执行 CMake 构建项目或手动将修改后的配置文件复制到构建目录的 `/config/` 下。
- 检查 JSON 格式是否正确（如多余的逗号），可以使用在线 JSON 校验工具。

</details>

<details>
<summary><b>Q6: 配置文件丢失后如何恢复？</b></summary>

**现象**: 误删了 `config/` 目录下的某个 JSON 文件，程序启动报错或使用默认值。

**解决方案**:

- 程序内置了默认配置（定义在 `DefaultConfigs.cpp`），丢失的文件会在运行时自动重新生成（前提是目录存在）。只需确保 `config/` 目录可写。
- 也可以从源码仓库中复制 `config/` 目录下的示例文件到运行目录。

</details>

<details>
<summary><b>Q7: 更改客户端所使用的端口遇到问题？</b></summary>

**更改方法**:

- 编译运行客户端后，打开 `配置` 页面，在端口中输入 **你需要更换的端口** 并点击右侧的 `确定`，若显示端口更改成功即完成更改。
- 在默认的配置文件 `system.json` 中 `app.websocket.port` 即是所用端口

> 若在项目根目录更改，请确保构建输出目录下的配置文件也应用更改！端口范围建议使用 1024~~65535 之间的数字，避免使用系统保留端口（0~~1023）。

**现象**

通过上述方式设置了端口，但应用的仍然是旧的或默认端口 `9999`。

**解决方法**

- 点击 `确定` 后根据 `错误信息` 更换端口再次点击 `确定` 尝试更改。
- 尝试重新执行 CMake 构建项目或手动自定义的 JSON 规则文件放入构建目录的 `config/` 下。

</details>

## 规则引擎问题

<details>
<summary><b>Q8: 规则文件在 UI 中不显示</b></summary>

**现象**: 将自定义的 JSON 规则文件放入 `config/rules/` 目录，但在客户端的规则页面中看不到。

**解决方案**:

- 检查文件名是否包含默认配置文件 `main.json` 中项 `rule.key` 指定的关键字（默认为 `rule`）。例如 `my_rules.json` 包含 `rule` 字样，可以被识别；`settings.json` 则不会。
- 确认规则文件的 JSON 格式正确，且根对象包含 `"rules", "version", "DGLABClient"` 键。
- 尝试重新执行 CMake 构建项目或手动自定义的 JSON 规则文件放入构建目录的 `config/rules/` 下。

</details>

<details>
<summary><b>Q9: 规则评估生成的命令不符合预期</b></summary>

**现象**: 调用 `RuleManager::evaluate_command("rule_name", args...)` 返回的 `QJsonObject` 中 `value` 字段的值不正确（如始终为 0 或未按公式计算），或者通道/模式与设置不符。

**可能原因及解决方案**:

| 原因 | 解决方法 |
| --- | --- |
| 传入的参数个数与规则中的 `{}` 占位符数量不匹配 | 检查规则模式中 `{}` 的个数，确保传入参数数量一致。若参数不足，表达式中的剩余 `{}` 不会被替换，导致求值失败（日志会输出“参数数量不匹配”错误）。 |
| 值表达式求值失败（如语法错误、除零等） | 查看日志中的 `Rule::computeValue` 错误信息，表达式错误会输出 `表达式求值失败: ...`。修正 `value_pattern` 中的表达式（例如确保运算符正确、括号匹配）。 |
| 计算结果被模式钳位规则修正 | 模式 2（“设为”）将值钳位到 [0, 200]；模式 3/4（“连减”/“连增”）钳位到 [1, 100]。如果期望的值超出范围，会被自动限制，这是正常行为。 |
| 通道无效（非 "A" 或 "B"） | `Rule::normalize_channel` 会将通道规范化为 "A"/"B"，无效通道（如 "C"、空字符串）会导致命令中使用默认通道 1（即 A）。检查规则定义中的 `channel` 字段是否为 "A" 或 "B"（大小写不敏感）。 |
| 模式值超出 0~4 范围 | 模式仅支持 0~4，若配置了其他值，行为未定义。请在规则文件中使用正确的模式编号。 |

**调试建议**:

- 启用 DEBUG 日志级别（`app.log.console_level = 0`），查看 `RuleManager` 和 `Rule` 模块输出的详细日志，包括加载的规则参数、表达式求值过程和结果。
- 使用 `RuleManager::get_rule_value_pattern("rule_name")` 获取原始表达式，手动验证计算逻辑。

</details>

## 中转服务与配对问题

<details>
<summary><b>Q10: 手机 App 无法连接到 WebSocket 服务</b></summary>

**可能原因及解决**:

| 原因 | 解决方法 |
| --- | --- |
| 手机与电脑不在同一局域网 | 确保手机连接与电脑相同的 Wi-Fi 网络 |
| 防火墙拦截了端口 | 在防火墙中放行 WebSocket 服务使用的端口（默认 9999） |
| 二维码中的地址错误 | 检查二维码中的 IP 是否为电脑的正确局域网 IP，不要使用 `127.0.0.1` 或 `localhost` |
| APP 提示无法识别二维码 | 二维码载荷必须是官方终端二维码协议格式：`https://www.dungeon-lab.com/app-download.php#DGLAB-SOCKET#ws://<局域网 IP>:<V3 端口>/<终端 ID>`，恰好两个 `#`、SOCKET 地址后不带额外路径；二维码下方那行短地址只供阅读，不参与扫码 |
| 服务未正常启动 | 查看页面状态栏与日志输出，确认中转服务已启动 |
| 某些 VPN 会拦截非代理流量 | 关闭 VPN |

</details>

<details>
<summary><b>Q11: 配对失败，提示 400 或 401</b></summary>

**错误码说明**:

| 错误码 | 说明                       |
| ------ | -------------------------- |
| 400    | 此 ID 已被其他客户端绑定   |
| 401    | 要绑定的目标客户端不存在   |
| 402    | 收信方和寄信方不是绑定关系 |
| 404    | 未找到收信人（离线）       |

**解决方法**: 检查二维码是否正确生成了 `clientId`，确保客户端与服务端的连接未中断，并确认没有重复绑定。

</details>

<details>
<summary><b>Q12: 消息发送失败，提示 405</b></summary>

**原因**: JSON 消息长度超过了 1950 字符，APP 会丢弃该消息。

**解决方法**: 将消息拆分为多条发送，确保每条消息长度在 1950 字符以内。

</details>

## 通用问题

<details>
<summary><b>Q13: 日志在 Qt 界面不显示或显示等级不对</b></summary>

**现象**: 程序运行时，左侧「日志」页的列表没有输出，或者只显示 ERROR 级别，而配置中设置了 INFO。

**解决方案**:

- 先看「日志」页右上角的「级别」下拉是否被调到了较高级别（它直接调用 `DebugLog::set_log_sink_level("qt_ui", level)` 并写回配置）。
- 检查配置文件中的 `app.log.ui_log_level` 值（0=DEBUG,1=INFO,2=WARN,3=ERROR,4=NONE）。例如需要显示 INFO 及以上，应设置为 1。
- 确认没有在代码中动态修改 UI 日志的 sink 级别（`DebugLog::set_log_sink_level("qt_ui", level)`）。
- 如果使用 `LOG_MODULE` 宏，确保模块名称和函数名称正确，且日志等级不低于全局设置。

</details>

<details>
<summary><b>Q14: Windows 调试控制台不出现</b></summary>

**现象**: 在配置文件中设置了 `"debug": true`，但程序启动时没有弹出黑色控制台窗口。

**解决方案**:

- 仅当编译为 Debug 配置且 `app.debug` 为 true 时才会创建控制台。Release 模式下可能被优化掉。
- 检查是否在 Windows 平台（该功能依赖 `<windows.h>`，Linux/macOS 无效）。
- 尝试以管理员身份运行程序。

</details>

<details>
<summary><b>Q15: IP 自动选择得到 127.0.0.1，但明明有真实网卡</b></summary>

**现象**: 自动选择的 IP 是 `127.0.0.1`，而电脑实际有有效的局域网 IP。

**解决方案**:

- 自动选择只取第一个「已启用且非回环」网卡的 IPv4 地址，不区分虚拟网卡（VMware / VirtualBox / Docker 等）与物理网卡。若选到的地址手机访问不到，请在“配置”页面的“连接”卡片中手动填写局域网 IP。
- 如果没有任何可用网卡，自动选择会回退到 `127.0.0.1`，此时手机无法扫码连接。

</details>

## 更新问题

<details>
<summary><b>Q16: 检查更新失败或下载更新包失败</b></summary>

**现象**: 点击"检查更新"后提示网络错误，或下载过程中失败。

**可能原因及解决**:

| 原因 | 解决方法 |
| --- | --- |
| 无法访问 GitHub | 确认网络可访问 `github.com` 与 `raw.githubusercontent.com`；如需代理，请为系统或 Qt 配置代理 |
| `manifest.json` 不存在 | 仅正式 Release 附带 `manifest.json`；预发布或开发构建无此文件，检查更新会失败，属正常 |
| SHA256 校验失败 | 下载文件损坏或被中间人篡改；删除临时文件后重试 |
| 当前平台无更新包 | manifest 的 `platforms` 中缺少对应平台条目，等待发布方补齐 |

</details>

<details>
<summary><b>Q17: 更新后程序未能自动重启或回滚</b></summary>

**现象**: 安装完成后程序没有自动拉起，或新版本启动失败但没有回滚。

**解决方案**:

- Windows：NSIS 安装包以 `/S` 静默执行；若安装目录无写权限，安装会失败。请以有权限的账户运行，或把程序安装到用户可写目录。
- macOS / Linux：更新脚本会先备份旧版本，新版本启动后 3 秒内写入、脚本 5 秒后检查（`--updated` 参数），脚本据此决定保留新版还是回滚。若新版本在 5 秒内崩溃，脚本会自动回滚并重新打开旧版。
- 检查临时目录下的 `dglab_update.bat` / `dglab_update.sh` 与日志，确认脚本是否被执行。
- 若回滚也失败，手动从备份文件（`.bak`）恢复。

</details>
