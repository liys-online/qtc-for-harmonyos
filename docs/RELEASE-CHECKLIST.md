# Harmony 插件 — 发布检查清单（Release Checklist）

> **用途**：在标记版本、上传构建产物或撰写 Release Notes 前逐项核对，降低「环境与文档不一致、用户无法复现」类问题。  
> **适用范围**：当前以 **Qt Creator 19+** 外载插件（`-pluginpath`）为主；预发布（**alpha / beta / rc**）与正式版共用本表，按「优先级」裁剪执行深度。

---

## 1. 术语与发布层级

| 层级 | 典型语义 | 对用户预期 |
|------|-----------|------------|
| **alpha** | 功能主路径可试，边界与稳定性未承诺 | 明确标注 *prerelease*，鼓励反馈 issue |
| **beta** | 主路径稳定度提升，仍可能有 API/行为调整 | 可收窄支持矩阵，仍建议 *prerelease* |
| **rc** | 冻结范围，仅修阻塞缺陷 | 接近正式版，Release Notes 与元数据需与正式版一致 |
| **正式版**（如 1.0.0） | 在声明矩阵内可日常使用 | 需完整验收与向后兼容说明（`CompatVersion` 策略） |

---

## 2. 优先级说明

| 标记 | 含义 |
|------|------|
| **P0** | 未完成则**不建议**对外发版（或仅能发内部/极小规模试用） |
| **P1** | **强烈建议**完成；预发布可酌情省略个别项但须在 Release Notes 中声明 |
| **P2** | **可选**；提升专业度与可维护性 |

---

## 3. 版本与插件元数据（P0）

| # | 检查项 | 说明 / 参考 |
|---|--------|-------------|
| 3.1 | `Harmony.json.in` 中 **Version** / **CompatVersion** | **仅允许纯数字段**：`主版本[.次[.补丁[_第四段]]]`（例：`1.0.0`、`4.5_2`）。**不得**使用 `1.0.0-alpha` 等 SemVer 预发布后缀 — Qt Creator `PluginSpec::isValidVersion` 会拒绝加载。**对外**仍可用 Git Tag `v1.0.0-alpha`、Release 标题区分预发布 |
| 3.2 | **CompatVersion** 策略明确 | 通常与 **Version** 相同；正式版后若需兼容旧设置再单独评估是否降低 **CompatVersion** |
| 3.3 | **Description / License / Copyright / Vendor** 无占位符 | 与仓库 **LICENSE**、[AUTHORS.md](AUTHORS.md) 一致 |
| 3.4 | **Url / DocumentationUrl** 可访问 | 指向正确**分支或 tag**；文档入口建议链至本目录 [README.md](README.md) |
| 3.5 | 构建后生成的 **Harmony.json** 已复核 | 确认 CMake 替换 `${IDE_PLUGIN_DEPENDENCIES}` 等后内容正确 |

---

## 4. 构建可复现性（P0 / P1）

| # | 检查项 | 优先级 |
|---|--------|--------|
| 4.1 | 记录 **Qt Creator 精确版本**（x.y.z）及构建类型（RelWithDebInfo / Release） | P0 → 模板见 [BUILD-REPRODUCIBILITY.md](BUILD-REPRODUCIBILITY.md) **§5** |
| 4.2 | 记录 **CMake 命令行**（含 `CMAKE_PREFIX_PATH`、`IDE_VERSION`、`-DWITH_TESTS` 等） | P0 → 见 [BUILD-REPRODUCIBILITY.md](BUILD-REPRODUCIBILITY.md) **§1** |
| 4.3 | 确认插件二进制与 **宿主 Qt Creator 主版本**匹配（见 [VERSIONING.md](../VERSIONING.md)） | P0 |
| 4.4 | 各目标平台产物路径与 [README.md](../README.md)「运行（加载插件）」一致 | P1 |

---

## 5. 质量验收（P0 / P1）

| # | 检查项 | 优先级 |
|---|--------|--------|
| 5.1 | **干净环境**（或 `-temporarycleansettings`）下完成：偏好配置 → Kit → **Build HAP** → **Deploy** → **Run** | P0 |
| 5.2 | 在 **Release Notes 声明的支持矩阵**内至少各验证 1 次（OS / 设备或 hdc 场景） | P0 |
| 5.3 | 可选：`WITH_TESTS=ON` 构建并执行 **`qtcreator -test Harmony`**（若工程已接入） | P1 |
| 5.4 | 预发布：在 Notes 中写明 **不包含能力**（模拟器、hilog、与 Android 对等度等），指向 [PRIORITY-PLAN.md](PRIORITY-PLAN.md) | P1 → 已写入 [RELEASE-NOTES-1.0.0-alpha.md](RELEASE-NOTES-1.0.0-alpha.md)，发版时按需增删 |

---

## 6. 文档与对外信息一致性（P1）

> **维护记录**：已与 [PRIORITY-PLAN.md](PRIORITY-PLAN.md) 对照修订 [COMPARISON-PROGRESS.md](COMPARISON-PROGRESS.md) **§1.5**（`WITH_TESTS` 注册）、**§14.3**（P1-12 Store 键）、**§15**（配置/运行/调试总览与 P0-01、P2-03、LLDB MVP 表述）。

| # | 检查项 |
|---|--------|
| 6.1 | [OPERATIONS.md](OPERATIONS.md) 中依赖（DevEco、Node、Java、hdc、签名/调试 HAP 等）与当前实现无冲突 |
| 6.2 | [COMPARISON-PROGRESS.md](COMPARISON-PROGRESS.md) 与 [PRIORITY-PLAN.md](PRIORITY-PLAN.md) 关键状态无互相矛盾 |
| 6.3 | 插件根 [README.md](../README.md) 中 **Qt Creator 最低版本**、加载方式与本次发布一致；[USER-MANUAL.md](USER-MANUAL.md) 与索引 [README.md](README.md) 已链至本次范围 |
| 6.4 | Native 调试边界与 [DEBUG-TASKS.md](DEBUG-TASKS.md)、[HARMONY-LLDB-DEBUG.md](HARMONY-LLDB-DEBUG.md) 一致 |

---

## 7. 法律与品牌（P1）

| # | 检查项 |
|---|--------|
| 7.1 | 明确 **非 The Qt Company 官方插件**（README / AUTHORS / Release 首段） |
| 7.2 | 第三方组件与 **SPDX / 许可证**声明完整（若仓库有子模块或捆绑依赖） |
| 7.3 | **VendorId**、主页 URL 与长期维护身份一致 |

---

## 8. 分发与制品（P1 / P2）

| # | 检查项 | 优先级 |
|---|--------|--------|
| 8.1 | **Git Tag** 与 **Version** 命名规范一致（如 `v1.0.0-alpha.1`） | P1 |
| 8.2 | Release 正文含：**适用 Qt Creator 版本**、**构建平台**、**安装方式**（`-pluginpath`）、**已知限制** | P1 → 模板：[RELEASE-NOTES-1.0.0-alpha.md](RELEASE-NOTES-1.0.0-alpha.md) |
| 8.3 | 附件压缩包附 **SHA256** 校验和 | P2 |
| 8.4 | 多平台分开发布时，文件名标明 **平台 + Creator 主版本** | P2 |

---

## 9. 发布后（P2）

| # | 检查项 |
|---|--------|
| 9.1 | 监控 issue：归类「环境 / 文档 / 缺陷」，更新 [PRIORITY-PLAN.md](PRIORITY-PLAN.md) 或 FAQ（流程：[POST-RELEASE.md](POST-RELEASE.md)） |
| 9.2 | 若默认分支或文档 URL 变更，同步 **Harmony.json.in** 与 Release 链接（同上） |

---

## 10. 快速勾选摘要（预发布最小集）

适用于 **1.0.0-alpha** 类首次对外预发布：

- [x] **3.1–3.4** `Harmony.json.in` 已填版本、许可、说明与 URL（维护者发版前再核 **3.5** 构建产物内 `Harmony.json`）  
- [x] **4.1–4.3** 记录模板与命令已落库 → [BUILD-REPRODUCIBILITY.md](BUILD-REPRODUCIBILITY.md)（**发版时填写 §5**）  
- [ ] **5.1–5.2** 主路径烟测 → 按 [QA-MANUAL-SMOKE.md](QA-MANUAL-SMOKE.md) 执行并签字  
- [x] **5.4 + 8.2** Release Notes 草稿 → [RELEASE-NOTES-1.0.0-alpha.md](RELEASE-NOTES-1.0.0-alpha.md)（粘贴前补全构建矩阵）  
- [ ] **8.1** 创建并推送 **Git Tag**（可与 `Harmony.json` **Version** 不同：Tag 可用 `v1.0.0-alpha`，JSON 内须为 **`1.0.0`** 等合法格式）  
- [ ] **8.3**（可选）附件 **SHA256**  

---

## 11. 本仓库已配套的发布文档

| 文档 | 对应清单章节 |
|------|----------------|
| [BUILD-REPRODUCIBILITY.md](BUILD-REPRODUCIBILITY.md) | §4、§3.5 |
| [QA-MANUAL-SMOKE.md](QA-MANUAL-SMOKE.md) | §5.1–5.3 |
| [RELEASE-NOTES-1.0.0-alpha.md](RELEASE-NOTES-1.0.0-alpha.md) | §5.4、§8.2 |
| [POST-RELEASE.md](POST-RELEASE.md) | §9 |
| [AUTHORS.md](AUTHORS.md) | §3.3、§7 |
| [USER-MANUAL.md](USER-MANUAL.md) | §6（使用者文档）；发版 Release 可引用 |

---

**维护**：发布流程变更时更新本页；重大范围变化时同步 [docs/README.md](README.md) 索引与「最近更新」表。
