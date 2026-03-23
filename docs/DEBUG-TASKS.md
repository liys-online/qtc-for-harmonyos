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
| **镜像** | **消费类真机**当前多为 **user、无 root** → 插件以 **§7.2（debug HAP + abstract）** 为默认自动编排。**§7.1（root + TCP + fport）** 仅适用于**可 root 的工程机 / 特殊镜像**；因官方零售设备**不开放 root**，**不在 Qt Creator 内实现 §7.1 全自动编排**（`harmonyhdcforward` 与文档仍供**手工**命令行或将来环境变化时使用）。 |
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

- [x] **2.1** 调研 hdc **`fport`** 语法并与 **§7.1** `lldb-server --listen "*:8080"` 对齐：**固定模板**与维护命令见 [HARMONY-LLDB-DEBUG.md](HARMONY-LLDB-DEBUG.md) §7.1 下「hdc fport 模板」。  
- [x] **2.2** 实现可复用封装：`harmonyhdcforward.*` — `hdcFportForwardTcp` / `hdcFportRemoveTcpForward` / `hdcFportList`（日志 `qtc.harmony.device`）。  
- [x] **2.3** **user + abstract socket** 与 fport 关系：**结论**已写入 [HARMONY-LLDB-DEBUG.md](HARMONY-LLDB-DEBUG.md) §7.1 末段（通常不依赖 fport；以实测为准）。  

---

## 5. 阶段 3 — Debugger 插件接入（MVP）

- [x] **3.1** `Harmony` 插件 **CMake**：`PLUGIN_DEPENDS` 已含 **QtCreator::Debugger**（`src/CMakeLists.txt`）。  
- [x] **3.2** **`harmonydebugsupport.cpp/.h`**：`HarmonyDebugWorkerFactory`，`DEBUG_RUN_MODE` + `HARMONY_RUNCONFIG_ID` + `STDPROCESS` 执行类型。  
- [x] **3.3** **`harmonyDebuggerRunParameters`**：`fromRunControl`、`setupPortsGatherer`、`setLldbPlatform("remote-ohos")`、`AttachToRemoteServer`、`setSkipDebugServer(true)`、`setUseExtendedRemote(true)`、清空 inferior（避免 hdc 被当作 symbol file）。**LLDB 可执行文件**沿用 **Kit 调试器**（或启动 Creator 时的环境变量 **`QTC_DEBUGGER_PATH`**）；插件内 **`HarmonyConfig::hostLldbExecutable()`** 仅作校验与**应用输出提示路径**（**不修改** Qt Creator 其它插件/核心源码）。  
- [x] **3.4** **`ohos.cpp`**：`setupHarmonyDebugWorker()` 与 `setupHarmonyRunWorker()` 并列。  
- [x] **3.5** **MVP**：未找到 host LLDB 时 **`runControl->errorTask(...)`** 明确报错；设备侧 **`HarmonyDevice`** 仍保留 **`freePorts` / `netstat` / `toolControlChannel`**（与其它通道或后续扩展兼容）。**默认 C++ 调试**走 **§7.2**，**不**依赖上述 TCP 通道与 **fport 自动编排**（零售机无 root，§7.1 全自动不在范围内）。  

---

## 6. 阶段 4 — 参数与运行链整合

- [x] **4.1** **solib-search-path**：`harmonydebugsupport.cpp` 在 C++ 调试时聚合 **CMake `buildDirectory` / `bc->buildDirectory()`**、**`harmonyBuildDirectory`**、**`entry/libs/<preferredAbi>`**、**`entry/build/default/intermediates/libs/default/<preferredAbi>`**（hvigor 打 HAP 前 native 布局）、**活动运行配置 workingDirectory**、**`QtVersion::qtSoPaths()`**（与 Android 思路一致；尚未从 ProjectNode 读专用 so 路径键）。  
- [x] **4.2** **sysroot**：若 Kit `SysRoot` 为空或路径不可读，则按 **Kit `Harmony.NDK`** 尝试 **`$NDK/sysroot`**、**`$NDK/llvm/sysroot`**（与常见 OpenHarmony NDK 布局一致；若你方 SDK 不同，可再补候选路径）。  
- [x] **4.3** **inferior / attach（user + abstract，默认）**：`harmonydebugsupport.cpp` 在 **C++ 调试** 且未设置 **`HARMONY_DEBUG_LEGACY`** 时，按官方 **§7.2** 顺序：`mkdir` → `hdc file send` **lldb-server** → `chmod` → 推送 **debug HAP** → **`bm install -p <设备上 .hap 路径>`** → **`aa start -a/-b`** → **`aa attach -b`** → **`sh -c 'aa process … -D "<lldb-server> platform --listen unix-abstract:///lldb-server/platform.sock" &'`**；主机 LLDB **`unix-abstract-connect:///lldb-server/platform.sock`**（**不**走 TCP `setupPortsGatherer`）。**不设** root + TCP + **fport** 的自动编排（当前官方零售机无 root）。**`HARMONY_DEBUG_LEGACY=1`**：仅启用 **`setupPortsGatherer`** 分配本机调试端口，**不会**自动推送 `lldb-server` 或执行 `fport`；供 **root 工程机**上**已手动**按 §7.1 起服后实验，或排障对比，**零售机请勿依赖**。  
- [x] **4.4** **PID 获取**：`harmonyutils::harmonyQueryApplicationPid` — 依次 **`bm dump -n`**、**`aa dump -b`**、**`ps -A` / `ps -ef`** 解析；结果写入 **`RunControl::setAttachPid`** → LLDB **remote attach**。若你机输出格式不同，可把样例贴 issue 以改进解析。  
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
| 2.x fport | `harmonyhdcforward.*`（`hdcFportForwardTcp` 等） |
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
| 2026-03-20 | **2.1–2.3**：`harmonyhdcforward.*` + `HARMONY-LLDB-DEBUG.md` fport 模板与 §7.2 结论。 |
| 2026-03-20 | **3.1–3.5 MVP**：`harmonydebugsupport.*`、`HarmonyDevice` 调试端口/通道；依赖 **Debugger** 插件；LLDB 路径由 **Kit / QTC_DEBUGGER_PATH** 配置。 |
| 2026-03-20 | **4.1 / 4.2**：`harmonydebugsupport` 设置 **solib search path** 与 **NDK sysroot 回退**；`harmonyutils` 新增 **`buildKeyForCMakeExtraData`**（部署与后续调试共用合成 build key）。 |
| 2026-03-20 | **4.3 / 4.4** + **范围**：默认 **§7.2**；**`harmonyQueryApplicationPid`**、bundle/ability、**`HARMONY_DEBUG_LEGACY`**。零售机无 root → **不做 §7.1 + fport 全自动编排**（§7.1 / `harmonyhdcforward` 以手工与文档为主）。 |
