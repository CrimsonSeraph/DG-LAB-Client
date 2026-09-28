# Python 模块目录（python）

> **注意**：本目录只保留路径查找工具 `PathFinder.py`，供 CS2 GSI 插件定位游戏安装目录。原先的 Python 通信层（`Bridge.py`、`WebSocketCore.py` 与宿主侧 `PythonSubprocessManager`）已随内置中转服务上线而移除，见 [CHANGELOG.md](../CHANGELOG.md)。

---

## 文件说明

### `PathFinder.py`

**跨平台文件/目录路径查找工具**，提供统一的接口，可在 Windows、macOS 和 Linux 上查找指定的文件或目录，支持通过 Steam 库定位游戏安装路径，也支持手动指定目录搜索。

#### 主要功能

- 双模式查找：
    - `steam` 模式：自动检测 Steam 安装目录，解析所有库文件夹，查找匹配的游戏名称，并返回游戏目录或可执行文件路径。
    - `manual` 模式：支持直接指定路径（文件或目录），或在指定目录下搜索特定名称的文件/目录。
- 交互控制：
    - 可启用图形化交互（基于 `tkinter`）：
        - 多个匹配项时弹出选择对话框。
        - 未找到匹配项时可手动输入路径。
        - 当 `manual_path` 为空字符串且交互开启时，弹出系统文件/目录选择器（根据 `target_type`）。
    - 支持非交互模式（`interactive=False`），直接返回第一个匹配或 `None`。
- 日志系统：
    - 内置日志支持，级别可调（DEBUG/INFO/WARNING/ERROR/NONE），每条日志均记录模块名、函数名和级别。
- 跨平台兼容：
    - Windows：读取注册表获取 Steam 路径，并支持常见默认安装目录。
    - macOS：检测 `/Applications/Steam.app`。
    - Linux：检测 `~/.steam/steam` 或 `~/.local/share/Steam`。

#### 核心 API

类：`PathFinder`；方法：`find_path(self, mode, steam_game=None, manual_path=None, search_name=None, target_type='file', interactive=True, executable_names=None) -> Optional[str]`

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| `mode` | str | `'steam'` 或 `'manual'` |
| `steam_game` | Optional[str] | Steam 游戏名称（如 `"Counter-Strike Global Offensive"`） |
| `manual_path` | Optional[str] | 手动指定的路径（文件或目录），若为 `""` 且交互模式则弹出选择对话框 |
| `search_name` | Optional[str] | 在 `manual_path` 目录下搜索的具体文件名或目录名 |
| `target_type` | str | `'file'` 或 `'directory'`，期望返回的类型 |
| `interactive` | bool | 是否启用交互（弹窗/提示） |
| `executable_names` | Optional[List[str]] | 在 Steam 游戏目录中查找可执行文件时的名称列表（默认根据平台自动设置） |

返回值：匹配的路径字符串，若未找到且不交互则返回 `None`。

#### 命令行接口

```bash
python PathFinder.py --mode steam --steam-game "Counter-Strike Global Offensive" --type file
python PathFinder.py --mode manual --manual-path "C:\Games" --search-name "cs2.exe" --type file
python PathFinder.py --mode manual --manual-path "" --type file  # 弹出选择文件对话框
```

可选参数：`--search-name`, `--type`, `--no-interactive`, `--executable-names`, `--log-level`。

#### 作为模块使用

```python
from PathFinder import find_path   # 模块级函数（兼容旧接口）或 PathFinder 类

path = find_path(
    mode='steam',
    steam_game='Counter-Strike Global Offensive',
    target_type='file',
    interactive=False
)
if path:
    print(f"找到路径: {path}")
else:
    print("未找到")
```

#### 依赖

- Python 3.6+
- 标准库：`os`, `sys`, `platform`, `re`, `logging`, `argparse`, `typing`
- 可选：`tkinter`（图形化交互，通常随 Python 一起安装）
- 无需额外第三方库（不使用 `vdf` 解析库，内置正则解析 `libraryfolders.vdf`）

#### 注意事项

- 该模块不涉及任何网络或 WebSocket 通信，纯本地文件系统操作。
- 在非交互模式下，不会弹出任何窗口，适合在后台或自动化脚本中使用。
- 日志默认输出到控制台，可通过 `set_log_level` 调整级别。
- `PathFinder.py` 在交互模式下会尝试使用 `tkinter`，若环境不支持 `tkinter` 会自动回退到控制台交互。
