"""
    Copyright (c) 2026 CrimsonSeraph(ltyy.leoyu@gmail.com)
    SPDX-License-Identifier: GPL-3.0-only
"""

import os
import sys
import platform
import re
import logging
import argparse
from typing import Optional, List, Union

LOG_LEVEL = logging.INFO
logging.basicConfig(
    level=LOG_LEVEL,
    format="%(asctime)s - %(name)s - %(levelname)s - %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S"
)
logger = logging.getLogger(__name__)

def set_log_level(level: Union[int, str]):
    """设置日志级别，支持 'DEBUG','INFO','WARNING','ERROR','NONE' 或 logging 常量"""
    global LOG_LEVEL
    if isinstance(level, str):
        level = level.upper()
        if level == "NONE":
            level = logging.CRITICAL + 1  # 高于CRITICAL，几乎不输出
        else:
            level = getattr(logging, level, logging.INFO)
    LOG_LEVEL = level
    logger.setLevel(level)
    logger.debug(f"[{__name__}] <set_log_level> (DEBUG): 日志级别已设置为 {level}")

# ---------- 核心类 PathFinder ----------
class PathFinder:
    """路径查找器，封装所有查找逻辑"""

    @staticmethod
    def get_steam_install_path() -> Optional[str]:
        """返回Steam安装根目录，若无则返回None"""
        func_name = "get_steam_install_path"
        logger.debug(f"[{__name__}] <{func_name}> (DEBUG): 开始检测Steam安装路径")
        system = platform.system()
        logger.debug(f"[{__name__}] <{func_name}> (DEBUG): 当前系统 {system}")
        if system == "Windows":
            try:
                import winreg
                key = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE,
                                     r"SOFTWARE\WOW6432Node\Valve\Steam")
                path, _ = winreg.QueryValueEx(key, "InstallPath")
                logger.info(f"[{__name__}] <{func_name}> (INFO): 从注册表读取Steam路径: {path}")
                return path
            except Exception as e:
                logger.warning(f"[{__name__}] <{func_name}> (WARNING): 注册表读取失败: {e}")
                # 常见默认路径备选
                for base in [os.environ.get("ProgramFiles(x86)"), "C:\\Program Files (x86)"]:
                    if base:
                        p = os.path.join(base, "Steam")
                        if os.path.exists(p):
                            logger.info(f"[{__name__}] <{func_name}> (INFO): 使用默认路径 {p}")
                            return p
                logger.warning(f"[{__name__}] <{func_name}> (WARNING): 未找到Steam安装")
                return None
        elif system == "Darwin":  # macOS
            p = "/Applications/Steam.app"
            if os.path.exists(p):
                logger.info(f"[{__name__}] <{func_name}> (INFO): 找到Steam: {p}")
                return p
            else:
                logger.warning(f"[{__name__}] <{func_name}> (WARNING): 未找到 /Applications/Steam.app")
                return None
        else:  # Linux
            for p in [os.path.expanduser("~/.steam/steam"),
                      os.path.expanduser("~/.local/share/Steam")]:
                if os.path.exists(p):
                    logger.info(f"[{__name__}] <{func_name}> (INFO): 找到Steam: {p}")
                    return p
            logger.warning(f"[{__name__}] <{func_name}> (WARNING): 未找到Steam路径")
            return None

    @staticmethod
    def get_steam_library_folders(steam_path: str) -> List[str]:
        """解析 libraryfolders.vdf，返回所有库目录列表（包含主库）"""
        func_name = "get_steam_library_folders"
        logger.debug(f"[{__name__}] <{func_name}> (DEBUG): 解析Steam库目录，主库: {steam_path}")
        libraries = [steam_path]
        vdf_path = os.path.join(steam_path, "steamapps", "libraryfolders.vdf")
        if not os.path.exists(vdf_path):
            logger.warning(f"[{__name__}] <{func_name}> (WARNING): 未找到 libraryfolders.vdf，仅使用主库")
            return libraries

        try:
            with open(vdf_path, 'r', encoding='utf-8') as f:
                content = f.read()
            # 匹配 "path" "xxx"
            pattern = r'"path"\s*"([^"]+)"'
            matches = re.findall(pattern, content)
            for m in matches:
                path = m.replace('\\\\', '\\')
                if os.path.exists(path):
                    libraries.append(path)
                    logger.debug(f"[{__name__}] <{func_name}> (DEBUG): 添加库目录: {path}")
            logger.info(f"[{__name__}] <{func_name}> (INFO): 共找到 {len(libraries)-1} 个附加库")
        except Exception as e:
            logger.error(f"[{__name__}] <{func_name}> (ERROR): 解析库文件失败: {e}")
        return libraries

    @staticmethod
    def find_steam_game_path(steam_paths: List[str], game_name: str) -> List[str]:
        """在Steam库中查找游戏目录（返回所有匹配的目录路径）"""
        func_name = "find_steam_game_path"
        logger.debug(f"[{__name__}] <{func_name}> (DEBUG): 查找游戏 {game_name}")
        matches = []
        for lib in steam_paths:
            common = os.path.join(lib, "steamapps", "common")
            if not os.path.exists(common):
                continue
            for item in os.listdir(common):
                if item.lower() == game_name.lower():
                    full = os.path.join(common, item)
                    if os.path.isdir(full):
                        matches.append(full)
                        logger.debug(f"[{__name__}] <{func_name}> (DEBUG): 找到匹配游戏目录: {full}")
        logger.info(f"[{__name__}] <{func_name}> (INFO): 游戏 '{game_name}' 匹配数: {len(matches)}")
        return matches

    @staticmethod
    def find_executable_in_game_dir(game_dir: str, executable_names: List[str]) -> Optional[str]:
        """在游戏目录下查找给定名称列表中的第一个可执行文件"""
        func_name = "find_executable_in_game_dir"
        logger.debug(f"[{__name__}] <{func_name}> (DEBUG): 在 {game_dir} 中查找可执行文件 {executable_names}")
        for root, dirs, files in os.walk(game_dir):
            depth = root[len(game_dir):].count(os.sep)
            if depth > 2:
                continue
            for f in files:
                if f in executable_names or any(f.lower() == name.lower() for name in executable_names):
                    exe_path = os.path.join(root, f)
                    logger.info(f"[{__name__}] <{func_name}> (INFO): 找到可执行文件: {exe_path}")
                    return exe_path
        logger.warning(f"[{__name__}] <{func_name}> (WARNING): 未在 {game_dir} 中找到可执行文件")
        return None

    @staticmethod
    def search_in_directory(base_dir: str, search_name: str, target_type: str) -> List[str]:
        """在指定目录下搜索文件或目录，返回所有匹配的完整路径"""
        func_name = "search_in_directory"
        logger.debug(f"[{__name__}] <{func_name}> (DEBUG): 在 {base_dir} 中搜索 {target_type} 名称 {search_name}")
        matches = []
        if not os.path.exists(base_dir) or not os.path.isdir(base_dir):
            logger.warning(f"[{__name__}] <{func_name}> (WARNING): 基础目录不存在或不是目录: {base_dir}")
            return matches
        for item in os.listdir(base_dir):
            full = os.path.join(base_dir, item)
            if target_type == 'file' and os.path.isfile(full) and item == search_name:
                matches.append(full)
            elif target_type == 'directory' and os.path.isdir(full) and item == search_name:
                matches.append(full)
        logger.debug(f"[{__name__}] <{func_name}> (DEBUG): 找到 {len(matches)} 个匹配项")
        return matches

    @staticmethod
    def interactive_select(options: List[str], prompt: str = "请选择一个匹配项：") -> Optional[str]:
        """弹出选择对话框或控制台选择"""
        func_name = "interactive_select"
        logger.debug(f"[{__name__}] <{func_name}> (DEBUG): 交互选择，选项数 {len(options)}")
        if not options:
            return None
        if len(options) == 1:
            logger.debug(f"[{__name__}] <{func_name}> (DEBUG): 唯一选项，直接返回")
            return options[0]

        # 尝试使用tkinter（GUI）
        try:
            import tkinter as tk
            from tkinter import simpledialog
            root = tk.Tk()
            root.withdraw()
            choices = [f"{i+1}: {p}" for i, p in enumerate(options)]
            choice_str = "\n".join(choices)
            idx = simpledialog.askinteger("选择路径",
                                          f"{prompt}\n\n{choice_str}\n请输入编号：",
                                          minvalue=1, maxvalue=len(options))
            root.destroy()
            if idx is not None:
                selected = options[idx-1]
                logger.info(f"[{__name__}] <{func_name}> (INFO): 用户选择: {selected}")
                return selected
            else:
                logger.info(f"[{__name__}] <{func_name}> (INFO): 用户取消选择")
                return None
        except Exception as e:
            logger.warning(f"[{__name__}] <{func_name}> (WARNING): GUI不可用，回退控制台: {e}")
            # 回退到控制台
            print(prompt)
            for i, p in enumerate(options):
                print(f"{i+1}: {p}")
            while True:
                try:
                    sel = input("请输入编号（或直接输入路径）：").strip()
                    if sel.isdigit():
                        idx = int(sel) - 1
                        if 0 <= idx < len(options):
                            selected = options[idx]
                            logger.info(f"[{__name__}] <{func_name}> (INFO): 用户选择: {selected}")
                            return selected
                    else:
                        if os.path.exists(sel):
                            logger.info(f"[{__name__}] <{func_name}> (INFO): 用户输入路径: {sel}")
                            return sel
                        else:
                            print("无效输入，请重新选择。")
                except KeyboardInterrupt:
                    logger.debug(f"[{__name__}] <{func_name}> (DEBUG): 用户中断")
                    return None
                except:
                    continue

    @staticmethod
    def interactive_input(prompt: str = "请输入路径：") -> Optional[str]:
        """弹窗或控制台输入路径"""
        func_name = "interactive_input"
        logger.debug(f"[{__name__}] <{func_name}> (DEBUG): 交互输入，提示: {prompt}")
        try:
            import tkinter as tk
            from tkinter import simpledialog
            root = tk.Tk()
            root.withdraw()
            path = simpledialog.askstring("输入路径", prompt)
            root.destroy()
            if path is not None:
                logger.info(f"[{__name__}] <{func_name}> (INFO): 用户输入: {path}")
            else:
                logger.info(f"[{__name__}] <{func_name}> (INFO): 用户取消输入")
            return path
        except:
            path = input(prompt).strip()
            logger.info(f"[{__name__}] <{func_name}> (INFO): 用户输入: {path}")
            return path

    @staticmethod
    def _select_path_via_dialog(target_type: str) -> Optional[str]:
        """弹出系统对话框让用户选择文件或目录（根据target_type）"""
        func_name = "_select_path_via_dialog"
        logger.debug(f"[{__name__}] <{func_name}> (DEBUG): 弹出选择对话框，类型={target_type}")
        try:
            import tkinter as tk
            from tkinter import filedialog
            root = tk.Tk()
            root.withdraw()
            if target_type == 'file':
                path = filedialog.askopenfilename(title="选择文件")
            else:
                path = filedialog.askdirectory(title="选择目录")
            root.destroy()
            if path:
                logger.info(f"[{__name__}] <{func_name}> (INFO): 用户选择: {path}")
            else:
                logger.info(f"[{__name__}] <{func_name}> (INFO): 用户取消选择")
            return path if path else None
        except Exception as e:
            logger.warning(f"[{__name__}] <{func_name}> (WARNING): 对话框打开失败: {e}")
            return None

    def find_path(
        self,
        mode: str,               # 'steam' 或 'manual'
        steam_game: Optional[str] = None,
        manual_path: Optional[str] = None,
        search_name: Optional[str] = None,
        target_type: str = 'file',   # 'file' 或 'directory'
        interactive: bool = True,
        executable_names: Optional[List[str]] = None
    ) -> Optional[str]:
        """
        查找指定路径并返回

        参数:
            mode: 'steam' 或 'manual'
            steam_game: 当 mode='steam' 时，游戏名称（如 "Counter-Strike Global Offensive"）
            manual_path: 当 mode='manual' 时，用户指定的路径（文件或目录）
            search_name: 当 manual_path 是目录且需要在该目录下搜索时，要查找的文件/目录名
            target_type: 'file' 或 'directory'，表示期望返回的是文件还是目录
            interactive: 是否启用交互（多选时弹窗，无匹配时输入）
            executable_names: 当 mode='steam' 且 target_type='file' 时，可执行文件名称列表（默认根据平台自动猜测）

        返回:
            匹配的路径字符串，若未找到且不交互则返回 None
        """
        func_name = "find_path"
        logger.info(f"[{__name__}] <{func_name}> (INFO): 开始查找，mode={mode}, target_type={target_type}, interactive={interactive}")
        logger.debug(f"[{__name__}] <{func_name}> (DEBUG): steam_game={steam_game}, manual_path={manual_path}, search_name={search_name}")

        # ---------- 新增：如果 manual_path 为空字符串且 interactive 为 True，弹出选择对话框 ----------
        if mode == 'manual' and manual_path == "" and interactive:
            selected = self._select_path_via_dialog(target_type)
            if selected:
                manual_path = selected
                logger.info(f"[{__name__}] <{func_name}> (INFO): 通过对话框获取路径: {manual_path}")
            else:
                logger.warning(f"[{__name__}] <{func_name}> (WARNING): 用户取消选择，返回None")
                return None

        if executable_names is None:
            system = platform.system()
            if system == "Windows":
                executable_names = ["cs2.exe", "csgo.exe"]
            elif system == "Darwin":
                executable_names = ["cs2", "csgo"]
            else:  # Linux
                executable_names = ["cs2", "csgo"]
            logger.debug(f"[{__name__}] <{func_name}> (DEBUG): 默认可执行文件名: {executable_names}")

        candidates = []

        if mode == 'steam':
            if not steam_game:
                logger.error(f"[{__name__}] <{func_name}> (ERROR): Steam模式下缺少 steam_game 参数")
                raise ValueError("Steam模式下必须提供 steam_game 参数")
            steam_install = self.get_steam_install_path()
            if not steam_install:
                if interactive:
                    path = self.interactive_input("未找到Steam安装路径，请手动输入Steam根目录：")
                    if path and os.path.exists(path):
                        steam_install = path
                        logger.info(f"[{__name__}] <{func_name}> (INFO): 用户手动指定Steam路径: {steam_install}")
                    else:
                        logger.warning(f"[{__name__}] <{func_name}> (WARNING): 未提供有效Steam路径")
                        return None
                else:
                    logger.error(f"[{__name__}] <{func_name}> (ERROR): 未找到Steam且非交互模式，无法继续")
                    return None
            libs = self.get_steam_library_folders(steam_install)
            game_dirs = self.find_steam_game_path(libs, steam_game)

            if target_type == 'directory':
                candidates = game_dirs
                logger.debug(f"[{__name__}] <{func_name}> (DEBUG): 目标为目录，候选数 {len(candidates)}")
            else:  # file
                for gd in game_dirs:
                    exe = self.find_executable_in_game_dir(gd, executable_names)
                    if exe:
                        candidates.append(exe)
                logger.debug(f"[{__name__}] <{func_name}> (DEBUG): 目标为文件，候选数 {len(candidates)}")
        else:
            # manual 模式
            if not manual_path:
                logger.error(f"[{__name__}] <{func_name}> (ERROR): 手动模式下缺少 manual_path 参数")
                raise ValueError("手动模式下必须提供 manual_path 参数")

            if os.path.exists(manual_path):
                if os.path.isfile(manual_path):
                    logger.debug(f"[{__name__}] <{func_name}> (DEBUG): manual_path 是文件: {manual_path}")
                    if target_type == 'file':
                        candidates = [manual_path]
                    else:
                        logger.debug(f"[{__name__}] <{func_name}> (DEBUG): 目标为目录但给了文件，返回父目录")
                        if interactive:
                            choice = self.interactive_select(
                                [os.path.dirname(manual_path)],
                                "您指定的是文件，但目标类型为目录，是否返回其父目录？")
                            if choice:
                                candidates = [choice]
                        else:
                            candidates = [os.path.dirname(manual_path)]
                elif os.path.isdir(manual_path):
                    logger.debug(f"[{__name__}] <{func_name}> (DEBUG): manual_path 是目录: {manual_path}")
                    if search_name:
                        found = self.search_in_directory(manual_path, search_name, target_type)
                        candidates = found
                    else:
                        if target_type == 'directory':
                            candidates = [manual_path]
                        else:
                            # 尝试查找可执行文件
                            if executable_names:
                                for name in executable_names:
                                    found = self.search_in_directory(manual_path, name, 'file')
                                    if found:
                                        candidates.extend(found)
                            if not candidates and interactive:
                                inp = self.interactive_input(f"在目录 '{manual_path}' 中未找到指定文件，请输入文件路径：")
                                if inp and os.path.exists(inp):
                                    candidates = [inp]
            else:
                logger.warning(f"[{__name__}] <{func_name}> (WARNING): manual_path 不存在: {manual_path}")
                if interactive:
                    inp = self.interactive_input(f"路径 '{manual_path}' 不存在，请重新输入：")
                    if inp and os.path.exists(inp):
                        # 递归调用但关闭交互避免死循环
                        logger.info(f"[{__name__}] <{func_name}> (INFO): 用户重新输入路径: {inp}")
                        return self.find_path(mode='manual', manual_path=inp, search_name=search_name,
                                              target_type=target_type, interactive=False,
                                              executable_names=executable_names)
                    else:
                        return None
                else:
                    return None

        # 处理候选结果
        if not candidates:
            logger.warning(f"[{__name__}] <{func_name}> (WARNING): 未找到任何匹配项")
            if interactive:
                inp = self.interactive_input("未找到匹配项，请手动输入路径：")
                if inp and os.path.exists(inp):
                    if target_type == 'file' and os.path.isfile(inp):
                        logger.info(f"[{__name__}] <{func_name}> (INFO): 用户手动指定文件: {inp}")
                        return inp
                    elif target_type == 'directory' and os.path.isdir(inp):
                        logger.info(f"[{__name__}] <{func_name}> (INFO): 用户手动指定目录: {inp}")
                        return inp
                    else:
                        logger.warning(f"[{__name__}] <{func_name}> (WARNING): 用户输入路径类型与期望不符")
                        return None
                else:
                    return None
            else:
                return None

        # 去重
        candidates = list(dict.fromkeys(candidates))
        logger.debug(f"[{__name__}] <{func_name}> (DEBUG): 去重后候选数 {len(candidates)}")
        if len(candidates) == 1:
            result = candidates[0]
            logger.info(f"[{__name__}] <{func_name}> (INFO): 返回结果: {result}")
            return result
        else:
            if interactive:
                selected = self.interactive_select(candidates, "找到多个匹配项，请选择：")
                if selected:
                    logger.info(f"[{__name__}] <{func_name}> (INFO): 用户选择: {selected}")
                else:
                    logger.info(f"[{__name__}] <{func_name}> (INFO): 用户取消，返回None")
                return selected
            else:
                logger.info(f"[{__name__}] <{func_name}> (INFO): 非交互模式，返回第一个: {candidates[0]}")
                return candidates[0]


# ---------- 保留原有模块级函数作为兼容接口（内部调用类方法） ----------
# 为了向后兼容，每个函数都实例化一个 PathFinder 对象并调用对应方法

def get_steam_install_path() -> Optional[str]:
    return PathFinder.get_steam_install_path()

def get_steam_library_folders(steam_path: str) -> List[str]:
    return PathFinder.get_steam_library_folders(steam_path)

def find_steam_game_path(steam_paths: List[str], game_name: str) -> List[str]:
    return PathFinder.find_steam_game_path(steam_paths, game_name)

def find_executable_in_game_dir(game_dir: str, executable_names: List[str]) -> Optional[str]:
    return PathFinder.find_executable_in_game_dir(game_dir, executable_names)

def search_in_directory(base_dir: str, search_name: str, target_type: str) -> List[str]:
    return PathFinder.search_in_directory(base_dir, search_name, target_type)

def interactive_select(options: List[str], prompt: str = "请选择一个匹配项：") -> Optional[str]:
    return PathFinder.interactive_select(options, prompt)

def interactive_input(prompt: str = "请输入路径：") -> Optional[str]:
    return PathFinder.interactive_input(prompt)

def find_path(
    mode: str,
    steam_game: Optional[str] = None,
    manual_path: Optional[str] = None,
    search_name: Optional[str] = None,
    target_type: str = 'file',
    interactive: bool = True,
    executable_names: Optional[List[str]] = None
) -> Optional[str]:
    finder = PathFinder()
    return finder.find_path(mode, steam_game, manual_path, search_name, target_type, interactive, executable_names)


# ---------- 命令行入口 ----------
def main():
    parser = argparse.ArgumentParser(description="查找文件或目录路径（带日志）")
    parser.add_argument("--mode", choices=['steam', 'manual'], required=True,
                        help="查找模式")
    parser.add_argument("--steam-game", help="Steam游戏名称（mode=steam时使用）")
    parser.add_argument("--manual-path", help="手动指定路径（mode=manual时使用），若为空字符串且交互开启，会弹出选择对话框")
    parser.add_argument("--search-name", help="在manual-path目录内搜索的名称")
    parser.add_argument("--type", choices=['file', 'directory'], default='file',
                        help="期望返回的类型")
    parser.add_argument("--no-interactive", action="store_true",
                        help="禁用交互（不弹窗/不提示，直接返回第一个匹配或None）")
    parser.add_argument("--executable-names", nargs='*', default=[],
                        help="Steam模式下查找可执行文件的名称列表，如 cs2.exe")
    parser.add_argument("--log-level", choices=['DEBUG','INFO','WARNING','ERROR','NONE'],
                        default='INFO', help="设置日志级别")
    args = parser.parse_args()

    # 设置日志级别
    set_log_level(args.log_level)

    interactive = not args.no_interactive
    exe_names = args.executable_names if args.executable_names else None

    # 如果 manual-path 显式传入空字符串，argparse 会将其解析为 ''，但如果是未指定则为 None
    # 为了支持空字符串触发对话框，我们传递 args.manual_path 可能为 None 或 ''
    # 在 find_path 内部处理 manual_path == "" 且 interactive 的情况
    result = find_path(
        mode=args.mode,
        steam_game=args.steam_game,
        manual_path=args.manual_path if args.manual_path is not None else None,
        search_name=args.search_name,
        target_type=args.type,
        interactive=interactive,
        executable_names=exe_names
    )

    if result:
        print(result)
        sys.exit(0)
    else:
        sys.exit(1)

if __name__ == "__main__":
    main()
