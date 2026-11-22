# Python 模块目录（python）

本目录存放插件附带的 Python 脚本，随程序一起安装（构建时整目录复制到 `<程序目录>/python/`）。宿主提供调用能力，当前没有内置脚本。

## 目录约定

- 脚本按功能命名，文件名用 `PascalCase`，例如 `SomeTool.py`
- 只用标准库，不引入第三方包；确需扩展时由 `python/Lib/site-packages` 承载，脚本自身不写安装逻辑
- 脚本不自行弹窗、不读交互输入，输入经命令行参数传入，结果写标准输出

## 宿主提供的调用能力

- 解释器路径来自 `user.json` 的 `python.path`（缺省 `python`），由宿主 `AppConfig` 读取
- 插件通过插件 SDK 的 `dglab_run_python`（`include/plugin/PythonRunner.h`）运行脚本：传入解释器、脚本路径、参数串与超时，回收标准输出与标准错误，返回 `DGLAB_PYTHON_*` 码
- 脚本在磁盘上的位置以程序目录为基准，即 `<程序目录>/python/<脚本名>.py`

## 当前状态

本目录没有内置脚本。原先的 `PathFinder.py`（Steam 目录查找）已由 C++ 实现取代，改为插件 SDK 的 `dglab_find_game_dir`（`include/plugin/PathFinder.h`，源码在 `src/plugin/`），CS2 GSI 插件不再需要 Python 解释器。
