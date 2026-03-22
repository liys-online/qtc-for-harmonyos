# Harmony 插件 — Native（C++/Qt）调试功能任务清单

> **对应计划**：[PRIORITY-PLAN.md](PRIORITY-PLAN.md) **P2-01**、**P2-07**（`hdc fport`）。  
> **操作与协议依据**：[HARMONY-LLDB-DEBUG.md](HARMONY-LLDB-DEBUG.md)（华为官方 LLDB 指南整理稿）。  
> **代码对标**：`src/plugins/android/androiddebugsupport.cpp`（`DebuggerRunParameters`、`debuggerRecipe`、`RunWorkerFactory`、`DEBUG_RUN_MODE`）。

---

## 0. 目标与边界

| 项 | 约定 |
|----|------|
| **语言** | 仅 **C/C++（含 Qt `.so`）**；不含 ArkTS / QML 调试（见 P2-02）。 |
| **设备** | **真机（aarch64 为主）** 与 **模拟器（x86_64 lldb-server）** 分阶段验证。 |
| **镜像** | **Phase A** 优先 **root + TCP** 跑通；**Phase B** 再攻 **user + HAP + abstract socket**（与官方 §7.2 一致）。 |
| **lldb-server** | 仅使用 **SDK 内华为签名** 二进制，路径见 [HARMONY-LLDB-DEBUG.md §3](HARMONY-LLDB-DEBUG.md)。 |

---

## 1. 阶段划分总览

| 阶段 | 名称 | 产出 |
|------|------|------|
| **0** | 前置与探测 | SDK 路径 API、设备矩阵结论、风险登记 |
| **1** | 命令行闭环 | 不依赖 Creator 的 root / user 调试「最小可复制」步骤 |
| **2** | hdc 与端口 | `fport` 封装、与 LLDB `connect` 串对齐（P2-07） |
| **3** | Debugger 接入 | `HarmonyDebugWorkerFactory` + `PLUGIN_DEPENDS` + 最小 `DebuggerRunParameters` |
| **4** | 参数完备 | `solib` / sysroot / 包名&PID / `aa` 序列与 Run 链整合 |
| **5** | 体验与文档 | UI 提示、失败 FAQ、COMPARISON 11.1 更新 |

---

## 2. 阶段 0 — 前置与探测

- [x] **0.1** 在 `HarmonyConfig` 中解析主机 `lldb`、各架构 `lldb-server` / 静态化 `lldb` 的 `FilePath`（**依赖已配置的 DevEco 路径** 下 `sdk/default/...`，见 `HarmonyConfig::devecoBundledSdkDefaultRoot()`、`hostLldbExecutable()`、`lldbServerExecutable()`、`staticLldbExecutable()`、`ohosLldbTripleForAbi()`；常量 `OHOS_LLDB_TRIPLE_*` 见 `ohosconstants.h`）。**可选后续**：在仅安装 OpenHarmony SDK（无 DevEco）时增加 `OHOS_SDK_HOME` 等同布局回退。  
- [x] **0.2** 定义**设备探测**辅助：`hdc shell id`、`getenforce` 结果枚举（root/user、SELinux），供后续选择 **§7.1 vs §7.2** 配方。实现：`harmonyutils.h` — `probeNativeDebugShellEnvironment()`、`nativeDebugRecipeKind()`。  
- [x] **0.3** 与 **Qt for OH** 约定：**`-g`/调试信息、strip、debug HAP** 的推荐构建方式已写入 [OPERATIONS.md](OPERATIONS.md) **§2.4**。  
- [x] **0.4** 登记**风险**：user 镜像下 `aa attach` / 签名校验失败等降级策略已写入 [OPERATIONS.md](OPERATIONS.md) **§2.5**。

---

## 3. 阶段 1 — 命令行闭环（验收门槛）

> 本阶段**不写** Qt Creator 插件逻辑，仅保证团队有人能**按官方步骤**复现。

**真机 / root + TCP（官方 §7.1）**

- [ ] **1.1** `setenforce 0`（若策略允许）后，完成 `lldb-server` + `a.out` 推送、`chmod`、监听 `*:port`。  
- [ ] **1.2** 主机 `lldb` → `platform select remote-ohos` → `platform connect connect://localhost:PORT`；必要时补 **`hdc fport`** 使 `PORT` 可达。  
- [ ] **1.3** `target create`、`b main`、`run` 或 attach 流程跑通，**断点命中**。  

**模拟器**

- [ ] **1.4** 使用 **`x86_64-linux-ohos/lldb-server`** 重复 **1.1–1.3**（路径见 [HARMONY-LLDB-DEBUG.md §3.3](HARMONY-LLDB-DEBUG.md)）。  

**user + HAP（官方 §7.2，可选后置）**

- [ ] **1.5** 使用 DevEco 生成的 **debug HAP**，完成 `bm install`、`aa start`、`aa attach`、`aa process -D ... platform.sock` 全链。  
- [ ] **1.6** 主机 `unix-abstract-connect:///lldb-server/platform.sock` + `settings append target.exec-search-paths` + 源码断点 + `attach <pid>` 命中 **native** 断点。  

**产出**：将**实际使用**的端口、`fport` 命令行、`lldb` 版本与 **API Level** 记入团队 Wiki 或本文 **§8 附录**。

---

## 4. 阶段 2 — hdc 与端口（P2-07）

- [ ] **2.1** 调研当前 hdc **`fport`** 语法（本机 ↔ 设备），与 **§7.1** 中 `lldb-server --listen "*:8080"` 对齐，形成**固定映射模板**。  
- [ ] **2.2** 实现**可复用**工具函数（例如 `HarmonyUtils` 或 `HarmonyHdcPortForward`）：`startForward(localPort, remotePort)` / `stopForward`（含异常与日志 `qtc.harmony.*`）。  
- [ ] **2.3** 定义 **user + abstract socket** 场景下是否仍需 fport，或 hdc 已透明转发 —— **以 1.5–1.6 实测为准** 写结论到 [HARMONY-LLDB-DEBUG.md](HARMONY-LLDB-DEBUG.md) 修订说明。  

---

## 5. 阶段 3 — Debugger 插件接入（MVP）

- [ ] **3.1** `Harmony` 插件 **CMake**：`PLUGIN_DEPENDS` 增加 **QtCreator::Debugger**（及 Android 插件已用的同级依赖，按 IDE 版本对齐）。  
- [ ] **3.2** 新增 **`harmonydebugsupport.cpp/.h`**（命名可对齐 `androiddebugsupport`）：实现 **`HarmonyDebugWorkerFactory`**，`addSupportedRunMode(DEBUG_RUN_MODE)`，`addSupportedRunConfig(HARMONY_RUNCONFIG_ID)`。  
- [ ] **3.3** 在 **`debuggerRunParameters(RunControl *)`** 中设置最小集：  
  - `DebuggerRunParameters::fromRunControl(runControl)`  
  - `setLldbPlatform("remote-ohos")`（与官方一致）  
  - `setStartMode(AttachToRemoteServer)` 或官方推荐的等价模式  
  - `setupPortsGatherer(runControl)`（若需本机端口）  
  - 调试器可执行文件：`rp.setLldbCommand` / 等价 API（以 Qt Creator 19+ API 为准）指向 **0.1** 解析的 `lldb`  
- [ ] **3.4** 在 **`ohos.cpp`**（或现有 `setupHarmony*` 聚合处）调用 **`setupHarmonyDebugWorker()`**，与 `setupHarmonyRunWorker()` 并列注册。  
- [ ] **3.5** **MVP 验收**：**Debug** 运行配置能**启动调试器进程**；若参数不全，至少**明确报错**（输出面板 / Issues），不静默失败。  

---

## 6. 阶段 4 — 参数与运行链整合

- [ ] **4.1** **solib-search-path**：对标 Android `getSoLibSearchPath`，聚合 **CMake 构建目录**、`harmonyBuildDirectory` / `ohpro` 下 **`.so` 输出路径**、Qt 版本提供的 so 路径（若有 API）。  
- [ ] **4.2** **sysroot**：按 **OHOS NDK / Qt OH** 布局设置 `rp.setSysRoot`（需与工具链文档一致）。  
- [ ] **4.3** **inferior / attach**：从 **RunConfiguration** 或 **app.json5 / 包名** 解析 **bundle name**、**Ability**；编排 **`aa start` → `aa attach` → `aa process -D ... lldb-server ...`**（user 场景）或与 **root TCP** 二选一。  
- [ ] **4.4** **PID 获取**：调试启动后从 `hdc shell` 或 `aa` 相关查询拿到 **pid** 传给 `attach`（具体命令以实测为准）。  
- [ ] **4.5** 与 **`HarmonyProcessRunnerFactory`** 关系：**Debug** 模式下是否 **先部署再调试用 hdc**，避免与 **Run** 重复安装 —— 定一种策略并文档化。  
- [ ] **4.6** **环境变量**：如需向 lldb 子进程注入 `HDC_SERVER_PORT` 等，对标 Android `modifyDebuggerEnvironment`。  

---

## 7. 阶段 5 — 体验、测试与文档

- [ ] **5.1** 运行配置 UI：必要时增加**简短说明**或链接到 [HARMONY-LLDB-DEBUG.md](HARMONY-LLDB-DEBUG.md)（user/root、勿锁屏、开发者选项）。  
- [ ] **5.2** 将官方 [FAQ §8](HARMONY-LLDB-DEBUG.md) 中 **A packet error 8**、**Permission denied** 转为 **Issues / 弹窗** 可识别文案。  
- [ ] **5.3** 更新 [COMPARISON-PROGRESS.md](COMPARISON-PROGRESS.md) **§11.1**（`HarmonyDebugWorkerFactory` 状态与备注）。  
- [ ] **5.4** 更新 [PRIORITY-PLAN.md](PRIORITY-PLAN.md) **P2-01** 状态；**P2-07** 在 fport 合入后勾连说明。  
- [ ] **5.5**（可选）**WITH_TESTS**：mock `hdc` 文本输出，测 **PID 解析 / fport 命令拼装**（不设网络 CI 硬依赖）。  

---

## 8. 附录 — 任务与代码映射（预填）

| 任务块 | 预期新增/修改文件（建议） |
|--------|---------------------------|
| 0.1 路径 | `harmonyconfigurations.*` 或 `harmonydebugpaths.*` |
| 0.2 设备壳探测 | `harmonyutils.*`（`HarmonyNativeDebugShellProbe`） |
| 0.3 / 0.4 构建与风险 | [OPERATIONS.md](OPERATIONS.md) §2.4、§2.5 |
| 2.x fport | `harmonyutils.*` 或 `harmonyhdcforward.*` |
| 3.x Factory | `harmonydebugsupport.cpp/.h`、`ohos.cpp`、`src/CMakeLists.txt` |
| 4.x 参数 | `harmonydebugsupport.cpp`、`harmonyrunconfiguration.*`、`harmonyutils.*` |

---

## 9. 修订记录

| 日期 | 说明 |
|------|------|
| 2026-03-20 | 初版：分阶段任务清单，对齐 P2-01 / P2-07 与 HARMONY-LLDB-DEBUG。 |
| 2026-03-20 | **0.1 代码落地**：`harmonyconfigurations.*` + `ohosconstants.h` 增加 LLDB 路径与 ABI→triple 映射。 |
| 2026-03-20 | **0.2**：`harmonyutils.*` 增加 `probeNativeDebugShellEnvironment` / `nativeDebugRecipeKind`（`id` + `getenforce`）。 |
| 2026-03-20 | **0.3 / 0.4**：`OPERATIONS.md` 增 §2.4（符号、strip、debug HAP）与 §2.5（user/签名风险与降级）。 |
