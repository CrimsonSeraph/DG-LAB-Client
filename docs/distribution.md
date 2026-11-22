# 分发链路

插件与应用各有一条独立的发布线：应用打 `v<版本>` 标签，插件打 `<插件>-v<版本>` 标签。两条线的产物都落在 GitHub Release 上，再由 Cloudflare 上的同一个静态资源 Worker 汇总成两份清单交给宿主读取。本文说明每一段的输入输出、产物落点，以及出错时先看哪里。

## 链路总览

```text
应用线                            插件线
git tag v2.5.0                    git tag gsi-v0.2.0
      │                                 │
      ▼                                 ▼
release-all.yml                   plugin-release.yml
      │                                 │
      │                                 ├─ 校验 plugin.json 的 id / version 与标签一致
      │                                 ├─ 只构建标签点名的那个插件
      │                                 └─ 四平台 zip：windows-x64 / linux-x64 / macos-x64 / macos-arm64
      │                                               │
      ├─ 构建各平台产物 ─▶ Release                    ▼
      │   （NSIS exe / universal dmg /          插件仓库 Release
      │    x64 AppImage，另有手动下载用的        （资产 <插件>-<版本>-<平台>.zip）
      │    zip、7z、tar.gz、deb、rpm、msi）             │
      │                                               ▼
      ├─ release job 生成 manifest.json ─▶ 同一 Release  gen_registry.py 扫描插件仓库全部 Release
      │                                               ▼
      │                                        registry.json
      ▼                                               │
      └──────────┬────────────────────────────────────┘
                 ▼
        deploy/cloudflare/public/{manifest.json, registry.json}（两份必须同时在场）
                 ▼
        npx wrangler@4 deploy —— direct upload，Cloudflare 侧零构建
                 ▼
        Cloudflare Worker：worker.js 把请求转给 ASSETS 绑定（自定义域名）
                 ▼
        宿主：插件市场读 registry.json · 自动更新读 manifest.json
              插件封面镜像自插件仓库 banners/（不随 zip 分发）
```

## 两条发布线

| 维度 | 应用发布 | 插件发布 |
| --- | --- | --- |
| 触发标签 | `v<主>.<次>.<修订>`，如 `v2.5.0` | `<插件>-v<主>.<次>.<修订>`，如 `warthunder-v0.1.0` |
| 工作流 | `.github/workflows/release-all.yml` | `.github/workflows/plugin-release.yml` |
| 构建范围 | 全部平台的全部产物 | 只有标签点名的那个插件 |
| 产物落点 | 主仓库 Release | 插件仓库 `DG-LAB-Client-Plugin` 的 Release |
| 生成的清单 | `manifest.json`（只服务应用自更新） | `registry.json`（只服务插件市场） |
| 消费方 | 客户端自更新 | 客户端插件市场 |

两条线互斥：`plugin-release.yml` 开头有显式守卫，遇到 `v1.2.3` 这类标签直接以「标签归属错误」失败，不会因为误触发就把应用标签当插件标签处理。

## 插件发布

### 标签命名规则

`<插件>-v<主>.<次>.<修订>`，其中 `<插件>` 必须是 `module/` 下的目录名，版本必须是三段数字。工作流解析标签后做三件事：确认 `module/<插件>/meta/plugin.json` 存在、确认它的 `id` 等于标签里的插件名、确认它的 `version` 等于标签里的版本，然后确认 `CMakeLists.txt` 里有对应的 `dgl_add_plugin(dgl_<插件>_plugin ...)`。

也可以用 `workflow_dispatch` 手动填插件名与版本，效果等价于打一个同名标签。

### 版本必须与 plugin.json 一致

侧车 `plugin.json` 的 `version` 与标签版本不一致时 CI 直接失败，报「版本不一致」并提示先把 `plugin.json` 改到同一版本再打标签。这条检查存在的理由是：`plugin.json` 会原样进 zip，宿主与插件市场都用它当展示与版本依据，标签与侧车不一致就会出现「市场显示 0.1.0、实际下载到 0.2.0」这类无法自证的状态。

### 四个平台与产物结构

每个插件都出四个 zip，平台标识固定：`windows-x64`、`linux-x64`、`macos-x64`、`macos-arm64`。资产名格式为 `<插件>-<版本>-<平台>.zip`，例如 `warthunder-0.1.0-macos-arm64.zip`。

zip 根目录是插件的 CMake 目标名，解压目标为宿主程序目录下的 `module/`：

```text
warthunder-0.1.0-windows-x64.zip
└── dgl_warthunder_plugin/
    ├── dgl_warthunder_plugin.dll     # 动态库，主干名恒等于目标名
    └── plugin.json                   # 侧车展示元数据
```

打包脚本会校验根目录必须是 `<目标名>/` 且必须含 `plugin.json`，否则报「zip 结构错误」。侧车目录里的其它附属文件会一并打进 `<目标名>/` 下，但图片后缀与 `banner*` 文件名被刻意过滤掉——封面不随 zip 分发，见下文。

### 发布到插件仓库

`publish` job 在插件仓库创建或更新同名标签的 Release，用 `gh release upload --clobber` 上传四份 zip。插件源码与动态库的产物不进主仓库的 Release，主仓库只做构建。

## 应用发布

### 标签与产物

`v<主>.<次>.<修订>` 触发 `release-all.yml`：各平台分别构建，汇总到 `dist/`，再由 `release` job 生成 `manifest.json` 一并作为 Release 资产上传。版本号的唯一来源是 `config/main.json` 的 `app.version`；标签与它不一致时只告警，产物文件名和 `manifest.version` 都取 `config/main.json` 的值。

### manifest.json 的字段

```json
{
    "version": "2.5.0",
    "release_notes": "本版本的说明正文",
    "release_url": "https://github.com/<仓库>/releases/tag/v2.5.0",
    "platforms": {
        "windows": { "file": "...", "url": "...", "sha256": "..." },
        "macos": { "file": "...", "url": "...", "sha256": "..." },
        "linux": { "file": "...", "url": "...", "sha256": "..." }
    }
}
```

`platforms` 固定三个键，每个键只放一个包：`file` 是资产文件名，`url` 指向该 tag 的 Release 资产，`sha256` 是该文件的摘要，供客户端下载后校验。

### 每平台只取一个推荐包

`release` job 用固定通配各挑一个文件：Windows 取 `DG-LAB-Client-*-win64*.exe`（NSIS 安装包）、macOS 取 `DG-LAB-Client-*-macos-universal.dmg`、Linux 取 `DG-LAB-Client-*-linux-x64.AppImage`。匹配不到任一推荐包就整个 job 失败，不会退化成「随便挑一个」。

Release 里同时上传的 `zip`、`7z`、`tar.gz`、`tgz`、`deb`、`rpm`、`msi`、`pkg` 是给用户手动下载的替代格式，不参与自更新：客户端只看 `manifest.json` 里那一个 `file`/`url`。想换推荐包形态，改的是 `release` job 里的挑包规则，不是往 Release 里多塞一个文件。

## 两份清单的分工

| 清单 | 服务对象 | 生成方 | 格式归属 | 内容要点 |
| --- | --- | --- | --- | --- |
| `registry.json` | 插件市场 | `scripts/gen_registry.py` 扫描插件仓库全部 Release | 宿主与生成脚本共用的插件契约 | `schemaVersion` + `generatedAt` + `plugins[]`，插件内含 `id`/`name`/`version`/`author`/`description`/`tags`/`banner`/`dependencies`/`variants[]`，每个 variant 是 `platform`/`url`/`sha256`/`size` |
| `manifest.json` | 应用自更新 | `release-all.yml` 的 `release` job | `release` job 独占 | `version`/`release_notes`/`release_url`/`platforms` |

两者都挂在同一个 Worker 上，但互不引用：插件市场不会因为应用发版而变，应用更新也不会因为插件发版而变。清单里所有下载 URL 都指向具体的 Release 资产，不经 Worker 中转。

客户端自更新的默认地址是主仓库最新 Release 上的 `manifest.json` 资产（配置键 `app.update.manifest_url`），Worker 上那份由 `publish` job 从 Release 取回、校验 sha256 后原样搬运，两边逐字节一致，所以「更新清单看不到了」时优先查 Release 资产，而不是先怀疑 Worker。

封面地址由生成脚本统一改写：每次生成都会抓取侧车 `banner` 指向的图片，落到 Worker 的静态资源目录 `banners/`，并把清单里的地址改写为 `<Worker 域名>/banners/<文件名>`，客户端因此不直接依赖外部图床。抓取失败默认直接报错（`--banner-on-error fail`），也可以显式降级成 `keep` 保留原始地址。所以排查封面问题先确认图片在插件仓库 `banners/` 里确实存在且可下载。

`registry.json` 的派生规则值得记住：同一插件只保留最新版本；tag 不符合 `<插件>-v<版本>` 的 Release 被跳过并打警告；没有任何符合命名契约资产的 Release 同样被跳过。所以插件「没出现在市场里」时，先查资产命名，再查 tag，最后才怀疑生成脚本。

## 部署形态

### direct upload，Cloudflare 侧零构建

`deploy/cloudflare/` 下只有三样东西：`wrangler.toml`、`worker.js`、CI 写出的 `public/`。`npx wrangler@4 deploy` 走 Direct Upload，把本机 `public/` 里的文件作为静态资源整体推到 Cloudflare，没有 build 命令、没有 Git 集成，构建全部发生在 GitHub Actions 里。因此不消耗 Cloudflare 的构建次数与构建时长。

`public/` 不入库，本地与仓库里都不保留生成物，避免生成结果与源数据打架。

### 必须有 main 脚本与 ASSETS 绑定

Worker 形态是「显式脚本 + `ASSETS` 绑定」，不是纯 assets：

```toml
name = "dglab-registry"
main = "worker.js"
compatibility_date = "2025-06-01"

routes = [{ pattern = "dglab-registry.crimsonseraph.top", custom_domain = true }]

[assets]
directory = "public"
binding = "ASSETS"
```

`worker.js` 只做一次 `env.ASSETS.fetch(request)` 转发。纯 assets 形态在本环境走不通，两种失败都实测过：

- 声明了 `binding = "ASSETS"` 却没有 `main`：`wrangler deploy` 直接拒绝，报 `Cannot use assets with a binding in an assets-only Worker`。
- 去掉 binding 的纯 assets 形态：上传能成功，但运行期每个请求返回 HTTP 500 `error code: 1101`。

对外用 `custom_domain` 走自定义域名，不暴露账号级的 `*.workers.dev` 子域。客户端插件市场默认就指向 `https://dglab-registry.crimsonseraph.top/registry.json`，可用配置键 `app.market.registry_url` 覆盖。

### 一次部署必须两份清单同时在场

`wrangler deploy` 会把 `public/` 当作 Worker 的完整静态资源集合整体替换：少放一个文件就等于把线上那一份删掉。应用发布线的部署步骤写 `manifest.json` 与 `registry.json`，并在上传前逐个 `test -f` 校验；插件发布线写 `registry.json` 与镜像后的 `banners/`。所以判断「线上少了什么」只需要看最近一次 deploy 的 `public/` 里有什么——漏写的那份不会被拦下，它会直接从线上消失。

## 宿主如何消费

| 环节 | 宿主侧 | 读取来源 |
| --- | --- | --- |
| 插件市场列表与版本 | `MarketBridge`，配置键 `app.market.registry_url` | `registry.json` |
| 插件安装 | 下载 variant 的 `url`，按 `sha256` 校验 | 插件仓库 Release 资产 |
| 插件封面（市场卡片） | 只认绝对 `http(s)` URL，其它写法判空；地址通常是 Worker 域名下的 `/banners/<文件名>` | `registry.json` 的 `banner` |
| 插件封面（本地已装插件） | 纯文件名走 `image://modulebanner/<插件文件名>/<文件名>`，绝对 URL 直接当 `Image.source` | 插件目录下的侧车与图片 |
| 应用自更新 | 取 `manifest.json`，按当前平台取 `platforms.<平台>` 的 `file`/`url`，下载后校验 `sha256` | `manifest.json`（缺省指向主仓库最新 Release 的资产） |

插件封面不随 zip 分发：官方封面的唯一真源是插件仓库的 `banners/` 目录，主仓库侧车的 `banner` 字段填该目录下图片的绝对 https URL，发布时镜像到 Worker。详见 [plugin-development.md](plugin-development.md) 的侧车字段说明。

## 常见失败与处置

| 现象 | 原因 | 处置 |
| --- | --- | --- |
| 插件工作流报「标签归属错误」 | 打了 `v1.2.3` 这类应用标签 | `v*` 归应用发布；插件标签改成 `<插件>-v<版本>` |
| 插件工作流报「版本不一致」 | 标签版本与 `module/<插件>/meta/plugin.json` 的 `version` 不同 | 改 `plugin.json` 再重打标签，删掉旧标签与旧 Release |
| 插件工作流报「标识不一致」 | 侧车 `id` 与 `module/` 下的目录名不同 | 让两者一致，`id` 同时是市场里的插件键 |
| 插件工作流报「插件未注册」 | `CMakeLists.txt` 里没有 `dgl_add_plugin(dgl_<插件>_plugin ...)` | 补注册；目标名规则固定为 `dgl_<插件>_plugin` |
| 打包报「侧车缺失」/`build 下没有 */module/<目标名>/plugin.json` | `META_DIR` 没指向侧车目录，或 `dgl_add_plugin` 参数漏写 | 检查 `META_DIR`，确认构建后侧车随动态库复制到 `module/<目标名>/` |
| 打包报「产物缺失」 | 动态库没落到 `module/` | 看失败日志里列出的候选路径，确认 CMake 目标确实构建成功 |
| 打包报「zip 结构错误」 | 根目录不是 `<目标名>/` 或缺少 `plugin.json` | 不要手改打包逻辑，回到 `dgl_add_plugin` 配置 |
| 应用发版报「缺少 Windows 安装包 / macOS dmg / Linux AppImage」 | `dist/` 里没有匹配 `DG-LAB-Client-*-win64*.exe` / `*-macos-universal.dmg` / `*-linux-x64.AppImage` 的文件 | 补构建产物，或确认产物命名仍符合挑包通配 |
| `wrangler deploy` 报 `Cannot use assets with a binding in an assets-only Worker` | `[assets]` 里有 `binding` 却没有 `main` | 保留 `main = "worker.js"`，不要退回纯 assets |
| 线上每个请求 HTTP 500 `error code: 1101` | 部署成了无脚本的纯 assets Worker | 同上，恢复「脚本 + ASSETS 绑定」形态 |
| 部署后另一份清单 404 | 只上传了一份清单，`public/` 被整体替换 | 两份清单同时写好再 deploy |
| 某份清单在线上 404 | 最近一次 deploy 的 `public/` 里没有这个文件，整体替换把它删了 | 补齐 `public/` 后重新 `wrangler deploy`；两条线都要确认该带的文件都带着 |
| 插件已发布但市场里看不到 | 资产命名不符 `<插件>-<版本>-<平台>.zip`，或 tag 不合契约 | 按契约重命名资产并重传 |

## 贡献者最小操作清单

1. 在 `module/<插件>/` 下写插件源码，侧车目录放 `plugin.json`，在 `CMakeLists.txt` 里用 `dgl_add_plugin(dgl_<插件>_plugin ... META_DIR module/<插件>/meta)` 注册。
2. 侧车 `id` 与 `module/` 下的目录名一致，`version` 与本轮要打的标签版本一致。
3. 封面：把图片传到插件仓库 `banners/`，侧车 `banner` 填其绝对 https URL；插件发布时会镜像到 Worker 并改写清单地址，图片本身不随 zip 分发。
4. 推送代码，打标签 `<插件>-v<主>.<次>.<修订>` 并 push。
5. 等 `plugin-release.yml` 绿灯：Release 上应有四份 `<插件>-<版本>-<平台>.zip`，摘要里有四平台清单。
6. `publish` job 会重新生成 `registry.json` 并部署 Worker；客户端插件市场刷新即可看到新版本。
7. 只改了插件、没改应用版本时，不要碰 `config/main.json` 的 `app.version`，也不要用 `v*` 标签。

## 相关文档

- [plugin-development.md](plugin-development.md)：插件接口、侧车字段与构建示例
- [build.md](build.md)：本地构建、打包与测试
- [features.md](features.md)：插件中心、加载状态与自动更新的用户视角
- [../deploy/cloudflare/README.md](../deploy/cloudflare/README.md)：Worker 目录结构、token 权限与本地预览
