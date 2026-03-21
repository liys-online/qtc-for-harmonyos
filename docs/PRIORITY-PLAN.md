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
| P0-02 | P0 | 运行 | `postStartShellCmd` 生命周期 | 进程结束后或用户 Stop 时执行设备侧命令（与 Android 行为对齐） | P0-01 选型（自定义 RunWorker） | 同上 | 完全对齐需自定义 RunControl Recipe；纯一条 `hdc shell` 难以覆盖 | 待开始 |
| P0-03 | P0 | 运行 | 默认 Ability / Bundle 可配置 | UI 或从 `module.json5` 读取默认 `-a/-b`，避免写死 `EntryAbility` | 工程模板约定 | `AndroidRunConfiguration` aspects | 多 Ability 工程需用户选择或解析配置 | 待开始 |
| P0-04 | P0 | 部署 | HAP 路径策略固化 | 从 `build-profile`/hvigor 输出约定解析优先；再 fallback 扫描；无产物时明确报错 | 已部分实现扫描 | `androiddeployqtstep.cpp` | 不同 hvigor 版本输出路径变更时需持续适配 | 部分完成 |
| P0-05 | P0 | 部署 | 部署前校验 hdc / 设备 / 产物 | `init()` 中统一 `reportWarningOrError` | — | 同上 | — | 已完成 |
| P0-06 | P0 | 构建 | `HarmonyBuildHapStep` 失败路径用户可见 | 配置缺失、ohpm/hvigor 失败进入 TaskHub + 输出面板 | — | `androidbuildapkstep.cpp` | — | 已完成 |
| P0-07 | P0 | 工具链 | `makeCommand()` 非 Windows | macOS/Linux 返回 `make`/`ninja`，不写死 `mingw32-make` | — | `androidtoolchain.cpp` | — | 待开始 |
| P0-08 | P0 | 工具链 | 父工具链同步非 Windows | `syncAutodetectedWithParentToolchains` 覆盖 Unix 宿主 | P0-07 | 同上 | — | 待开始 |
| P0-09 | P0 | 配置 | `kitsRestored` 未配置 SDK 引导 | InfoBar 跳转 Harmony 设置页（对标 Android） | 设置页 ID 稳定 | `androidplugin.cpp` | — | 待开始 |
| P0-10 | P0 | 配置 | `registerQtVersions()` 策略 | 恢复调用或删除死代码并文档说明 | — | `androidconfigurations.cpp` | 若与当前 Kit 策略冲突需产品决策 | 待开始 |
| P1-01 | P1 | 日志 | 全模块 `QLoggingCategory` | config/device/build/deploy/run 分类；环境变量或 Qt 日志规则可开关 | — | 各 `qCDebug` 用法 | — | 部分完成 |
| P1-02 | P1 | 日志 | 移除调试向 `writeSilently` | 仅保留用户必须感知的 Disrupting/输出面板 | P1-01 | — | — | 待开始 |
| P1-03 | P1 | 错误 | Deploy/Run 对话框父窗口 | `Core::ICore::dialogParent()` | — | — | — | 已完成 |
| P1-04 | P1 | 错误 | Deploy 自定义 HAP 按钮 early return | 每条 return 前有 Message/Task | — | `androiddeployqtstep.cpp` | — | 已完成 |
| P1-05 | P1 | 设备 | `HarmonyDeviceWidget::updateDeviceFromUi` | 可编辑项写回 `IDevice`（若需要） | — | `androiddevice.cpp` | 若无可编辑字段可标为 N/A 已搁置 | 待开始 |
| P1-06 | P1 | 设备 | `updateDeviceState` / Refresh | 调用 hdc 刷新单设备状态 | — | 同上 | hdc 无单设备 query 时需全量 list | 待开始 |
| P1-07 | P1 | 设备 | 设备图标资源 | 独立 Harmony 图标，移除 Android 资源引用 | 资源文件 | `androiddevice` 工厂 | — | 待开始 |
| P1-08 | P1 | 设备 | hdc 输出解析健壮性 | 版本化解析器 + 单测 mock | — | `adb` 解析思路 | hdc 文本格式变更 | 待开始 |
| P1-09 | P1 | 构建 | ohpm registry / strict_ssl 可配置 | 设置项或 `QSettings`，默认保持现状 | — | sdkmanager 代理思路 | 企业内网源需文档 | 待开始 |
| P1-10 | P1 | 构建 | `deviceTypes` 等非硬编码 | 来 Kit/设置/模板 | — | — | — | 待开始 |
| P1-11 | P1 | 常量 | `ohosconstants.h` 瘦身 | 未用 `Parameter` 迁出或删除 | — | `androidconstants.h` | 删除前确认无外部引用 | 待开始 |
| P1-12 | P1 | ID | 配置 ID 前缀统一与迁移 | `Harmony.*` + `fromMap` 读旧键 | — | — | 用户旧工程需迁移说明 | 待开始 |
| P1-13 | P1 | CMake BC | `HarmonyCMakeBuildConfiguration` 实用页签 | 与 iOS/Android CMake 扩展对齐的最小集 | — | iOS CMake BC | — | 待开始 |
| P1-14 | P1 | 解析 | hvigor/ohpm OutputTaskParser | Issues 可点击进源文件 | — | `javaparser.cpp` | 错误格式随工具升级变化 | 待开始 |
| P1-15 | P1 | Qt 版本 | `supportsMultipleQtAbis` 去硬编码 | 按版本范围或特性检测 | — | `androidqtversion.cpp` | — | 待开始 |
| P2-01 | P2 | 调试 | `HarmonyDebugWorkerFactory` | LLDB/GDB 远程或官方调试桥（若存在） | Run 稳定 | `androiddebugsupport.cpp` | **可能无法实现**：官方调试协议、权限、与 Qt Creator Debugger 插件耦合深 | 待开始 |
| P2-02 | P2 | 分析 | QML Profiler / Preview RunWorker | 若 Qt for Harmony 支持 QML 远程调试 | P2-01 | `androidqmltoolingsupport.cpp` | **可能无法实现**：取决于 Qt Harmony 运行时是否暴露同等端口 | 待开始 |
| P2-03 | P2 | SDK | SDK 包管理 UI | 安装/更新 API 包（若 CLI 可用） | — | `androidsdkmanager*.cpp` | **可能无法实现**：OpenHarmony/DevEco CLI 与 Google sdkmanager 不对等 | 待开始 |
| P2-04 | P2 | SDK | SDK 下载向导 | 对标首次配置下载 | P2-03 | `androidsdkdownloader.cpp` | 分发策略与网络合规 | 待开始 |
| P2-05 | P2 | 模拟器 | 枚举与启动 | hdc 或厂商模拟器命令 | — | `avdmanager`/`avdcreatordialog` | **可能无法实现**：无统一 CLI 或需 DevEco 独占 | 待开始 |
| P2-06 | P2 | 设备 | DeviceFileAccess（hdc file） | 浏览/推送文件（可选） | — | `AndroidFileAccess` | 性能与大文件限制 | 待开始 |
| P2-07 | P2 | 设备 | 端口转发 `hdc fport` | 为调试/网络调试预留 | — | adb forward | — | 待开始 |
| P2-08 | P2 | 签名 | Harmony 签名流程 UI | p7b/p12 或调用官方签名工具 | — | `keystorecertificatedialog` | **可能无法实现**：政策与工具仅 DevEco 内；可文档引导「DevEco 签名」 | 待开始 |
| P2-09 | P2 | 编辑器 | `app.json5` / `module.json5` 结构化编辑 | 只读校验 + 跳转或简单表单 | — | `androidmanifesteditor.cpp` | JSON5 与 Stage 模型复杂度高 | 待开始 |
| P2-10 | P2 | 语言服务 | ArkTS/ETS 对接 LSP（可选） | 外部 language server 配置页 | — | `javalanguageserver.cpp` | **可能无法实现**：无稳定开源 LSP 或授权限制 | 待开始 |
| P2-11 | P2 | 向导 | 新建 Harmony + CMake 工程向导 | 模板与 ohpro 生成 | — | `manifestwizard.cpp` | — | 待开始 |
| P2-12 | P2 | 模板 | `ohprojectecreator` 去个人路径、动态 API 表 | 资源与 `sdkVersionMap` | — | — | — | 待开始 |
| P2-13 | P2 | 测试 | 单元测试骨架 + hdc 输出 mock | `WITH_TESTS` 注册 | — | `sdkmanageroutputparser_test.cpp` | CI 需固定工具版本 | 待开始 |
| P2-14 | P2 | 运行 | 应用日志流（hilog）到输出面板 | RunWorker 附加 reader | P0-01 | logcat 思路 | hdc 缓冲与性能 | 待开始 |
| P3-01 | P3 | 研究 | `harmonydeployqt` 独立工具链 | 是否由 Qt 提供类似 androiddeployqt | Qt 商业/开源路线 | `androiddeployqt` | **可能无法实现**：无官方工具则长期仅用 hdc | 待开始 |
| P3-02 | P3 | 研究 | 与 DevEco 深度集成 | 协议、自动化 API | — | — | **可能无法实现**：封闭 IDE、无公开 API | 待开始 |
| P3-03 | P3 | 研究 | 应用内 Profiler（非 QML） | 系统工具 | — | — | **可能无法实现** | 待开始 |

---

## 4. 建议执行顺序（在同优先级内）

1. **P0-04 → P0-05 → P0-06**（部署与构建错误可见，与 Run 并行可接受）  
2. **P0-01 → P0-02 → P0-03**（运行体验）  
3. **P0-07 → P0-08**（跨平台工具链）  
4. **P0-09 → P0-10**（配置与 Kit）  
5. 进入 **P1** 按「日志→错误→设备→解析→常量/ID」顺序  
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
