# Harmony 插件 — 构建、部署与运行（运维说明）

本文档描述插件依赖的**外部工具**及**主流程**，供集成、测试与技术支持查阅。路线图与任务拆解见 [PRIORITY-PLAN.md](PRIORITY-PLAN.md)。

---

## 1. 外部依赖

| 组件 | 用途 | 典型来源 |
|------|------|----------|
| Node.js | 执行 hvigor、ohpm 脚本 | DevEco：`tools/node/node` 或 `tools/node/bin/node`；否则系统 **PATH** 中的 `node` |
| JDK | hvigor / Gradle 子进程 | DevEco：`jbr/Contents/Home`（macOS）或 `jbr`（扁平布局）；否则 **JAVA_HOME**、macOS **`/usr/libexec/java_home`** 或 PATH 中的 `java`；Harmony Qt Kit 构建环境会**自动注入** `JAVA_HOME` |
| hvigorw（hvigorw.js） | 工程同步与 HAP 构建 | DevEco / SDK 工具链目录 |
| ohpm（pm-cli.js） | 依赖安装 | 同上 |
| hdc | 设备通信、安装 HAP | SDK `toolchains` 等路径下 |
| CMake + Ninja/Make | IDE 侧生成与 native 构建 | 由 Kit / Qt Creator 提供；Harmony 自动 Kit 可为 Unix Makefiles 预填 **`CMAKE_MAKE_PROGRAM`** |
| Qt for Harmony | 交叉 Qt 与工程描述 | Qt 发行渠道 |

**约束**：插件通过**设置页与环境变量**解析路径，不在代码中写死绝对路径。

**首次配置引导**（`kitsLoaded` 后 **InfoBar**，可打开 **Harmony 设置** `LL.Harmony Configurations`）：
- 已注册 **Harmony Qt**，但**无有效 SDK 路径**（默认 SDK、SDK 列表、`OHOS_SDK_HOME` / `OHOS_SDK_PATH` 根目录均无效）→ 提示配置 SDK。
- **已有有效 SDK**，但**未注册任何 Harmony Qt** → 提示在 Harmony 设置或 **Preferences > Kits > Qt Versions** 添加 qmake。
- 两者皆无（尚未接触 Harmony 配置）→ 不打扰。

**hvigor 子进程环境（实现要点）**：`DEVECO_SDK_HOME`；有效 JDK 时 `JAVA_HOME` 与 `PATH` 含 `$JAVA_HOME/bin`；工作目录为构建目录下已创建的 **`ohpro`** 规范绝对路径，并设置 **`PWD`** / **`INIT_CWD`**（减轻 Node `uv_cwd` 类错误）。

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

每步启动前会 **确保构建目录下的 `ohpro` 子目录已创建** 并采用 **规范绝对路径** 作为 cwd；与主步骤一致注入 **DevEco / Java** 相关环境变量（见 §1）。

标准输出/错误回传至 Qt Creator **构建输出**；失败应可通过 **Issues** 与日志定位（**hvigor 日志解析进 Issues** 仍属 [PRIORITY-PLAN.md](PRIORITY-PLAN.md) **P1-14**）。

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

- **HarmonyRunConfiguration** 描述启动参数与环境；可选 **Bundle / Ability 名称覆盖**（留空则分别用 `AppScope/app.json5` 与 `ohpro` 下 `module.json5` 启发式解析，否则回退 `EntryAbility`）。
- 实际执行由 **RunWorker** 完成（例如通过 `hdc shell` 调用 **aa start** 等）；`preStartShellCmd` / `postStartShellCmd` 等高级行为以 [PRIORITY-PLAN.md](PRIORITY-PLAN.md) **P0** 任务为准逐步对齐 Android 生命周期。
- **`aa start` 会立刻返回**：若不在设备 shell 里保持会话，本机 `hdc` 会马上退出，Qt Creator 会认为运行已结束（**停止按钮不可用**）。实现上在启动命令后追加设备侧 `while/sleep` 循环以保持会话，直到用户点「停止」（与 Android 用 PID 轮询保持 `adb` 会话的思路一致）。
- **Post-quit on-device shell commands**：在 **本机 `hdc` 进程结束** 后（含用户点停止、会话被 kill），按行执行运行配置里的命令（每条一次 `hdc shell …`，与 Android 多条 `adb shell` 类似）。

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
