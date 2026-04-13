# Qt Creator 版本策略（19+ 基线，可扩展到 20+）

## 目标

- **最低**：Qt Creator **19.x**（`IDE_VERSION` 主版本 &lt; 19 时 configure **失败**）。
- **向前兼容**：20 及以后若 API 再变，在本框架内用 **宏分岔** 或 **按大版本追加 `.cpp`**，无需推翻工程结构。

## CMake 流程

1. `src/CMakeLists.txt` 依次包含：
   - `cmake/QtCreatorVersion.cmake` — 解析 `IDE_VERSION`，校验最低版本，设置 `HARMONY_IDE_VERSION_*`。
   - `cmake/HarmonyQtcOptionalSources.cmake` — 维护者可按大版本追加 `HARMONY_QTC_EXTRA_SOURCES`。
2. 可调缓存变量：`HARMONY_QTC_MIN_MAJOR`（默认 `19`），一般保持默认即可。

版本来源：`IDE_VERSION` 或环境变量 `QTC_HARMONY_IDE_VERSION`；未设置时假定 `19.0.0` 并 **WARNING**。

```bash
cmake -DCMAKE_PREFIX_PATH=/path/to/QtCreator -DIDE_VERSION=20.0.0 ...
```

## 编译期宏（目标 `Harmony`）

| 宏 | 含义 |
|----|------|
| `QTC_IDE_VERSION_MAJOR` / `MINOR` / `PATCH` | 来自当前配置的 Qt Creator 版本 |
| `HARMONY_QTC_USE_QTTASKTREE` | 当前恒为 `1`（19+ 基线）；若某大版本弃用可再在 CMake 中分岔 |

头文件 **`src/harmonyqtcdefs.h`** 提供：

- `QTC_IDE_AT_LEAST_MAJOR(20)`  
- `QTC_IDE_AT_LEAST(20, 1)`（主.次）

## 接到 Qt Creator 20 时怎么做？

1. **小改**：在现有 `.cpp` 里 `#include "harmonyqtcdefs.h"`，用 `#if QTC_IDE_AT_LEAST_MAJOR(20)` 包新/旧调用。
2. **大改**：在 `src/compat/` 增加 `foo_qtc20.cpp`，并在 **`cmake/HarmonyQtcOptionalSources.cmake`** 里按  
   `HARMONY_IDE_VERSION_MAJOR GREATER_EQUAL 20` 加入该文件。详见 **`src/compat/README.md`**。

## 运行

插件 **必须与加载它的 Qt Creator 大版本匹配**（同一主版本 SDK 编出）。多版本并存时，各用各自的 `-pluginpath`。

---

## `Harmony.json` 的 `Version` / `CompatVersion`

Qt Creator 内核校验插件版本字符串，**仅允许**形如 `主[.次[.补丁[_第四段数字]]]` 的纯数字段（源码见 `src/libs/extensionsystem/pluginspec.cpp` 中 `PluginSpec::isValidVersion`）。

- **合法**：`1`、`1.2`、`1.0.0`、`4.5_2`  
- **不合法**：`1.0.0-alpha`、`1.0.0-beta.1`（含字母或 `-` 预发布后缀会 **无法加载**）

预发布（alpha/beta）请用 **Git Tag / Release 名称** 与 **Description** 文案区分；JSON 内仍写 **`1.0.0`** 等合规版本号。

---

## 插件自身版本号升级策略

> 本节说明 `Harmony.json.in` 中 `Version` / `CompatVersion` 字段的**语义**与**变更规则**，与上文 Qt Creator 版本适配层（`IDE_VERSION`）相互独立。

### 版本格式

采用 **`MAJOR.MINOR.PATCH`** 三段纯数字（符合 Qt Creator `PluginSpec::isValidVersion`）：

| 段 | 含义 |
|---|---|
| **MAJOR** | 破坏性变更：要求用户重新配置 SDK/Kit，或正式弃用对某个 Qt Creator 主版本的支持 |
| **MINOR** | 新能力：新功能、新 UI、新偏好项；旧设置键仍可读（可无损迁移） |
| **PATCH** | 修复：仅缺陷修复、文档、翻译、性能改善；**不新增**设置键或公开 API |

### 各段变更触发条件

#### PATCH 升级（`1.0.x → 1.0.x+1`）

以下任意一项成立即可升 PATCH：

- 修复唯一问题（无新功能）
- 翻译、文档、注释更新
- 性能 / 健壮性改善但无可见行为变化
- 依赖版本（如 Qt Creator 最低次版本）无变化

> PATCH 期间 `CompatVersion` **保持不变**，`Version` 递增。

#### MINOR 升级（`1.x.0 → 1.x+1.0`，PATCH 归零）

满足以下**全部**条件时升 MINOR：

1. 新增功能或新增设置项；
2. 旧版本的设置文件仍可被新版本**无损迁移**读取（旧键通过 `HarmonyMigratingStringAspect` 或兼容代码处理）；
3. 不提升 Qt Creator 最低主版本要求。

> MINOR 升级后 `CompatVersion` 可保持 **上一个稳定 MINOR 的起始点**（如 `1.0.0`），显式表示"1.0.0 及以上的 .qtcreator/.json 文件格式兼容此版本"。**若本次 MINOR 引入无法向后迁移的键（已有键重命名且无迁移路径），则 `CompatVersion` 必须等于 `Version`。**

#### MAJOR 升级（`x.0.0 → x+1.0.0`，MINOR 与 PATCH 归零）

以下任意一项成立才升 MAJOR：

- 提升 Qt Creator **最低主版本**（如从 19 升到 20），同时**删除**旧版本的 `#if QTC_IDE_AT_LEAST_MAJOR` 兼容分支；
- **删除**已公开的设置键/运行配置选项，且无迁移路径；
- 工程文件格式（`.user` / `extraData`）发生不可前向迁移的破坏性变更。

> MAJOR 升级时 `CompatVersion` **必须等于** `Version`（重置兼容起点）。

---

### `CompatVersion` 决策表

| 情形 | Version | CompatVersion | 说明 |
|------|---------|---------------|------|
| 仅 PATCH 修复 | `1.0.1` | `1.0.0`（不变） | 旧插件打开的工程文件仍兼容 |
| MINOR 新功能，旧键有迁移 | `1.1.0` | `1.0.0` | 明确可加载 1.0.0 写入的文件 |
| MINOR 新功能，旧键**无**迁移 | `1.1.0` | `1.1.0` | 兼容起点重置 |
| MAJOR 破坏性 | `2.0.0` | `2.0.0` | 必须重新配置 |

---

### 预发布阶段编号规则

Qt Creator 只识别纯数字版本，因此**预发布阶段仅在 Git Tag 与 Release 标题中区分**，JSON 中写**目标正式版号**：

| 阶段 | `Harmony.json` Version | Git Tag 示例 | Description 字段标注 |
|------|-----------------------|-------------|---------------------|
| Alpha（公开测试，功能不完整） | `1.0.0` | `v1.0.0-alpha.1` | `…Early-access (alpha)…` |
| Beta（功能完整，稳定性提升中） | `1.0.0` | `v1.0.0-beta.1` | `…Beta quality…` |
| RC（冻结，仅修阻塞缺陷） | `1.0.0` | `v1.0.0-rc.1` | `…Release Candidate…` |
| 正式版 | `1.0.0` | `v1.0.0` | 去掉预发布后缀 |

同一正式版循环内（alpha → beta → rc → release），`Version` **保持相同**，每次打新 Git Tag 即可；正式版 GA 后再按上述规则判断下一次应升 PATCH / MINOR / MAJOR。

---

### Qt Creator 主版本与插件 MAJOR 的对应关系

| 插件 MAJOR | 支持 Qt Creator 主版本 | 备注 |
|---|---|---|
| **1** | 19（基线），兼容层覆盖至更高版本 | 现行；遇 20 新 API 用 `#if QTC_IDE_AT_LEAST_MAJOR(20)` 分岔 |
| **2**（规划） | 20+（弃 19 支持 **或** 引入 20 破坏性 API） | 届时视变更烈度决定是否触发 MAJOR |

---

## 相关文档

- 插件说明与文档索引：[docs/README.md](docs/README.md)
- 发布检查清单：[docs/RELEASE-CHECKLIST.md](docs/RELEASE-CHECKLIST.md)（§3 版本与插件元数据）
