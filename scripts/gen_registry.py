#!/usr/bin/env python3
# 扫描插件仓库的全部 Release，生成注册表 registry.json
#
# registry.json 是宿主端插件市场与分发链路的唯一数据源，字段名不可改动：
#   schemaVersion / generatedAt / plugins[] 与 variants[] 内的 id name version author
#   description tags banner dependencies platform url sha256 size
#
# plugins[] 另有一个可选字段 sourceCommit：记录生成本份清单的主仓库提交，用于溯源；
# 缺省不写，schemaVersion 仍是 1（可选字段不是破坏性变更）
#
# 用法：
#   # 真实模式：读取插件仓库全部 Release，并把封面镜像进 Worker 的静态资源目录
#   python scripts/gen_registry.py --repo CrimsonSeraph/DG-LAB-Client-Plugin \
#       --modules-dir module \
#       --banner-dir deploy/cloudflare/public/banners \
#       --banner-base-url https://dglab-registry.crimsonseraph.top \
#       --out deploy/cloudflare/public/registry.json
#
#   # 自检模式：用内置样例数据离线跑通，不访问网络
#   python scripts/gen_registry.py --self-test
#
# 封面：插件仓库 banners/ 仍是唯一真源。本脚本把 plugins[].banner 指向的图片抓取到
# --banner-dir，并把字段改写成 <banner-base-url>/banners/<文件名>，对客户端只暴露 Worker 域名
#
# 只用 Python 3 标准库，不引入第三方依赖
"""生成插件注册表 registry.json"""

from __future__ import annotations

import argparse
import datetime as _dt
import hashlib
import http.client
import json
import os
import pathlib
import re
import sys
import tempfile
import urllib.error
import urllib.parse
import urllib.request
from typing import Any, Callable, Dict, List, Mapping, Optional, Sequence, Tuple

SCHEMA_VERSION = 1

# platform 取值集合固定，宿主端按同一份契约解析
PLATFORMS: Tuple[str, ...] = (
    "windows-x64",
    "windows-arm64",
    "linux-x64",
    "linux-arm64",
    "macos-x64",
    "macos-arm64",
)

PLUGIN_FIELDS = (
    "id",
    "name",
    "version",
    "author",
    "description",
    "tags",
    "banner",
    "dependencies",
    "variants",
)

VARIANT_FIELDS = ("platform", "url", "sha256", "size")

# plugins[] 的可选溯源字段，排在契约字段之后
SOURCE_COMMIT_FIELD = "sourceCommit"
PLUGIN_FIELDS_WITH_SOURCE_COMMIT = PLUGIN_FIELDS + (SOURCE_COMMIT_FIELD,)
COMMIT_RE = re.compile(r"^[0-9a-f]{7,40}$")

# 插件 tag 形如 warthunder-v0.1.0
TAG_RE = re.compile(r"^(?P<plugin>[A-Za-z0-9][A-Za-z0-9_-]*)-v(?P<version>\d+\.\d+\.\d+)$")
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
GENERATED_AT_RE = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$")

API_ROOT = "https://api.github.com"
USER_AGENT = "dg-lab-client-gen-registry"
CHUNK_SIZE = 1 << 20

# 封面对外的唯一下载基地址：registry Worker 的自定义域名
DEFAULT_BANNER_BASE_URL = "https://dglab-registry.crimsonseraph.top"

# 封面文件名白名单：不允许路径分隔符，也不允许除了图片后缀之外的形态
BANNER_NAME_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]*\.(jpg|jpeg|png|webp|gif|avif)$", re.IGNORECASE)


class RegistryError(RuntimeError):
    """可预期的失败，main 捕获后打印成一行中文错误，不抛栈"""


def _warn(message: str) -> None:
    print(f"警告：{message}", file=sys.stderr)


def _info(message: str) -> None:
    print(f"提示：{message}", file=sys.stderr)


def utc_timestamp() -> str:
    return _dt.datetime.now(_dt.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def parse_tag(tag: str) -> Optional[Tuple[str, str]]:
    match = TAG_RE.match(tag)
    if match is None:
        return None
    return match.group("plugin"), match.group("version")


def parse_version(version: str) -> Tuple[int, int, int]:
    parts = version.split(".")
    if len(parts) != 3 or not all(part.isdigit() for part in parts):
        raise RegistryError(f"版本号不是 主.次.修订 形式：{version}")
    return int(parts[0]), int(parts[1]), int(parts[2])


def variant_platform(plugin: str, version: str, asset_name: str) -> Optional[str]:
    """按 <plugin>-<version>-<platform>.zip 命名解析平台，不符合契约命名时返回 None"""
    prefix = f"{plugin}-{version}-"
    if not asset_name.startswith(prefix) or not asset_name.endswith(".zip"):
        return None
    platform = asset_name[len(prefix) : -len(".zip")]
    return platform if platform in PLATFORMS else None


def load_plugin_metadata(modules_dir: str) -> Dict[str, Dict[str, Any]]:
    """读取 module/<插件>/meta/plugin.json，得到契约需要的展示字段"""
    root = pathlib.Path(modules_dir)
    if not root.is_dir():
        raise RegistryError(f"插件源码目录不存在：{root}")

    metadata: Dict[str, Dict[str, Any]] = {}
    for meta_path in sorted(root.glob("*/meta/plugin.json")):
        try:
            data = json.loads(meta_path.read_text(encoding="utf-8-sig"))
        except (OSError, ValueError) as exc:
            _warn(f"跳过无法解析的 {meta_path}：{exc}")
            continue
        if not isinstance(data, dict):
            _warn(f"跳过格式不是对象的 {meta_path}")
            continue

        fallback_id = meta_path.parent.parent.name
        plugin_id = str(data.get("id") or fallback_id).strip()
        if not plugin_id:
            _warn(f"跳过缺少 id 的 {meta_path}")
            continue
        if plugin_id != fallback_id:
            _warn(f"{meta_path} 的 id 是 {plugin_id}，与目录名 {fallback_id} 不一致，以 id 为准")

        description = str(data.get("description") or "").strip()
        # 注册表只需要一句话简介，多行描述取首行
        description = description.splitlines()[0].strip() if description else ""

        metadata[plugin_id] = {
            "name": str(data.get("name") or plugin_id).strip(),
            "author": str(data.get("author") or "").strip(),
            "description": description,
            "tags": [str(tag) for tag in data.get("tags") or []],
            "banner": str(data.get("banner") or "").strip(),
            "dependencies": [str(dep) for dep in data.get("dependencies") or []],
        }
    return metadata


def _open(url: str, token: Optional[str], accept: str):
    headers = {"User-Agent": USER_AGENT, "Accept": accept}
    if token:
        headers["Authorization"] = f"Bearer {token}"
    return urllib.request.urlopen(urllib.request.Request(url, headers=headers), timeout=60)


def fetch_releases(repo: str, token: Optional[str]) -> List[Dict[str, Any]]:
    releases: List[Dict[str, Any]] = []
    page = 1
    while True:
        url = f"{API_ROOT}/repos/{repo}/releases?per_page=100&page={page}"
        try:
            with _open(url, token, "application/vnd.github+json") as response:
                batch = json.loads(response.read().decode("utf-8"))
        except urllib.error.HTTPError as exc:
            if exc.code == 404:
                raise RegistryError(f"仓库不存在或 token 无权限：{repo}") from exc
            if exc.code in (401, 403):
                raise RegistryError(
                    f"GitHub API 拒绝访问（{exc.code}）：检查 token 是否有效、是否被限流"
                ) from exc
            raise RegistryError(f"读取 {repo} 的 Release 失败：HTTP {exc.code}") from exc
        except urllib.error.URLError as exc:
            raise RegistryError(f"无法访问 GitHub API（网络不可用？）：{exc.reason}") from exc

        if not isinstance(batch, list):
            raise RegistryError(f"GitHub API 返回的 Release 列表格式异常：{repo}")
        releases.extend(item for item in batch if isinstance(item, dict))
        if len(batch) < 100:
            return releases
        page += 1


def _download_sha256(url: str, token: Optional[str]) -> Tuple[str, int]:
    digest = hashlib.sha256()
    size = 0
    try:
        with _open(url, token, "application/octet-stream") as response:
            while True:
                chunk = response.read(CHUNK_SIZE)
                if not chunk:
                    break
                digest.update(chunk)
                size += len(chunk)
    except urllib.error.HTTPError as exc:
        raise RegistryError(f"下载资产失败：HTTP {exc.code} {url}") from exc
    except urllib.error.URLError as exc:
        raise RegistryError(f"下载资产失败（网络不可用？）：{exc.reason} {url}") from exc
    return digest.hexdigest(), size

BannerFetcher = Callable[[str], bytes]


def banner_filename(url: str) -> str:
    """从封面地址解析出白名单化的文件名；路径分隔符、.. 与奇怪后缀一律拒绝"""
    parts = urllib.parse.urlsplit(url)
    if parts.scheme not in ("http", "https"):
        raise RegistryError(f"封面地址只接受 http/https：{url}")
    name = parts.path.rsplit("/", 1)[-1]
    if not name:
        raise RegistryError(f"封面地址里没有文件名：{url}")
    if "/" in name or "\\" in name or ".." in name or name in (".", ".."):
        raise RegistryError(f"封面文件名含路径分隔符或 ..，拒绝落盘：{url}")
    if not BANNER_NAME_RE.match(name):
        raise RegistryError(f"封面文件名不在白名单内（字母数字 ._- 加图片后缀）：{name}")
    return name


def fetch_banner(url: str) -> bytes:
    """抓取封面原始字节，只走 http/https"""
    parts = urllib.parse.urlsplit(url)
    if parts.scheme not in ("http", "https"):
        raise RegistryError(f"封面地址只接受 http/https：{url}")
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    try:
        with urllib.request.urlopen(request, timeout=60) as response:
            return response.read()
    except urllib.error.HTTPError as exc:
        raise RegistryError(f"下载封面失败：HTTP {exc.code} {url}") from exc
    except urllib.error.URLError as exc:
        raise RegistryError(f"下载封面失败（网络不可用？）：{exc.reason} {url}") from exc
    except (OSError, http.client.HTTPException) as exc:
        # TLS 握手失败、连接被重置、响应被截断都属于这一类，统一折成一行中文错误
        raise RegistryError(f"下载封面失败：{exc} {url}") from exc
    except ValueError as exc:
        raise RegistryError(f"封面地址无法解析：{url}") from exc


def mirror_banners(
    plugins: List[Dict[str, Any]],
    banner_dir: str,
    banner_base_url: str,
    fetch: BannerFetcher,
    on_error: str = "fail",
) -> Tuple[List[str], List[str]]:
    """把 plugins[].banner 抓取到 banner_dir 并改写成对外地址

    返回（已改写的地址列表，降级保留原始地址的列表）。on_error 为 fail 时
    任何抓取失败都抛 RegistryError；为 keep 时保留原始 URL 并留一行提示。
    """
    base = banner_base_url.strip().rstrip("/")
    if urllib.parse.urlsplit(base).scheme not in ("http", "https"):
        raise RegistryError(f"--banner-base-url 只接受 http/https：{banner_base_url}")

    root = pathlib.Path(banner_dir)
    rewritten: List[str] = []
    kept: List[str] = []
    for plugin in plugins:
        source = str(plugin.get("banner") or "").strip()
        if not source:
            _info(f"{plugin.get('id')} 没有封面，跳过镜像")
            continue
        try:
            name = banner_filename(source)
            payload = fetch(source)
            root.mkdir(parents=True, exist_ok=True)
            target = root / name
            reused = target.is_file() and target.stat().st_size == len(payload)
            if not reused:
                target.write_bytes(payload)
            plugin["banner"] = f"{base}/banners/{name}"
            state = "已存在且大小一致，跳过写入" if reused else "新写入"
            _info(
                f"镜像成功（{state}）：{source} -> {plugin['banner']}"
                f"（{len(payload)} 字节）"
            )
            rewritten.append(plugin["banner"])
        except RegistryError as exc:
            if on_error != "keep":
                raise
            _info(f"降级：保留原始封面地址，{exc}")
            kept.append(source)
    return rewritten, kept


def asset_hash(asset: Mapping[str, Any], token: Optional[str]) -> Tuple[str, int]:
    """优先读 API 的 digest 字段，缺失时下载资产实际计算"""
    digest = str(asset.get("digest") or "")
    if digest.startswith("sha256:"):
        return digest[len("sha256:") :].lower(), int(asset.get("size") or 0)
    url = str(asset.get("browser_download_url") or "")
    if not url:
        raise RegistryError(f"资产既没有 digest 也没有下载地址：{asset.get('name')}")
    _warn(f"资产 {asset.get('name')} 没有 digest 字段，改为下载后计算 sha256")
    return _download_sha256(url, token)


ShaResolver = Callable[[Mapping[str, Any]], Tuple[str, int]]


def build_registry(
    releases: Sequence[Mapping[str, Any]],
    metadata: Mapping[str, Mapping[str, Any]],
    resolve_hash: ShaResolver,
    generated_at: str,
    source_commit: Optional[str] = None,
) -> Dict[str, Any]:
    """把 Release 列表整理成契约结构；同一插件只保留最新版本

    source_commit 非空时给每个插件追加可选的 sourceCommit 字段（溯源用）；
    留空则完全不写该字段。
    """
    latest: Dict[str, Tuple[str, List[Dict[str, Any]]]] = {}

    for release in releases:
        if release.get("draft"):
            continue
        tag = str(release.get("tag_name") or "")
        parsed = parse_tag(tag)
        if parsed is None:
            if tag:
                _warn(f"跳过不合规的 tag：{tag}（期望 <插件>-v<主>.<次>.<修订>）")
            continue
        plugin, version = parsed

        variants: List[Dict[str, Any]] = []
        for asset in release.get("assets") or []:
            if not isinstance(asset, dict):
                continue
            platform = variant_platform(plugin, version, str(asset.get("name") or ""))
            if platform is None:
                continue
            sha256, size = resolve_hash(asset)
            variants.append(
                {
                    "platform": platform,
                    "url": str(asset.get("browser_download_url") or ""),
                    "sha256": sha256,
                    "size": size,
                }
            )
        if not variants:
            _warn(f"跳过没有契约命名资产的 Release：{tag}")
            continue

        current = latest.get(plugin)
        if current is not None and parse_version(current[0]) >= parse_version(version):
            continue
        latest[plugin] = (version, variants)

    plugins: List[Dict[str, Any]] = []
    for plugin_id in sorted(latest):
        version, variants = latest[plugin_id]
        meta = metadata.get(plugin_id)
        if meta is None:
            _warn(f"{plugin_id} 缺少 module/{plugin_id}/meta/plugin.json，展示字段回退为 id")
            meta = {
                "name": plugin_id,
                "author": "",
                "description": "",
                "tags": [],
                "banner": "",
                "dependencies": [],
            }
        entry: Dict[str, Any] = {
            "id": plugin_id,
            "name": meta["name"],
            "version": version,
            "author": meta["author"],
            "description": meta["description"],
            "tags": list(meta["tags"]),
            "banner": meta["banner"],
            "dependencies": list(meta["dependencies"]),
            "variants": sorted(variants, key=lambda item: PLATFORMS.index(item["platform"])),
        }
        if source_commit:
            entry[SOURCE_COMMIT_FIELD] = source_commit
        plugins.append(entry)

    return {
        "schemaVersion": SCHEMA_VERSION,
        "generatedAt": generated_at,
        "plugins": plugins,
    }


def validate_registry(registry: Mapping[str, Any]) -> None:
    """按共享契约校验，任何偏离都抛 RegistryError 并指出具体位置"""
    problems: List[str] = []

    if set(registry) != {"schemaVersion", "generatedAt", "plugins"}:
        problems.append(f"顶层字段应为 schemaVersion/generatedAt/plugins，实际 {sorted(registry)}")
    if registry.get("schemaVersion") != SCHEMA_VERSION:
        problems.append(f"schemaVersion 应为 {SCHEMA_VERSION}，实际 {registry.get('schemaVersion')}")
    generated_at = str(registry.get("generatedAt") or "")
    if not GENERATED_AT_RE.match(generated_at):
        problems.append(f"generatedAt 应为 UTC 的 YYYY-MM-DDTHH:MM:SSZ，实际 {generated_at!r}")

    plugins = registry.get("plugins")
    if not isinstance(plugins, list):
        problems.append("plugins 应为数组")
        plugins = []

    seen_ids: List[str] = []
    for index, plugin in enumerate(plugins):
        where = f"plugins[{index}]"
        if not isinstance(plugin, dict):
            problems.append(f"{where} 应为对象")
            continue
        if tuple(plugin) not in (PLUGIN_FIELDS, PLUGIN_FIELDS_WITH_SOURCE_COMMIT):
            problems.append(
                f"{where} 字段应依次为 {list(PLUGIN_FIELDS)}"
                f"（可选追加 {SOURCE_COMMIT_FIELD}），实际 {list(plugin)}"
            )
        if SOURCE_COMMIT_FIELD in plugin:
            source_commit = plugin[SOURCE_COMMIT_FIELD]
            if not isinstance(source_commit, str) or not COMMIT_RE.match(source_commit):
                problems.append(
                    f"{where}.{SOURCE_COMMIT_FIELD} 应为小写十六进制提交号（7-40 位），"
                    f"实际 {source_commit!r}"
                )
        plugin_id = str(plugin.get("id") or "")
        seen_ids.append(plugin_id)
        if not TAG_RE.match(f"{plugin_id}-v0.0.0"):
            problems.append(f"{where}.id 不含合法插件标识：{plugin_id!r}")
        try:
            parse_version(str(plugin.get("version") or ""))
        except RegistryError as exc:
            problems.append(f"{where}.version 非法：{exc}")
        if not isinstance(plugin.get("tags"), list):
            problems.append(f"{where}.tags 应为数组")
        if not isinstance(plugin.get("dependencies"), list):
            problems.append(f"{where}.dependencies 应为数组")
        banner = str(plugin.get("banner") or "")
        if banner and not banner.startswith("https://"):
            problems.append(f"{where}.banner 必须留空或是 https 地址：{banner!r}")

        variants = plugin.get("variants")
        if not isinstance(variants, list) or not variants:
            problems.append(f"{where}.variants 不能为空")
            continue
        platforms: List[str] = []
        for position, variant in enumerate(variants):
            at = f"{where}.variants[{position}]"
            if not isinstance(variant, dict):
                problems.append(f"{at} 应为对象")
                continue
            if tuple(variant) != VARIANT_FIELDS:
                problems.append(f"{at} 字段应依次为 {list(VARIANT_FIELDS)}，实际 {list(variant)}")
            platform = variant.get("platform")
            if platform not in PLATFORMS:
                problems.append(f"{at}.platform 不在契约取值集合内：{platform!r}")
            else:
                platforms.append(platform)
            if not str(variant.get("url") or "").startswith("https://"):
                problems.append(f"{at}.url 必须是 https 地址")
            if not SHA256_RE.match(str(variant.get("sha256") or "")):
                problems.append(f"{at}.sha256 必须是小写十六进制 64 位")
            size = variant.get("size")
            if not isinstance(size, int) or isinstance(size, bool) or size <= 0:
                problems.append(f"{at}.size 必须是正整数")
        expected = [name for name in PLATFORMS if name in platforms]
        if platforms != expected:
            problems.append(f"{where}.variants 未按 platform 稳定排序：{platforms}")

    if seen_ids != sorted(seen_ids):
        problems.append(f"plugins 未按 id 稳定排序：{seen_ids}")

    if problems:
        raise RegistryError("registry.json 不符合契约：\n  " + "\n  ".join(problems))


def write_registry(registry: Mapping[str, Any], out_path: str) -> pathlib.Path:
    target = pathlib.Path(out_path)
    target.parent.mkdir(parents=True, exist_ok=True)
    text = json.dumps(registry, ensure_ascii=False, indent=2)
    target.write_text(text + "\n", encoding="utf-8")
    return target


SELF_TEST_GENERATED_AT = "2026-11-22T05:00:00Z"
# 自检用的假提交号：形状与 github.sha 一致，不指向任何真实提交
SELF_TEST_SOURCE_COMMIT = "4f1c2b8a9d7e6350a1b2c3d4e5f60718293a4b5c"

SELF_TEST_METADATA: Dict[str, Dict[str, Any]] = {
    "gsi": {
        "name": "CSGO 2 | 反恐精英 2",
        "author": "CrimsonSeraph",
        "description": "通过游戏状态集成读取 CS2 对局数据",
        "tags": ["游戏遥测", "GSI"],
        "banner": "https://raw.githubusercontent.com/CrimsonSeraph/DG-LAB-Client-Plugin/main/banners/gsi.jpg",
        "dependencies": [],
    },
    "warthunder": {
        "name": "WarThunder | 战争雷霆",
        "author": "CrimsonSeraph",
        "description": "读取 127.0.0.1:8111 空战遥测",
        "tags": ["游戏遥测", "HTTP"],
        "banner": "https://raw.githubusercontent.com/CrimsonSeraph/DG-LAB-Client-Plugin/main/banners/warthunder.jpg",
        "dependencies": [],
    },
}


def _self_test_releases() -> List[Dict[str, Any]]:
    def asset(name: str, size: int, digest: Optional[str] = None) -> Dict[str, Any]:
        url = f"https://github.com/CrimsonSeraph/DG-LAB-Client-Plugin/releases/download/{name}"
        item: Dict[str, Any] = {"name": name, "size": size, "browser_download_url": url}
        if digest is not None:
            item["digest"] = f"sha256:{digest}"
        return item

    return [
        # 顺序故意打乱，用来验证排序
        {
            "tag_name": "warthunder-v0.2.0",
            "draft": False,
            "assets": [
                asset("warthunder-0.2.0-macos-arm64.zip", 41001, "d" * 64),
                asset("warthunder-0.2.0-windows-x64.zip", 21003, "b" * 64),
                # 没有 digest 字段，走下载计算分支（自检里由替身函数给出结果）
                asset("warthunder-0.2.0-linux-x64.zip", 31002),
                asset("warthunder-0.2.0-banner.jpg", 9000, "e" * 64),
            ],
        },
        {
            "tag_name": "gsi-v0.7.0",
            "draft": False,
            "assets": [
                asset("gsi-0.7.0-linux-x64.zip", 51005, "c" * 64),
                asset("gsi-0.7.0-windows-x64.zip", 61006, "a" * 64),
                asset("gsi-0.7.0-SHA256SUMS.txt", 512, "f" * 64),
            ],
        },
        # 同一插件的旧版本，应当被丢弃
        {
            "tag_name": "warthunder-v0.1.9",
            "draft": False,
            "assets": [asset("warthunder-0.1.9-windows-x64.zip", 20000, "1" * 64)],
        },
        # 不合规 tag 与草稿 Release，应当被跳过
        {"tag_name": "nightly-2024", "draft": False, "assets": []},
        {
            "tag_name": "warthunder-v0.3.0",
            "draft": True,
            "assets": [asset("warthunder-0.3.0-windows-x64.zip", 22000, "2" * 64)],
        },
    ]


def _self_test_hash(asset: Mapping[str, Any]) -> Tuple[str, int]:
    digest = str(asset.get("digest") or "")
    if digest.startswith("sha256:"):
        return digest[len("sha256:") :].lower(), int(asset.get("size") or 0)
    # 替身：用资产名算出稳定的 sha256，等价于下载后计算结果
    name = str(asset.get("name") or "").encode("utf-8")
    return hashlib.sha256(name).hexdigest(), int(asset.get("size") or 0)

SELF_TEST_BANNER_BYTES: Dict[str, bytes] = {
    "gsi.jpg": b"\xff\xd8\xff\xe0" + b"gsi-cover-sample" * 8,
    "warthunder.jpg": b"\xff\xd8\xff\xe0" + b"warthunder-cover-sample" * 8,
}


def _write_banner_fixtures(fixture_dir: pathlib.Path) -> Dict[str, pathlib.Path]:
    """自检用本地文件冒充插件仓库 banners/ 里的封面"""
    fixture_dir.mkdir(parents=True, exist_ok=True)
    fixtures: Dict[str, pathlib.Path] = {}
    for name, payload in SELF_TEST_BANNER_BYTES.items():
        path = fixture_dir / name
        path.write_bytes(payload)
        fixtures[name] = path
    return fixtures


def _local_banner_fetch(fixtures: Mapping[str, pathlib.Path]) -> BannerFetcher:
    """抓取替身：按 URL 文件名读本地样例文件，全程不联网"""

    def fetch(url: str) -> bytes:
        name = urllib.parse.urlsplit(url).path.rsplit("/", 1)[-1]
        path = fixtures.get(name)
        if path is None:
            raise RegistryError(f"下载封面失败：HTTP 404 {url}")
        return path.read_bytes()

    return fetch


def _expect_registry_error(failures: List[str], message: str, action: Callable[[], None]) -> None:
    try:
        action()
    except RegistryError as exc:
        print(f"[self-test] 断言通过：{message}（{exc}）")
    else:
        failures.append(f"{message}，但实际没有报错")


def _self_test_banner_guards(failures: List[str], banner_dir: str) -> None:
    """白名单与失败降级：这些路径都不联网，失败时也落不了盘"""

    def fetch(url: str) -> bytes:
        raise RegistryError(f"下载封面失败：HTTP 404 {url}")

    dotted = "https://raw.githubusercontent.com/CrimsonSeraph/DG-LAB-Client-Plugin/main/banners/.."
    _expect_registry_error(
        failures,
        "封面文件名含 .. 时拒绝落盘",
        lambda: mirror_banners(
            [{"id": "gsi", "banner": dotted}], banner_dir, DEFAULT_BANNER_BASE_URL, fetch
        ),
    )
    _expect_registry_error(
        failures,
        "非 http/https 封面地址被拒绝",
        lambda: mirror_banners(
            [{"id": "gsi", "banner": "ftp://example.com/banners/gsi.jpg"}],
            banner_dir,
            DEFAULT_BANNER_BASE_URL,
            fetch,
        ),
    )

    missing = "https://raw.githubusercontent.com/CrimsonSeraph/DG-LAB-Client-Plugin/main/banners/missing.jpg"
    degraded = {"id": "gsi", "banner": missing}
    rewritten, kept = mirror_banners(
        [degraded], banner_dir, DEFAULT_BANNER_BASE_URL, fetch, on_error="keep"
    )
    if rewritten:
        failures.append("抓取失败降级时不应改写 banner")
    elif degraded["banner"] != missing or kept != [missing]:
        failures.append(f"降级必须保留原始 URL，实际 {degraded['banner']!r} / {kept}")
    else:
        print(f"[self-test] 断言通过：抓取失败降级保留原始 URL {missing}")
    _expect_registry_error(
        failures,
        "抓取失败且未降级时给出中文错误",
        lambda: mirror_banners(
            [{"id": "gsi", "banner": missing}], banner_dir, DEFAULT_BANNER_BASE_URL, fetch
        ),
    )


def _check(failures: List[str], ok: bool, message: str) -> None:
    if ok:
        print(f"[self-test] 断言通过：{message}")
    else:
        failures.append(message)


def run_self_test() -> int:
    registry = build_registry(
        _self_test_releases(),
        SELF_TEST_METADATA,
        _self_test_hash,
        SELF_TEST_GENERATED_AT,
        SELF_TEST_SOURCE_COMMIT,
    )
    validate_registry(registry)

    failures: List[str] = []
    plugins = {plugin["id"]: plugin for plugin in registry["plugins"]}
    _check(
        failures,
        [plugin["id"] for plugin in registry["plugins"]] == ["gsi", "warthunder"],
        "plugins 应按 id 排序为 gsi / warthunder",
    )
    _check(
        failures,
        plugins.get("warthunder", {}).get("version") == "0.2.0",
        "warthunder 应保留最新版本 0.2.0",
    )
    _check(failures, "nightly" not in plugins, "不合规 tag nightly-2024 不应进入注册表")
    platforms = [variant["platform"] for variant in plugins["warthunder"]["variants"]]
    _check(
        failures,
        platforms == ["windows-x64", "linux-x64", "macos-arm64"],
        f"variants 应按契约平台顺序排序，实际 {platforms}",
    )
    _check(
        failures,
        all(SHA256_RE.match(variant["sha256"]) for variant in plugins["warthunder"]["variants"]),
        "sha256 应是小写十六进制 64 位",
    )
    _check(
        failures,
        all(plugin.get(SOURCE_COMMIT_FIELD) == SELF_TEST_SOURCE_COMMIT for plugin in registry["plugins"]),
        f"sourceCommit 已写入 {SELF_TEST_SOURCE_COMMIT}",
    )
    bare = build_registry(
        _self_test_releases(), SELF_TEST_METADATA, _self_test_hash, SELF_TEST_GENERATED_AT
    )
    validate_registry(bare)
    _check(
        failures,
        all(SOURCE_COMMIT_FIELD not in plugin for plugin in bare["plugins"]),
        "不传 source_commit 时 plugins[] 不带该字段（可选字段，schemaVersion 仍为 1）",
    )
    _expect_registry_error(
        failures,
        f"{SOURCE_COMMIT_FIELD} 不是提交号时判为违约",
        lambda: validate_registry(
            {
                **bare,
                "plugins": [{**bare["plugins"][0], SOURCE_COMMIT_FIELD: "not-a-sha"}, *bare["plugins"][1:]],
            }
        ),
    )

    with tempfile.TemporaryDirectory() as workdir:
        work = pathlib.Path(workdir)
        banner_dir = work / "public" / "banners"
        fixtures = _write_banner_fixtures(work / "fixtures")
        fetch = _local_banner_fetch(fixtures)

        rewritten, kept = mirror_banners(
            registry["plugins"], str(banner_dir), DEFAULT_BANNER_BASE_URL, fetch
        )
        _check(
            failures,
            len(rewritten) == 2 and not kept,
            f"两个封面都镜像并改写，实际改写 {len(rewritten)} 个、保留 {len(kept)} 个",
        )
        for plugin_id, name in (("gsi", "gsi.jpg"), ("warthunder", "warthunder.jpg")):
            mirrored = banner_dir / name
            expected_bytes = fixtures[name].read_bytes()
            _check(
                failures,
                mirrored.is_file() and mirrored.read_bytes() == expected_bytes,
                f"镜像成功：{plugin_id} 的封面写入 {mirrored.as_posix()}（{len(expected_bytes)} 字节，与源文件逐字节一致）",
            )
            expected = f"{DEFAULT_BANNER_BASE_URL}/banners/{name}"
            _check(
                failures,
                plugins[plugin_id]["banner"] == expected,
                f"URL 已改写为 {expected}",
            )

        # 第二遍：文件已存在且大小一致，应当跳过写入，改写结果保持稳定
        again, again_kept = mirror_banners(
            registry["plugins"], str(banner_dir), DEFAULT_BANNER_BASE_URL, fetch
        )
        _check(
            failures,
            again == rewritten and not again_kept,
            "封面已存在且大小一致时跳过写入，URL 保持稳定",
        )
        validate_registry(registry)

        broken = json.loads(json.dumps(registry))
        broken["plugins"][0]["banner"] = "http://raw.githubusercontent.com/x/y.jpg"
        _expect_registry_error(
            failures,
            "registry.json 里非 https 的 banner 判为违约",
            lambda: validate_registry(broken),
        )

        _self_test_banner_guards(failures, str(work / "guards"))

    print(json.dumps(registry, ensure_ascii=False, indent=2))
    if failures:
        print("[self-test] 失败：", file=sys.stderr)
        for item in failures:
            print(f"  {item}", file=sys.stderr)
        return 1
    print(
        "[self-test] 通过：契约两种形态均校验通过，插件 2 个，变体 6 个，"
        "sourceCommit 按参数可选写入，封面 2 张已镜像并改写到 Worker 域名"
    )
    return 0


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="扫描插件仓库全部 Release 生成 registry.json")
    parser.add_argument("--repo", help="插件仓库，形如 owner/name")
    parser.add_argument("--modules-dir", default="module", help="本仓库插件源码目录，读取展示元数据")
    parser.add_argument("--out", default="registry.json", help="registry.json 输出路径")
    parser.add_argument(
        "--token",
        default=os.environ.get("GITHUB_TOKEN") or os.environ.get("GH_TOKEN"),
        help="GitHub token，默认读 GITHUB_TOKEN / GH_TOKEN",
    )
    parser.add_argument(
        "--banner-dir",
        default=os.path.join("deploy", "cloudflare", "public", "banners"),
        help="封面镜像落盘目录，默认 deploy/cloudflare/public/banners（wrangler 的静态资源目录）",
    )
    parser.add_argument(
        "--banner-base-url",
        default=DEFAULT_BANNER_BASE_URL,
        help=f"封面对外下载基地址，默认 {DEFAULT_BANNER_BASE_URL}",
    )
    parser.add_argument(
        "--banner-on-error",
        choices=("fail", "keep"),
        default="fail",
        help="封面抓取失败时：fail 直接报错（默认），keep 降级保留原始 URL 并留一行提示",
    )
    parser.add_argument(
        "--source-commit",
        default=os.environ.get("GITHUB_SHA") or "",
        help="写进 plugins[].sourceCommit 的源码提交，默认读 GITHUB_SHA；留空则不写该字段",
    )
    parser.add_argument("--self-test", action="store_true", help="用内置样例数据离线自检")
    args = parser.parse_args(argv)

    if args.self_test:
        try:
            return run_self_test()
        except RegistryError as exc:
            print(f"[self-test] 失败：{exc}", file=sys.stderr)
            return 1

    if not args.repo:
        print("错误：真实模式必须提供 --repo owner/name", file=sys.stderr)
        return 2

    try:
        metadata = load_plugin_metadata(args.modules_dir)
        releases = fetch_releases(args.repo, args.token)
        if not releases:
            raise RegistryError(f"{args.repo} 还没有任何 Release，请先发布插件")
        registry = build_registry(
            releases,
            metadata,
            lambda asset: asset_hash(asset, args.token),
            utc_timestamp(),
            args.source_commit or None,
        )
        if not registry["plugins"]:
            raise RegistryError(
                f"{args.repo} 的 Release 里没有 <插件>-v<版本> 形式的 tag 或 <插件>-<版本>-<平台>.zip 资产"
            )
        mirrored, kept = mirror_banners(
            registry["plugins"],
            args.banner_dir,
            args.banner_base_url,
            fetch_banner,
            args.banner_on_error,
        )
        validate_registry(registry)
        target = write_registry(registry, args.out)
    except RegistryError as exc:
        print(f"错误：{exc}", file=sys.stderr)
        return 2

    print(f"已写入 {target}：{len(registry['plugins'])} 个插件")
    if args.source_commit:
        print(f"sourceCommit：{args.source_commit}")
    for plugin in registry["plugins"]:
        platforms = ", ".join(variant["platform"] for variant in plugin["variants"])
        print(f"  {plugin['id']} {plugin['version']}（{platforms}）{plugin['banner']}")
    if mirrored:
        print(f"封面 {len(mirrored)} 张已镜像到 {args.banner_dir}，对外基地址 {args.banner_base_url}")
    if kept:
        print(
            f"提示：{len(kept)} 个插件保留原始封面地址（--banner-on-error keep），"
            "客户端仍会回落到原始地址",
            file=sys.stderr,
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
