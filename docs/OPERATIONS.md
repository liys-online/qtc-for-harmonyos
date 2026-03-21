# Harmony 插件 — 构建、部署与运行（运维说明）

本文档描述插件依赖的**外部工具**及**主流程**，供集成、测试与技术支持查阅。路线图与任务拆解见 [PRIORITY-PLAN.md](PRIORITY-PLAN.md)。

---

## 1. 外部依赖

| 组件 | 用途 | 典型来源 |
|------|------|----------|
| Node.js | 执行 hvigor、ohpm 脚本 | 官方运行时或 DevEco 捆绑目录 |
| hvigorw（hvigorw.js） | 工程同步与 HAP 构建 | DevEco / SDK 工具链目录 |
| ohpm（pm-cli.js） | 依赖安装 | 同上 |
| hdc | 设备通信、安装 HAP | SDK `toolchains` 等路径下 |
| CMake + Ninja/Make | IDE 侧生成与 native 构建 | 由 Kit / Qt Creator 提供 |
| Qt for Harmony | 交叉 Qt 与工程描述 | Qt 发行渠道 |

**约束**：插件通过**设置页与环境变量**解析路径，不在代码中写死绝对路径。

---

## 2. 构建管线（HAP）

### 2.1 输入

- 工程为 **CMake** 工程；由 Qt / Harmony 相关 CMake 包提供变量与目标。
- Kit 指定 **Qt 版本**、**OHOS 工具链**及目标 ABI（策略可与 Android 多 ABI 对齐演进）。

### 2.2 步骤（HarmonyBuildHapStep）

典型子步骤包括：

1. **hvigor sync**（校验工程与依赖）  
2. **ohpm install**（安装 ohpm 依赖）  
3. **hvigor assembleHap**（或等价产物任务）

标准输出/错误回传至 Qt Creator **构建输出**；失败应可通过 **Issues** 与日志定位（输出解析器见 [PRIORITY-PLAN.md](PRIORITY-PLAN.md) 中 P1 任务）。

### 2.3 与 Android 的差异

- Android 以 **Gradle** 为主；Harmony 以 **hvigor + ohpm** 为主。
- 插件侧统一抽象为：**可配置命令 + 工作目录 + 环境变量 + 退出码**。

---

## 3. 部署管线（hdc）

- **HarmonyDeployQtStep** 解析当前运行设备、HAP 产物路径，执行 **hdc install**。
- HAP 路径应优先来自构建约定，其次扫描产物目录；找不到产物时须明确报错（见实现与 [PRIORITY-PLAN.md](PRIORITY-PLAN.md)）。
- 安装失败信息应对齐用户可理解场景（证书、版本降级等），参考 Android `androiddeployqtstep` 的错误分类思路。

---

## 4. 运行

- **HarmonyRunConfiguration** 描述启动参数与环境。
- 实际执行由 **RunWorker** 完成（例如通过 `hdc shell` 调用 **aa start** 等）；`preStartShellCmd` / `postStartShellCmd` 等高级行为以 [PRIORITY-PLAN.md](PRIORITY-PLAN.md) **P0** 任务为准逐步对齐 Android 生命周期。

### 后续增强（计划）

- **调试**：对接 Qt Creator Debugger 与远程调试端（依赖上游）。  
- **日志**：`hdc hilog` 或等价方式接入应用输出（见 PRIORITY-PLAN **P2**）。

---

## 5. 测试建议

- **单元测试**：hdc 输出解析、路径拼接、命令行转义（mock 文本）。  
- **集成测试**：在 CI 固定 SDK 版本下做「干跑」或轻量命令校验。

---

## 6. 日志与诊断

- 使用统一日志分类（如 `qtc.harmony.*`），便于技术支持按模块过滤。  
- 可在设置或诊断动作中输出：hdc 版本、`list targets` 摘要、关键环境变量（注意脱敏）。
