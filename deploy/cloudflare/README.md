# 静态资源 Worker（dglab-registry）

托管两份清单的零构建静态资源 Worker。Worker 名已从 `dg-lab-plugin-registry` 改为 `dglab-registry`：它现在不只托管插件注册表，还托管应用更新清单。

Worker 形态是「显式脚本 + `ASSETS` 绑定」：`worker.js` 把每个请求转给 `env.ASSETS`，静态文件仍由 `wrangler deploy` 从本机直传，Cloudflare 侧不编译任何东西。

## 托管什么

| 文件 | 内容 | 格式归属 | 生成方 |
| --- | --- | --- | --- |
| `registry.json` | 插件清单 | 宿主端插件市场契约（`schemaVersion` / `plugins[]` / `variants[]`），与客户端 `MarketBridge` 共用 | `scripts/gen_registry.py` |
| `manifest.json` | 应用更新清单 | 由 `release-all.yml` 的 `release` job 定义，客户端 `UpdateManager` 解析（`version` / `release_notes` / `release_url` / `platforms`） | `release-all.yml` 的 `release` job 生成并作为 Release 资产上传 |

本目录不参与任何清单的生成，`public/` 里的两份文件都是 CI 写好之后原样上传的。更新清单的格式归 `release` job 所有，`publish` job 只从 Release 取回同一份字节，因此 Worker 上的 `manifest.json` 与 `https://github.com/CrimsonSeraph/DG-LAB-Client/releases/latest/download/manifest.json` 始终一致。

## 为什么是零 Cloudflare 构建

- `wrangler deploy` 走 Direct Upload：本地读 `public/` 目录，把里面的文件作为静态资源直接推到 Cloudflare
- Worker 的 `main` 只是一个转发脚本（`env.ASSETS.fetch(request)`），没有 `build` 命令，也没有 Git 集成
- 因此不消耗 Cloudflare 的构建次数与构建时长，构建全部发生在 GitHub Actions 里
- 与 Workers Builds 的区别：Workers Builds 会在 Cloudflare 侧检出仓库并跑一次构建，本方案不需要那条链路

## 为什么必须有 main 脚本

纯 assets、没有 main 脚本的 Worker 在这套环境下走不通，两种失败都实测过：

- 声明了 `binding = "ASSETS"` 却没有 `main`：`wrangler deploy` 直接拒绝，报 `Cannot use assets with a binding in an assets-only Worker`，要求去掉 binding 或补上 main 脚本
- 去掉 binding 的纯 assets 形态：上传能成功，但运行期每个请求都是 HTTP 500 `error code: 1101`

所以两个 Worker 统一成「显式脚本 + `ASSETS` 绑定」：`worker.js` 只做一次 `env.ASSETS.fetch(request)` 转发，静态资源仍然由 direct upload 推上去

## 一次部署必须两份清单同时在场

`wrangler deploy` 会把 `public/` 当作 Worker 的完整静态资源集合整体替换：少放一个文件就会把线上那一份删掉。所以 `plugin-release.yml` 与 `release-all.yml` 的部署步骤都是先把两份清单写齐再上传，任何一份缺失都直接失败。

## 目录

```text
deploy/cloudflare/
├── wrangler.toml   # main 指向 worker.js，[assets] directory 指向 public/
├── worker.js       # 把请求转给 env.ASSETS，不做别的事
├── public/         # CI 生成，不入库
│   ├── registry.json
│   └── manifest.json
└── README.md
```

`public/` 由 CI 写出，本地与仓库里都不保留，避免生成物与源数据打架

## 生成与部署

```bash
# 插件清单（需要能读插件仓库 Release 的 token）
export GITHUB_TOKEN=...
python scripts/gen_registry.py \
  --repo CrimsonSeraph/DG-LAB-Client-Plugin \
  --modules-dir module \
  --out deploy/cloudflare/public/registry.json

# 应用更新清单：从 Release 取回 release job 的产物，不重新生成也不改字段
gh release download v2.5.0 --pattern manifest.json --dir deploy/cloudflare/public

# 部署静态资源
cd deploy/cloudflare
npx wrangler@4 deploy
```

`.github/workflows/plugin-release.yml` 的 `publish` job 负责插件清单，`.github/workflows/release-all.yml` 的 `publish` job 负责应用发版后的两份清单

## 需要的 token 权限

`CLOUDFLARE_API_TOKEN` 建议用自定义 token，最小权限：

- `Account` → `Workers Scripts` → `Edit`：创建与更新 Worker 版本
- `Account` → `Workers KV Storage` → `Edit`：仅当后续需要 KV 时才要，当前可以不给
- `Zone` → `Workers Routes` → `Edit`：仅当绑定自定义域名时才要，当前可以不给

另外需要 `CLOUDFLARE_ACCOUNT_ID`，在 Cloudflare 面板的账号首页右侧可以取到

## 本地预览

```bash
cd deploy/cloudflare
npx wrangler@4 dev
```

`wrangler dev` 同样读 `public/`，改完清单文件刷新即可看到结果

## 访问方式

首次 `wrangler deploy` 会创建 Worker 并给出 `*.workers.dev` 地址，`registry.json` 与 `manifest.json` 都在该地址根路径下

宿主端拉取插件注册表时用这个地址；换自定义域名只需在 Cloudflare 面板加一条 Workers Route，本文件不用改。客户端更新走的是 Release 资产里的 `manifest.json`，不依赖本 Worker。
