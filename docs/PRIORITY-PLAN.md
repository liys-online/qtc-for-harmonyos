# Harmony 插件 — 按优先级执行计划表（对标 Android，可逐项勾选）

> **用途**：与 Android 插件能力对齐的**主计划表**；后续按表中顺序逐项实施，完成后更新 **状态** 与 **备注**。  
> **关联**：[COMPARISON-PROGRESS.md](COMPARISON-PROGRESS.md)（功能对标矩阵）、[ARCHITECTURE.md](ARCHITECTURE.md)（架构与已知限制）、[docs/README.md](README.md)（文档索引）。

---

## 1. 优先级定义

| 级别 | 含义 | 建议节奏 |
|------|------|----------|
| **P0** | 主链路阻塞：无则「配置→构建→部署→运行」不可用或严重不可用 | 必须先闭环 |
| **P1** | 产品级体验：错误可见、可诊断、跨平台、与 Android 日常开发体验差距明显处 | 第二轮 |
| **P2** | 增强：调试、SDK 管理、编辑器、向导、测试 | 第三轮 |
| **P3** | 研究/长期：依赖上游工具链或 IDE API，**可能无法实现或仅部分实现** | 评估后纳入或标注放弃 |

---

## 2. 状态列约定

| 状态 | 含义 |
|------|------|
| 待开始 | 未动工 |
| 进行中 | 已开 PR/分支 |
| 部分完成 | 已有实现，仍差子项（在备注写清） |
| 已完成 | 主路径验收通过 |
| 阻塞 | 依赖外部条件（在「风险/可能无法实现」说明） |
| 已搁置 | 经评估不做或只做文档说明（须写原因） |

---

## 3. 总表（按 P0→P3 排序，功能尽量齐全）

> **说明**：「Android 参照」为 `src/plugins/android` 下典型文件，便于对照阅读。  
> **风险/可能无法实现**：若后期确认做不到，在本列补充**原因**（工具链未开放、无 CLI、License、Qt Creator API 限制等），并将状态改为 **已搁置**。

| ID | 优先级 | 模块 | 任务 | 交付物 / 验收要点 | 依赖 | Android 参照 | 风险 / 可能无法实现 | 状态 |
|----|--------|------|------|-------------------|------|----------------|---------------------|------|
| P0-01 | P0 | 运行 | RunWorker 与启动命令完整闭环 | `Run` 可执行；失败有 Task/输出；支持取消；与 Kit 设备一致 | 已有 `ProcessRunnerFactory` 时可升级为自定义 Recipe | `androidrunner.cpp`、`androidrunnerworker.cpp` | 若仅 `ProcessRunner` 无法表达「停止后 post shell」，需自定义 Worker（非阻塞） | 部分完成 |
| P0-02 | P0 | 运行 | `postStartShellCmd` 生命周期 | hdc 会话结束（含用户 Stop）后按行执行 Post-quit 命令；见 `harmonyrunner.cpp` | P0-01 | 同上 | — | 已完成 |
| P0-03 | P0 | 运行 | 默认 Ability / Bundle 可配置 | 运行配置两项覆盖 + `defaultHarmonyAbilityName()` 解析 `module.json5`（entry 模块优先） | 工程模板约定 | `AndroidRunConfiguration` aspects | 复杂 JSON5/多 entry 需用户覆盖 | 已完成 |
| P0-04 | P0 | 部署 | HAP 路径策略固化 | `findBuiltHapPackage()`：`build-profile.json5` modules（entry 优先）→ `entry/…` → 树内最新 `*.hap`；部署错误附诊断轨迹 | — | `androiddeployqtstep.cpp` | hvigor 输出路径若再变，可继续加候选文件名 | 已完成 |
| P0-05 | P0 | 部署 | 部署前校验 hdc / 设备 / 产物 | `init()` 中统一 `reportWarningOrError` | — | 同上 | — | 已完成 |
| P0-06 | P0 | 构建 | `HarmonyBuildHapStep` 失败路径用户可见 | 配置缺失、ohpm/hvigor 失败进入 TaskHub + 输出面板；另含 Node/Java 解析、ohpro 目录与 `PWD`/`INIT_CWD`（见 §7） | — | `androidbuildapkstep.cpp` | — | 已完成 |
| P0-07 | P0 | 工具链 | `makeCommand()` 非 Windows | macOS/Linux 在 PATH 中解析 `make`/`gmake`（对齐 GCC `mingwAwareMakeCommand` 思路） | — | `androidtoolchain.cpp` / `gcctoolchain.cpp` | — | 已完成 |
| P0-08 | P0 | 工具链 | 父工具链同步（Windows MinGW） | `syncAutodetectedWithParentToolchains` 使用 `HARMONY_TOOLCHAIN_TYPEID`（原误用 `CLANG_TOOLCHAIN_TYPEID` 导致从不执行）；Unix 仍与上游 Clang 一致不挂 MinGW | P0-07 | `gcctoolchain.cpp` | — | 已完成 |
| P0-09 | P0 | 配置 | `kitsRestored` 未配置 SDK 引导 | InfoBar：缺 SDK / 缺 Harmony Qt 两种情形 → Harmony 设置；全无则不提示 | 设置页 ID 稳定 | `androidplugin.cpp` | — | 已完成 |
| P0-10 | P0 | 配置 | `registerQtVersions()` 策略 | `syncToolchainsQtAndKits()` 统一：`applyConfig` / `kitsLoaded`（无 SDK 自增时）/ `qtVersionsChanged` / `addQmake`/`removeQmake` | — | `androidconfigurations.cpp` | — | 已完成 |
| P1-01 | P1 | 日志 | 全模块 `QLoggingCategory` | config/device/build/deploy/run 分类；环境变量或 Qt 日志规则可开关 | `harmonylogcategories.*`；`usbmonitor` 内 `qtc.harmony.device.usbmonitor` | 各 `qCDebug` 用法 | — | 已完成 |
| P1-02 | P1 | 日志 | 移除调试向 `writeSilently` | 仅保留用户必须感知的 Disrupting/输出面板 | P1-01 | — | Kit 侧 **OHOS_ARCH** 纠正改为 `writeFlashing` | 已完成 |
| P1-03 | P1 | 错误 | Deploy/Run 对话框父窗口 | `Core::ICore::dialogParent()` | — | — | — | 已完成 |
| P1-04 | P1 | 错误 | Deploy 自定义 HAP 按钮 early return | 每条 return 前有 Message/Task | — | `androiddeployqtstep.cpp` | — | 已完成 |
| P1-05 | P1 | 设备 | `HarmonyDeviceWidget::updateDeviceFromUi` | 可编辑项写回 `IDevice`（若需要） | — | `androiddevice.cpp` | 若无可编辑字段可标为 N/A 已搁置 | 待开始 |
| P1-06 | P1 | 设备 | `updateDeviceState` / Refresh | `harmonydevice.cpp`：Refresh → `HarmonyDeviceManager::queryDevice()`，与 USB 热插拔共用 `hdc list targets -v`；`harmonyDeviceLog` 记触发设备 id | — | 同上 | hdc 无单设备 query 时需全量 list | 已完成 |
| P1-07 | P1 | 设备 | 设备图标资源 | 已移除 `:/android/images/androiddevice*.png`；现用 `:/projectexplorer/images/desktopdevice(.png|@2x.png)`（不依赖 Android 插件加载） | 资源文件 | `androiddevice` 工厂 | 独立 Harmony 品牌素材仍可后续替换 | 部分完成 |
| P1-08 | P1 | 设备 | hdc 输出解析健壮性 | `harmonyhdctargetsparser.*`：`parseHdcListTargetsLine` / 状态映射；`harmonydevicemanager` 仅消费结构化结果；`WITH_TESTS` 下 `harmonyhdctargetsparser_test`（mock 行，无 hdc）；运行：`qtcreator -test Harmony` | — | `adb` 解析思路 | 新 hdc 格式：增 QTest 数据行 + 必要时在解析器内加分支 | 已完成 |
| P1-09 | P1 | 构建 | ohpm registry / strict_ssl 可配置 | **Preferences → Harmony**：registry 行编辑 + strict SSL 复选框；`HarmonyConfig` 持久化；`HarmonyBuildHapStep::ohpmInstallTask` 使用 `effectiveOhpmRegistryUrl()`；非法 URL 构建前报错 | — | sdkmanager 代理思路 | 企业内网源见 [OPERATIONS.md](OPERATIONS.md) §2.2 | 已完成 |
| P1-10 | P1 | 构建 | `deviceTypes` 等非硬编码 | **Preferences**：默认 `module.json5` deviceTypes 为 **预设多选**（`ohModuleDeviceTypePresetIds`：phone/tablet/2in1/wearable/tv/car）；未选或空存储仍等价 **2in1**；**Kit**：`Harmony.ModuleDeviceTypes` 在 `updateAutomaticKitList` 同步；**构建步骤**：可选 **跟随偏好/Kit** 或 **预设多选覆盖**（持久化为 `Harmony.BuildOhModuleDeviceTypesLine` 逗号串）→ `effectiveModuleDeviceTypes()`；`harmonyutils` `parse/joinOhModuleDeviceTypesLine`；`createOhPro` 用全局默认 | — | — | 从已有工程 `module.json5` 自动推断仍待（可续 P2） | 已完成 |
| P1-11 | P1 | 常量 | `ohosconstants.h` 瘦身 | 未用 `Parameter` 迁出或删除 | — | `androidconstants.h` | 删除前确认无外部引用 | 待开始 |
| P1-12 | P1 | ID | 配置 ID 前缀统一与迁移 | **运行配置**：`settingsKey` 与 `setId` 字符串一致（如 `Harmony.AaStartArgs`、`Harmony.Run.BundleNameOverride`），`HarmonyMigratingStringAspect` / `BaseStringListAspect` 在 `fromMap` 读旧 `*Key` 后缀键；**HAP 步骤**：`Harmony.BuildHap.TargetSdk` / `Harmony.BuildHap.BuildToolsVersion`（旧 `BuildTargetSdk` / `BuildToolsVersion`）；**部署步骤**：`Harmony.Deploy.UninstallPreviousPackage`（旧 `UninstallPreviousPackage`）；`harmonyutils` 读 Bundle/Ability 覆盖时兼容旧键 | — | — | 打开并保存工程后即落盘新键；CMake 工程 `extraData`（如 `HarmonyApplicationArgs`）未改 | 已完成 |
| P1-13 | P1 | CMake BC | `HarmonyCMakeBuildConfiguration` 实用页签 | **Projects → Build**：`HarmonyCMakeSummaryWidget`（标题 **Harmony**）— Qt 版本、Kit NDK、`OHOS_ARCH`（Kit CMake + 工程 `extraData`）、`CMAKE_TOOLCHAIN_FILE`；按钮打开 **Harmony 偏好** / **当前 Kit**；**刷新**；顺序与 iOS 一致：Harmony 子页在 CMake 默认「Build Environment / Custom Parsers」之前 | — | `IosCMakeBuildConfiguration` | — | 已完成 |
| P1-14 | P1 | 解析 | hvigor/ohpm OutputTaskParser | Issues 可点击进源文件 | `harmonyhvigoroutputparser.*` + `HarmonyBuildHapStep::setupOutputFormatter` | `javaparser.cpp` | 错误格式随工具升级变化 | 已完成 |
| P1-15 | P1 | Qt 版本 | `supportsMultipleQtAbis` 去硬编码 | **`HarmonyQtVersion`**：`qtAbis().size() > 1`（随 qmake / `qt6ct` JSON 等解析结果变化，不再绑定 `5.15.12`） | — | `androidqtversion.cpp`（Android 仍用版本区间） | 单 ABI 安装恒为 `false` | 已完成 |
| P2-01 | P2 | 调试 | `HarmonyDebugWorkerFactory` | **§7.2 user + debug HAP + abstract** 已自动编排；**§7.1 root+TCP+fport 全自动不做**（零售机无 root） | **阶段 3–4** 见 [DEBUG-TASKS.md](DEBUG-TASKS.md) | `androiddebugsupport.cpp` | QML/其它语言、Run 链去重（4.5）仍待 | 部分完成 |
| P2-02 | P2 | 分析 | QML Profiler / Preview RunWorker | 若 Qt for Harmony 支持 QML 远程调试 | P2-01 | `androidqmltoolingsupport.cpp` | **可能无法实现**：取决于 Qt Harmony 运行时是否暴露同等端口 | 待开始 |
| P2-03 | P2 | SDK | SDK 包管理 UI | 安装/更新 API 包；HTTP 列表 + 下载（见 SDK-PACKAGE-MANAGER.md） | 设置页「Manage SDK Packages…」对话框 | `androidsdkmanager*.cpp` | 解压/自动注册 SDK 待做；端点可后续改设置项 | 部分完成 |
| P2-04 | P2 | SDK | SDK 下载向导 | 对标首次配置下载 | P2-03 | `androidsdkdownloader.cpp` | 分发策略与网络合规 | 待开始 |
| P2-05 | P2 | 模拟器 | 枚举与启动 | hdc 或厂商模拟器命令 | — | `avdmanager`/`avdcreatordialog` | **可能无法实现**：无统一 CLI 或需 DevEco 独占 | 待开始 |
| P2-06 | P2 | 设备 | DeviceFileAccess（hdc file） | 浏览/推送文件（可选） | — | `AndroidFileAccess` | 性能与大文件限制 | 待开始 |
| P2-07 | P2 | 设备 | 端口转发 `hdc fport` | **手工 / 其它场景**；**不与调试 RunWorker 自动串联**（零售机无 root，§7.1 全自动编排不做） | [DEBUG-TASKS.md](DEBUG-TASKS.md) 阶段 2、`harmonyhdcforward.*` | adb forward | hdc 子命令随版本变化 | 部分完成（API+文档） |
| P2-08 | P2 | 签名 | Harmony 签名流程 UI | p7b/p12 或调用官方签名工具 | — | `keystorecertificatedialog` | **可能无法实现**：政策与工具仅 DevEco 内；可文档引导「DevEco 签名」 | 待开始 |
| P2-09 | P2 | 编辑器 | `app.json5` / `module.json5` 结构化编辑 | 只读校验 + 跳转或简单表单 | — | `androidmanifesteditor.cpp` | JSON5 与 Stage 模型复杂度高 | 待开始 |
| P2-10 | P2 | 语言服务 | ArkTS/ETS 对接 LSP（可选） | 外部 language server 配置页 | — | `javalanguageserver.cpp` | **可能无法实现**：无稳定开源 LSP 或授权限制 | 待开始 |
| P2-11 | P2 | 向导 | 新建 Harmony + CMake 工程向导 | 模板与 ohpro 生成 | — | `manifestwizard.cpp` | — | 待开始 |
| P2-12 | P2 | 模板 | `ohprojectecreator` 去个人路径、动态 API 表 | 资源与 `sdkVersionMap` | — | — | — | 待开始 |
| P2-13 | P2 | 测试 | 单元测试骨架 + hdc 输出 mock | 除 P1-08 外其它模块 `*_test`、CI 矩阵仍待 | P1-08 | `sdkmanageroutputparser_test.cpp` | CI 需固定工具版本 | 部分完成 |
| P2-14 | P2 | 运行 | 应用日志流（hilog）到输出面板 | RunWorker 附加 reader | P0-01 | logcat 思路 | hdc 缓冲与性能 | 待开始 |
| P3-01 | P3 | 研究 | `harmonydeployqt` 独立工具链 | 是否由 Qt 提供类似 androiddeployqt | Qt 商业/开源路线 | `androiddeployqt` | **可能无法实现**：无官方工具则长期仅用 hdc | 待开始 |
| P3-02 | P3 | 研究 | 与 DevEco 深度集成 | 协议、自动化 API | — | — | **可能无法实现**：封闭 IDE、无公开 API | 待开始 |
| P3-03 | P3 | 研究 | 应用内 Profiler（非 QML） | 系统工具 | — | — | **可能无法实现** | 待开始 |

---

## 4. 建议执行顺序（在同优先级内）

1. **P0-04 → P0-05 → P0-06**（部署与构建错误可见，与 Run 并行可接受）— *P0-06 相关：hvigor 环境、路径与 cwd 已加固，见 §7*  
2. **P0-01 → P0-02 → P0-03**（运行体验）— *均已完成；下一阶段主线 **P0-04**（HAP 路径）等*  
3. **P0-07 → P0-08**（跨平台工具链）— *已完成：`harmonytoolchain` `makeCommand` + MinGW 父链 ID；Kit CMake `CMAKE_MAKE_PROGRAM` Unix 解析*  
4. **P0-09 → P0-10**（配置与 Kit）— *均已完成*  
5. 进入 **P1** 按「日志→错误→设备→解析→常量/ID」顺序（**P1-14** hvigor 输出进 Issues 可与构建体验衔接）  
6. **P2** 按业务价值选：解析器、设备图标、hilog、签名文档化、向导  
7. **P3** 仅评估，确认后转 P2 或 **已搁置**

---

## 5. 文档同步约定

- 完成某 **ID** 后：  
  - 本表 **状态** 改为「已完成」或「部分完成」并补 **备注**；  
  - [COMPARISON-PROGRESS.md](COMPARISON-PROGRESS.md) 对应行更新 ✅/🔄；  
  - 若放弃实现：[COMPARISON-PROGRESS.md](COMPARISON-PROGRESS.md) 备注 **已搁置 + 原因**，本表 **风险** 列写最终结论。

---

## 6. 修订记录

| 版本 | 日期 | 说明 |
|------|------|------|
| 0.1 | （填写） | 初版：P0–P3 总表 + 状态约定 |
| 0.2 | 2025-03-20 | P0-10→部分完成；新增 §7 近期落地；§4 标注下一阶段建议（P0-01～03、P1-14） |
| 0.3 | 2026-03-20 | **P1-14**：`HarmonyHvigorOhpmOutputParser`（hvigor/ohpm/ArkTS 常见行）+ `ohpro`/工程目录 `addSearchDir` |
| 0.4 | 2026-03-20 | **P1-01 / P1-02**：`harmonylogcategories` 统一分类；`HarmonyConfigurations` 调试输出改 `qCDebug`；OHOS_ARCH 用户提示改 `writeFlashing` |
| 0.5 | 2026-03-20 | **P2-01 / P2-07**：新增 [DEBUG-TASKS.md](DEBUG-TASKS.md) 分阶段任务清单；总表 P2-01/P2-07「落地」列引用 |
| 0.6 | 2026-03-20 | **DEBUG-TASKS 0.2**：§7 摘要增 `probeNativeDebugShellEnvironment`（`harmonyutils`） |
| 0.7 | 2026-03-20 | **DEBUG-TASKS 0.3 / 0.4**：`OPERATIONS.md` §2.4、§2.5（Native 调试构建约定与风险） |
| 0.8 | 2026-03-20 | **P2-07 / DEBUG-TASKS 2.x**：`harmonyhdcforward.*`，`HARMONY-LLDB-DEBUG` fport 模板 |
| 0.9 | 2026-03-20 | **P2-01 / DEBUG-TASKS 3.x MVP**：`harmonydebugsupport.*` + `HarmonyDevice` 调试端口；LLDB 由 Kit / `QTC_DEBUGGER_PATH` 配置（不修改 Creator 核心） |
| 1.0 | 2026-03-20 | **范围**：零售机无 root → **不做 §7.1 + fport 与调试 RunWorker 自动串联**；P2-01/P2-07 与 DEBUG-TASKS、HARMONY-LLDB-DEBUG 已同步说明。 |
| 1.1 | 2026-03-24 | **P1-06**：设备 Refresh 调用 `queryDevice()`；**P1-07**：工厂图标改 ProjectExplorer 通用设备图，去掉 Android 资源依赖（专属 Harmony 图标仍可选）。 |
| 1.2 | 2026-03-24 | **P1-08**：`harmonyhdctargetsparser.*` + `harmonyhdctargetsparser_test`（`extend_qtc_plugin` / `addTestCreator`）；`harmonydevicemanager` 消费解析结果。 |
| 1.3 | 2026-03-24 | **P1-09**：ohpm `--registry` / `--strict_ssl` 偏好设置 + `HarmonyConfig` 键；`OPERATIONS.md` §2.2 企业镜像说明。 |
| 1.4 | 2026-03-24 | **P1-10**：ohpro `deviceTypes` — 全局偏好、`Harmony.ModuleDeviceTypes` Kit 字段、`HarmonyBuildHapStep` 覆盖行 + `effectiveModuleDeviceTypes()`。 |
| 1.5 | 2026-03-24 | **P1-10（UI）**：偏好与 HAP 步骤的 deviceTypes 自单行输入改为 **预设复选框**；步骤侧增加「使用偏好与 Kit、不覆盖」；存储与 `effectiveModuleDeviceTypes()` 逻辑不变。 |
| 1.6 | 2026-03-24 | **P1-12**：`.user` / 工程内 **Harmony 构建·部署·运行** 配置 Store 键统一为 `Harmony.*` 规范名；`fromMap` 兼容旧键，**保存后**写入新键（见 [OPERATIONS.md](OPERATIONS.md)）。 |
| 1.7 | 2026-03-24 | **P1-13**：`HarmonyCMakeBuildConfiguration::createSubConfigWidgets` 增加 **Harmony CMake summary** 子页（`harmonybuildconfiguration.cpp` 内 `HarmonyCMakeSummaryWidget`）。 |
| 1.8 | 2026-03-24 | **P1-15**：`HarmonyQtVersion::supportsMultipleQtAbis` 改为 `qtAbis().size() > 1`，去掉对 `5.15.12` 的硬编码。 |

---

## 7. 近期落地摘要（实现已合入源码，未单独占 ID）

> 便于与 §3 总表对照；细节见 [OPERATIONS.md](OPERATIONS.md)、[COMPARISON-PROGRESS.md](COMPARISON-PROGRESS.md)。

| 主题 | 说明 |
|------|------|
| **配置持久化** | `HarmonyConfig` 变更路径/SDK/qmake 等时调用 `HarmonyConfigurations::persistSettings()`；qmake 增删后走 `syncToolchainsQtAndKits()`（与 `applyConfig` / `kitsLoaded` 同源） |
| **CMake / Kit** | `OHOS_ARCH` 规范化（避免 `unknown` / 非法 ABI）；Unix 可自动设 `CMAKE_MAKE_PROGRAM`；Windows 可用 Harmony 设置中的 MinGW `make` |
| **qdevice.pri** | `HarmonyQtVersion::targetAbi()` 跳过注释行、支持 `OHOS_ARCH=` 赋值，避免误解析 |
| **Node / hvigor** | `nodeLocation()`：`tools/node/node`、`tools/node/bin/node`、系统 PATH；错误提示更明确 |
| **Java / hvigor** | `javaLocation()`：macOS `jbr/Contents/Home`、`JAVA_HOME`、`/usr/libexec/java_home`、PATH；`HarmonyQtVersion::addToBuildEnvironment` 注入 `JAVA_HOME`；sync/ohpm/assemble 统一 `applyDevecoAndJavaEnv` |
| **uv_cwd / EPERM** | `prepareOhProDirectory()` + 环境变量 `PWD` / `INIT_CWD`；主进程 cwd 规范化 |
| **hvigor / ohpm → Issues** | `HarmonyBuildHapStep::setupOutputFormatter` 注册 `HarmonyHvigorOhpmOutputParser`；匹配 `*.ets`/`*.ts` 等 `path:line:col:`、`At file:`、`hvigor ERROR` / `ohpm ERROR` 等（详见源码注释） |
| **日志分类（P1-01）** | `harmonylogcategories.{h,cpp}`：`qtc.harmony.config|device|build|deploy|run|plugin|toolchain`；`QT_LOGGING_RULES` 可开 `*.debug=true` |
| **Native 调试前置（DEBUG-TASKS 0.2）** | `harmonyutils`：`probeNativeDebugShellEnvironment` / `nativeDebugRecipeKind`（`hdc shell id`、`getenforce`） |
| **Native 调试文档（DEBUG-TASKS 0.3–0.4）** | [OPERATIONS.md](OPERATIONS.md) §2.4（`-g`/strip/debug HAP）§2.5（user/签名风险与降级） |
| **hdc fport（P2-07 / DEBUG-TASKS 2）** | `harmonyhdcforward.*`；§7.1 模板见 [HARMONY-LLDB-DEBUG.md](HARMONY-LLDB-DEBUG.md)（**手工**，不与调试 RunWorker 自动串联） |
| **Debugger MVP（P2-01 / DEBUG-TASKS 3）** | `harmonydebugsupport.*`（仅 Harmony 插件内；**不修改** Debugger 核心源码） |
| **设备 Refresh + 图标（P1-06 / P1-07）** | `HarmonyDevice` 的 Refresh 动作 → `instance()->queryDevice()`；`HarmonyDeviceFactory::setCombinedIcon` 使用 ProjectExplorer `desktopdevice`（见 §3 P1-06/P1-07） |
| **hdc list targets 解析（P1-08）** | `parseHdcListTargetsLine` / `hdcListTargetsStateToConnectionState` 独立于 `HarmonyDeviceManager`；`WITH_TESTS` 下 mock 行单测；验收：`qtcreator -test Harmony`（需整树 `-DWITH_TESTS=ON` 编过） |
| **ohpm 源与 TLS（P1-09）** | 偏好设置 + `Harmony.OhpmRegistryUrl` / `Harmony.OhpmStrictSsl`；构建步骤 `ohpm install --registry` / `--strict_ssl` |
| **ohpro deviceTypes（P1-10）** | `Harmony.OhModuleDeviceTypes`（`QStringList`）；Kit `Harmony.ModuleDeviceTypes`；`Harmony.BuildOhModuleDeviceTypesLine`（逗号串）；**UI**：`HarmonySettingsWidget` 预设复选框 + `HarmonyBuildHapWidget`「跟随偏好/Kit」+ 覆盖复选框；`ohModuleDeviceTypePresetIds` / `parseOhModuleDeviceTypesLine` / `joinOhModuleDeviceTypesLine`（`harmonyutils`） |
| **工程 Store 键（P1-12）** | 运行 aspects：`Harmony.AaStartArgs` 等（旧 `Harmony.AaStartArgsKey` 等仍可读）；HAP：`Harmony.BuildHap.TargetSdk` / `Harmony.BuildHap.BuildToolsVersion`；部署：`Harmony.Deploy.UninstallPreviousPackage` |
| **CMake 构建配置 UI（P1-13）** | `HarmonyCMakeBuildConfiguration`：`HarmonyCMakeSummaryWidget`（只读摘要 + 快捷打开设置）；其后仍为通用 **Build environment** / **Custom parsers** |
| **多 ABI 判定（P1-15）** | `HarmonyQtVersion::supportsMultipleQtAbis`：以 **`qtAbis()`** 是否多于一项为准（与 qmake / 元数据一致） |
