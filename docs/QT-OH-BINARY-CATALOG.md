# Qt for OpenHarmony 二进制发布清单 — JSON 协议（设计稿 v1）

> **目的**：不依赖 Gitee/GitCode 的 Releases REST API（列表不全、匿名限制、字段差异），改由**一份可托管在任意 HTTPS 源上的 JSON 清单**描述「有哪些二进制包」及「直链下载地址」。  
> **范围**：供 **Qt Creator Harmony 插件**（`harmonyqttsdkmanagerdialog` 等）拉取并展示；维护方可由 CI 从 GitCode/Gitee 发布页生成，或手写维护。  
> **状态**：**协议设计稿**；插件侧加载与设置项可在后续迭代实现，本文件先固定字段语义与版本策略。

---

## 1. 设计原则

| 原则 | 说明 |
|------|------|
| **显式模式** | 用 `schema` + `schemaVersion` 标识格式，便于兼容升级。 |
| **最小可用** | v1 只要求「发布单元 + 资源名 + 下载 URL」；校验、镜像、平台标签为可选。 |
| **UTF-8 JSON** | 文件编码 **UTF-8**，换行不限；布尔用小写 `true`/`false`。 |
| **时间与 URL** | 时间建议 **ISO 8601**（UTC 带 `Z`）；URL 建议 **HTTPS**。 |

---

## 2. 顶层对象

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `schema` | string | **是** | 固定为 `qt-for-openharmony.binary-catalog`，用于与其它 JSON 区分。 |
| `schemaVersion` | integer | **是** | 本协议版本；当前为 **1**。阅读方**必须**拒绝不认识的 major（见 §6）。 |
| `catalogId` | string | 否 | 清单逻辑 ID（如 `openharmony-sig/qt`），便于缓存与多源切换。 |
| `title` | string | 否 | 人类可读标题，用于 UI 或日志。 |
| `updatedAt` | string | 否 | 清单本身更新时间，ISO 8601。 |
| `homepage` | string | 否 | 项目或发布页 URL（展示用，非下载列表）。 |
| `releases` | array | **是** | 发布条目列表；允许空数组。 |

---

## 3. `releases[]` 发布条目

表示一个「版本线」或「一次发布」（对标 GitHub/Gitee 的 *Release*）。

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `id` | string | 否 | 稳定唯一键（建议与 `tag` 一致或 ULID）；用于去重/缓存。 |
| `tag` | string | **是** | 版本标签（如 `v6.5.3-oh1`）；插件解压目录名等可由此派生。 |
| `qtVersion` | string | 否 | **上游 Qt 版本号**，与 OpenHarmony 后缀无关；建议 **`major.minor.patch`**（如 `6.6.0`），**不要**加前缀 `v`。用于 Kits/工具链展示、排序或与 `qmake -query QT_VERSION` 对照；缺省时 UI 可尝试从 `tag`/`title` 解析。 |
| `title` | string | 否 | 短标题；缺省时 UI 可用 `tag`。 |
| `createdAt` | string | 否 | 该发布创建时间，ISO 8601；用于排序（新→旧）。 |
| `summary` | string | 否 | 短说明（纯文本或 Markdown，由 UI 决定是否渲染）。 |
| `notesUrl` | string | 否 | 完整发行说明外链。 |
| `channel` | string | 否 | 通道：`stable` / `beta` / `snapshot` 等；未知值忽略。 |
| `assets` | array | **是** | 可下载文件列表；允许空数组（表示「占位发布」）。 |

---

## 4. `releases[].assets[]` 资源（二进制包）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `name` | string | **是** | 文件名（含扩展名），用于展示与本地保存名建议。 |
| `downloadUrl` | string | **是** | **直链**；302/301 到最终文件亦可，客户端跟随重定向。 |
| `size` | integer | 否 | 字节数；≥ 0。 |
| `sha256` | string | 否 | 小写十六进制 SHA-256；有则插件下载后**应**校验。 |
| `contentType` | string | 否 | MIME，如 `application/gzip`；仅提示。 |
| `kind` | string | 否 | `archive` / `installer` / `metadata` 等；未知值当 `archive` 处理。 |
| `platform` | string | 否 | 宿主开发机（运行 Qt Creator 的机器）：`darwin` / `linux` / `windows`；插件列表按此过滤。可多值时用数组见 §5。 |
| `arch` | string | 否 | **OpenHarmony / HarmonyOS 目标侧** ABI 提示（与 `platform` 无关）：如真机 `aarch64`（常见对应 `arm64-v8a`）、模拟器 `x86_64`，或 `universal`；**不是**宿主 CPU 架构。 |
| `mirrors` | array of string | 否 | 与 `downloadUrl` 等价的备用 URL；失败时可依次重试。 |

**约束**：

- 同一 `releases[].assets[]` 内建议 `name` 唯一；重复时阅读方保留**文档顺序中后者覆盖或合并策略应在实现中固定**（建议后者覆盖）。
- `downloadUrl` 与 `mirrors[]` 必须为绝对 URL。

---

## 5. 扩展约定（v1 已预留）

- **多平台 / 多目标**：若同一逻辑包拆成多条 `assets`，用 `platform`（宿主 OS）与 `arch`（OH 目标 ABI）区分；不要用一条 asset 里把 `platform` 设为数组——**v1 不支持数组**，多组合请多条 `assets`。
- **未来 v2**：可在顶层增加 `signature`（清单签名）、`minIdeVersion` 等；在 `schemaVersion` 递增时在本文件追加「变更记录」。

---

## 6. 版本与兼容性

| 规则 | 说明 |
|------|------|
| **相同 `schema`** | `schemaVersion` **增大**：阅读方须向后兼容已存在字段；新增字段可选。 |
| **`schemaVersion` 更大且阅读方只支持 v1** | 应提示用户升级插件，**不得**静默按 v1 解析（避免字段语义变化导致装错包）。 |
| **`schema` 不同** | 直接拒绝，提示非本清单类型。 |

---

## 7. 完整示例（最小 + 典型）

### 7.1 最小合法清单

```json
{
  "schema": "qt-for-openharmony.binary-catalog",
  "schemaVersion": 1,
  "releases": [
    {
      "tag": "v6.5.3-oh1",
      "qtVersion": "6.5.3",
      "assets": [
        {
          "name": "qt-6.5.3-openharmony-mac.tar.gz",
          "downloadUrl": "https://example.com/dist/qt-6.5.3-openharmony-mac.tar.gz"
        }
      ]
    }
  ]
}
```

### 7.2 带校验与镜像

```json
{
  "schema": "qt-for-openharmony.binary-catalog",
  "schemaVersion": 1,
  "catalogId": "openharmony-sig/qt",
  "title": "Qt for OpenHarmony — public binary catalog",
  "updatedAt": "2026-03-20T10:00:00Z",
  "homepage": "https://gitcode.com/openharmony-sig/qt/releases",
  "releases": [
    {
      "id": "v6.5.3-oh1",
      "tag": "v6.5.3-oh1",
      "qtVersion": "6.5.3",
      "title": "Qt 6.5.3 for OpenHarmony",
      "createdAt": "2025-11-01T12:00:00Z",
      "channel": "stable",
      "summary": "First stable drop for OH SDK 20.",
      "assets": [
        {
          "name": "qt-6.5.3-oh1-linux-x64.tar.xz",
          "downloadUrl": "https://cdn.example.com/qt/6.5.3/qt-6.5.3-oh1-linux-x64.tar.xz",
          "mirrors": [
            "https://mirror.example.org/qt/6.5.3/qt-6.5.3-oh1-linux-x64.tar.xz"
          ],
          "size": 2147483648,
          "sha256": "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789",
          "kind": "archive",
          "platform": "linux",
          "arch": "x86_64"
        }
      ]
    }
  ]
}
```

---

## 8. 与既有 API 模型的对应关系（便于迁移）

| Gitee/GitCode Release API | 本清单 |
|---------------------------|--------|
| `tag_name` | `releases[].tag` |
| （API 无直接字段） | `releases[].qtVersion`：由生成器从 tag/说明解析或构建系统显式写入 |
| `name` | `releases[].title` |
| `created_at` | `releases[].createdAt` |
| `body` | `releases[].summary` 或外链 `notesUrl` |
| `assets[].name` | `assets[].name` |
| `assets[].browser_download_url` | `assets[].downloadUrl` |

生成器可从 API 拉全量后**转写**为本 JSON，静态托管（对象存储、Git 仓 raw、内网网关均可）。

---

## 9. 安全与运营建议

- 清单 URL 应使用 **HTTPS**；企业内网可例外但需在文档中声明风险。
- 提供 `sha256` 时，**发布流程应保证清单与对象存储内容一致**（CI 计算后写入清单）。
- 可在一段时间内**同时保留**「API 拉取」与「清单 URL」两种数据源，由设置项或默认优先级决定（实现阶段再定）。

---

## 10. 文档维护

- 协议变更：递增 `schemaVersion`，并在本节追加变更条目。  
- **v1（2026-03-20）**：初版字段集。  
- **v1（修订）**：`releases[]` 增加可选字段 **`qtVersion`**（Qt `major.minor.patch`）；仍为 `schemaVersion: 1`，阅读方将未知字段忽略即可。

### 仓库内示例文件

- **规范文件名**：[`qt-oh-binary-catalog.v1.json`](qt-oh-binary-catalog.v1.json) — 当前内容为自 GitCode API 导出的 **真实清单**（仅 `Alpha_v7` 及以后 tag，直链与 `summary` 与仓库 Release 一致）。  
  - **推荐**：在开启 **`WITH_TESTS`** 的 CMake 构建中编译目标 **`qt-oh-binary-catalog-generator`**，用环境变量 **`GITCODE_PRIVATE_TOKEN`** 调用生成器覆盖该文件（说明见 [`test/README.md`](../test/README.md)）。  
  - **手工**：`GET https://api.gitcode.com/api/v5/repos/openharmony-sig/qt/releases`（分页至 `total_page`）自行转写。

### 方案 B：与本插件仓库同仓托管（GitCode / GitHub raw）

清单放在本仓库 **`docs/qt-oh-binary-catalog.v1.json`**（或你 monorepo 中的等价路径），推送后由 **GitCode raw** 与 **GitHub raw** 提供匿名 HTTPS。

**Qt Creator 插件内置默认**（见 `ohosconstants.h`）：

| 角色 | 当前默认 URL |
|------|----------------|
| **主源** | `https://raw.gitcode.com/Li-Yaosong/qtc-for-harmonyos/raw/main/docs/qt-oh-binary-catalog.v1.json` |
| **备用** | `https://raw.githubusercontent.com/liys-online/qtc-for-harmonyos/refs/heads/main/docs/qt-oh-binary-catalog.v1.json` |

环境变量 **`QT_OH_BINARY_CATALOG_URL`** 可覆盖**主源**；备用仍为内置 GitHub。设置页**不**提供 URL 配置项。

- 下载器拉清单**不需要** token；token 仅用于本机/CI 运行 **`qt-oh-binary-catalog-generator`** 更新该 JSON。

---

## 相关文档

- [SDK-PACKAGE-MANAGER.md](SDK-PACKAGE-MANAGER.md) — Qt OH SDK 管理器现状（Gitee/GitCode）与包管理行为。  
- [MODULES.md](MODULES.md) — 源码模块索引。
