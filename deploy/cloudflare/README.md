# 插件注册表的 Cloudflare 部署

托管 `registry.json` 的静态资源 Worker，产物由 CI 生成后直接上传，Cloudflare 侧全程不跑构建

## 为什么是零 Cloudflare 构建

- `wrangler deploy` 走 Direct Upload：本地读 `public/` 目录，把里面的文件作为静态资源直接推到 Cloudflare
- Worker 只声明 `[assets]`，没有 `main` 脚本入口，也没有 `build` 命令或 Git 集成
- 因此不消耗 Cloudflare 的构建次数与构建时长，构建全部发生在 GitHub Actions 里
- 与 Workers Builds 的区别：Workers Builds 会在 Cloudflare 侧检出仓库并跑一次构建，本方案不需要那条链路

## 目录

```text
deploy/cloudflare/
├── wrangler.toml   # 只声明 [assets]，directory 指向 public/
├── public/         # CI 生成，不入库
│   └── registry.json
└── README.md
```

`public/` 由 `scripts/gen_registry.py` 在 CI 里写出，本地与仓库里都不保留，避免生成物与源数据打架

## 生成与部署

```bash
# 生成注册表（需要能读插件仓库 Release 的 token）
export GITHUB_TOKEN=...
python scripts/gen_registry.py \
  --repo CrimsonSeraph/DG-LAB-Client-Plugin \
  --modules-dir module \
  --out deploy/cloudflare/public/registry.json

# 部署静态资源
cd deploy/cloudflare
npx wrangler@4 deploy
```

`.github/workflows/plugin-release.yml` 的 `publish` job 会自动完成这两步

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

`wrangler dev` 同样读 `public/`，改完 `registry.json` 刷新即可看到结果

## 访问方式

首次 `wrangler deploy` 会创建 Worker 并给出 `*.workers.dev` 地址，`registry.json` 就在该地址根路径下

宿主端拉取注册表时用这个地址；换自定义域名只需在 Cloudflare 面板加一条 Workers Route，本文件不用改
