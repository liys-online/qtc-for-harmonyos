# Harmony 插件 — 架构与设计说明

| 项 | 说明 |
|----|------|
| **适用范围** | `src/plugins/harmonyos`（Qt Creator 插件 **Harmony**） |
| **对标参照** | Qt Creator 内置插件 `src/plugins/android`（能力划分与扩展点习惯） |
| **维护** | 架构变更时同步更新本文档；版本策略见根目录 [VERSIONING.md](../VERSIONING.md) |

---

## 1. 概述

本插件在 Qt Creator 中提供 **Qt for HarmonyOS / OpenHarmony** 相关能力：SDK 与工具链配置、Kit/Qt 版本与工具链联动、基于 **CMake** 的工程构建，以及通过 **hvigor / ohpm** 生成 **HAP**，经 **hdc** 部署与启动应用。

插件**不**在进程内重新实现 hvigor、ohpm、hdc 协议；通过**可配置路径**调用已安装的官方或本地工具链，与 Android 插件调用 adb/Gradle 的方式一致。

---

## 2. 设计目标与非目标

### 2.1 目标

| 编号 | 目标 | 说明 |
|------|------|------|
| G1 | 主链路可用 | 配置 → Kit → 构建 → 部署 → 运行（及后续调试扩展），失败可诊断 |
| G2 | 与 Android 插件对称 | 模块职责、Qt Creator 扩展点用法与用户心智尽量对齐 |
| G3 | 可演进 | Qt Creator 大版本升级通过版本宏与可选源码隔离（见 [VERSIONING.md](../VERSIONING.md)、[../src/compat/README.md](../src/compat/README.md)） |
| G4 | 边界清晰 | IDE 内逻辑与外部 CLI/SDK 职责分离，便于测试与问题定位 |

### 2.2 非目标（当前阶段）

- 不替代 **DevEco Studio** 的全部能力（如完整模拟器生命周期、可视化资源编辑等）。
- 不承诺与**所有**发行版工具链逐版本锁定兼容；通过检测与文档管理期望。
- 部分能力依赖上游（调试协议、SDK CLI、模拟器接口），**可能无法与 Android 插件完全等价**，以 [PRIORITY-PLAN.md](PRIORITY-PLAN.md) 中「风险/可能无法实现」列及 [COMPARISON-PROGRESS.md](COMPARISON-PROGRESS.md) 为准。

---

## 3. 设计原则

1. **扩展点优先**：使用 Qt Creator 已有的 `IPlugin`、`Toolchain`、`QtVersion`、`IDevice`、`BuildStep`、`DeployConfiguration`、`RunConfiguration` 等机制。
2. **失败可观测**：构建与部署错误进入输出面板与 **Issues**（`TaskHub`），关键路径使用可分类日志（`QLoggingCategory`）。
3. **最小可用路径先行**：优先闭环真机 + CMake + HAP + hdc；再扩展模拟器、深度调试、SDK 商店式管理等。

---

## 4. 逻辑分层

```
Qt Creator 核心 / 公共插件（ProjectExplorer、CMakeProjectManager、QtSupport 等）
        │
        ▼
Harmony 插件（目标 Harmony）
  ├── 配置与持久化（SDK、DevEco 工具路径等）
  ├── Qt 版本与 OHOS 工具链
  ├── 设备（hdc 枚举、USB 辅助刷新）
  ├── CMake 构建配置扩展
  ├── BuildStep：HAP 构建（hvigor / ohpm）
  ├── DeployStep：hdc 安装
  └── RunConfiguration / RunWorker：应用启动
        │
        ▼
外部工具：Node、hvigorw、ohpm、hdc、Qt for Harmony、OHOS NDK 等
```

---

## 5. Qt Creator 扩展点与数据流

### 5.1 主要扩展点（与 Android 对照）

| 扩展点 | Harmony 侧用途 |
|--------|----------------|
| `IPlugin` | 注册工厂、设置页、信号连接 |
| `QtVersion` / `QtVersionFactory` | 识别支持 Harmony 的 Qt |
| `Toolchain` / `ToolchainFactory` | OHOS NDK / Clang 交叉工具链 |
| `IDevice` / `IDeviceFactory` | 物理设备等 |
| `BuildStep` | HAP 构建步骤 |
| `DeployConfiguration` / `BuildStep`（部署列表） | hdc 安装等 |
| `RunConfiguration` / `RunWorkerFactory` | 运行与（后续）日志、调试衔接 |

> 说明：Android 侧部分能力由独立 **KitAspect** 承载；Harmony 侧部分字段现分布在配置与 Kit 自动创建流程中，是否在独立 `KitAspect` 中收敛见 [PRIORITY-PLAN.md](PRIORITY-PLAN.md)。

### 5.2 构建 → 部署（概要）

1. **构建**：`HarmonyBuildConfiguration` 与 CMake 协同；`HarmonyBuildHapStep` 通过 **QtTaskTree** 执行 hvigor sync、ohpm install、assembleHap 等子步骤。  
2. **部署**：`HarmonyDeployQtStep` 解析目标设备与 HAP 产物路径，调用 **hdc install**。  
3. **运行**：`HarmonyRunConfiguration` 生成启动命令；由 **RunWorker**（如 `ProcessRunnerFactory` 或后续自定义 Recipe）执行。

与 Android 的 **Gradle → APK → adb** 形态对应；差异在构建编排（hvigor）与设备通道（hdc）。

---

## 6. 设备与并发模型

- **HarmonyDeviceManager**：基于 `hdc list targets` 等刷新设备列表；可结合 USB 事件触发全量刷新。
- **HarmonyDevice**：实现 `IDevice`，承载序列号、ABI、API 等扩展数据。
- **长任务**：Qt Creator **19+** 使用 **QtTaskTree** + `Utils::ProcessTask`；避免在含 `Layouting::Group` 等符号的 `.cpp` 中全局 `using namespace QtTaskTree`，以防与 `Group` 歧义。

---

## 7. 配置与持久化

- 全局选项通过 `Core::ICore::settings()` 持久化（与 Android 插件同类模式）。
- Kit、构建目录与工程侧约定通过 **BuildConfiguration** / **ProjectNode** 扩展数据等与 CMake 输出目录关联。

---

## 8. 安全与命令执行（后续）

外部命令应校验路径与参数，降低命令注入风险；设备侧操作失败时给出可操作的错误信息。

---

## 9. 已知限制（对外简要说明）

以下条目为当前产品化过程中的**公开说明**，细节任务见 [PRIORITY-PLAN.md](PRIORITY-PLAN.md)。

- **调试**：与 Android 侧 LLDB/调试 Worker 的深度集成依赖工具链与 IDE 调试插件配合，可能仅能分阶段提供。
- **模拟器**：依赖厂商 CLI 与 hdc 能力，可能与 AVD 式体验不对等。
- **SDK 包管理**：OpenHarmony/DevEco 与 Android SDK Manager 模型不同，未必能实现同等 UI。
- **签名**：部分场景需 DevEco 或官方签名流程，插件内可能仅提供引导或简化配置。
- **跨平台**：宿主工具链（如 Make）与 DevEco 安装路径在不同 OS 上需分别验证。

---

## 10. 相关文档

| 文档 | 用途 |
|------|------|
| [README.md](README.md) | 文档索引与阅读路径 |
| [MODULES.md](MODULES.md) | 源码模块与文件索引 |
| [OPERATIONS.md](OPERATIONS.md) | 构建、部署、运行与外部依赖 |
| [ANDROID-PARITY.md](ANDROID-PARITY.md) | 与 Android 插件模块映射（概要） |
| [COMPARISON-PROGRESS.md](COMPARISON-PROGRESS.md) | 功能对标矩阵与进度 |
| [PRIORITY-PLAN.md](PRIORITY-PLAN.md) | 优先级任务计划表 |
