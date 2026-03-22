# HarmonyOS — NDK Native LLDB 调试与性能分析（整理稿）

> **官方文档（权威）**：[指南-NDK 开发调试和性能分析-LLDB 高性能调试器](https://developer.huawei.com/consumer/cn/doc/harmonyos-guides/debug-lldb)（文中简称「官方 LLDB 指南」）。  
> **官方更新时间**（据页面）：2026-03-09 10:50。  
>
> **本文定位**：Harmony 插件团队内部 **P2-01（HarmonyDebugWorkerFactory）** 的操作参考与 **Qt Creator 对接提纲**。**分阶段任务清单**见 [DEBUG-TASKS.md](DEBUG-TASKS.md)。正文主体与官方指南一致；**若与官网冲突，以官网为准**。  
> **路径占位**：下文 `<DevEco_Studio_Home>`、`<clang distribution>` 等请替换为本机实际路径；Windows 示意为 `lldb.exe`，Linux / macOS 请使用同目录下 `lldb` 可执行文件。

---

## 1. 概述

**LLDB**（Low Level Debugger）为新一代高性能调试器，支持断点、变量查看与修改、内存操作、线程控制、表达式计算、堆栈回溯等，并支持跨平台与插件扩展。

- 当前 HarmonyOS 中的 LLDB 基于 **LLVM 15.0.4** 适配演进，为 **HUAWEI DevEco Studio** 工具链默认调试器，可调试 **C / C++** 应用。  
- 更多 LLDB 通用语法见 **LLDB 官方文档**（官方指南文中引用）。

---

## 2. 功能特点

| 特点 | 说明 |
|------|------|
| 调试能力 | 断点、变量、内存、线程、表达式、回溯等 |
| 跨平台 | Windows、Linux x86_64、**ohos**、Mac |
| 扩展性 | 支持插件扩展、可定制 |

---

## 3. 工具获取与目录结构

通过安装 **HUAWEI DevEco Studio** 及 SDK 组件可获取完整 LLDB 套件。以下为官方给出的 **Windows 路径结构示意**（其他 OS 目录层级相同，可执行文件名可能为 `lldb` 无 `.exe`）：

```text
<DevEco_Studio_Home>/
└── sdk/
    ├── default/
    │   ├── openharmony/
    │   │   └── native/
    │   │       └── llvm/
    │   │           ├── bin/
    │   │           │   └── lldb.exe                 # Windows 客户端主程序
    │   │           └── lib/
    │   │               └── clang/
    │   │                   └── current/
    │   │                       └── bin/
    │   │                           ├── aarch64-linux-ohos/
    │   │                           │   └── lldb      # ARM64 静态化 lldb
    │   │                           └── arm-linux-ohos/
    │   │                               └── lldb      # ARM32 静态化 lldb
    │   └── hms/
    │       └── native/
    │           └── lldb/
    │               ├── aarch64-linux-ohos/
    │               │   └── lldb-server              # ARM64 调试服务端（真机常用）
    │               ├── arm-linux-ohos/
    │               │   └── lldb-server              # ARM32 调试服务端
    │               └── x86_64-linux-ohos/
    │                   └── lldb-server              # x86_64 模拟器调试服务端
```

### 3.1 主机 LLDB 客户端（Windows 示例）

```text
<DevEco_Studio_Home>\sdk\default\openharmony\native\llvm\bin\lldb.exe
```

### 3.2 静态化 lldb（表 1 — 设备本地调试用）

| 路径（相对 DevEco / SDK） | 说明 |
|---------------------------|------|
| `...\openharmony\native\llvm\lib\clang\current\bin\aarch64-linux-ohos\lldb` | **aarch64-linux-ohos** |
| `...\openharmony\native\llvm\lib\clang\current\bin\arm-linux-ohos\lldb` | **arm-linux-ohos** |

### 3.3 lldb-server（表 2 — 远程 / 推送用）

| 路径（相对 DevEco / SDK） | 说明 |
|---------------------------|------|
| `...\hms\native\lldb\aarch64-linux-ohos\lldb-server` | **aarch64-linux-ohos** |
| `...\hms\native\lldb\arm-linux-ohos\lldb-server` | **arm-linux-ohos** |
| `...\hms\native\lldb\x86_64-linux-ohos\lldb-server` | **x86_64-linux-ohos**（模拟器） |

### 3.4 签名校验（官方说明）

- **lldb-server** 运行时会做**数字签名校验**；仅 **华为官方签名** 的 `lldb-server` 可正常调试。  
- 表 2 中的 `lldb-server` 为经闭源签名处理的版本，用于保护鸿蒙调试权限、防止未授权调试。  
- **建议**：优先通过 **DevEco Studio** 调试自动部署 `lldb-server`，避免手动推送带来的**版本不兼容**与**权限**问题。

---

## 4. LLDB 常用命令速查（官方摘录）

> Windows / Linux x86_64 / Mac 上 LLDB 略有差异，以实际为准。更多见官方「LLDB 工具使用指导」与 LLDB 手册。

**记录日志**

```text
(lldb) log enable -F -T -p -f d:\lldb.log lldb all
(lldb) log enable -F -T -p -f d:\lldbgdbremote.log gdb-remote all
(lldb) log enable -f /tmp/breakpoints.log lldb break
(lldb) log list
```

**断点**

```text
(lldb) breakpoint -f main.cpp -l 266
(lldb) breakpoint set -f main.cpp -l 20 -c '(x > 100)'
(lldb) breakpoint list
(lldb) breakpoint disable 1
```

**观察点**

```text
(lldb) watchpoint set variable global_var
(lldb) watchpoint set expression -w write -- 0x7ffeefbff5d8
(lldb) watchpoint list
```

**表达式 / 打印**

```text
(lldb) print int $value1 = 7
(lldb) expression int $value2 = 7
(lldb) print $value1
(lldb) expression $value2
(lldb) expression $value1 * 3
(lldb) p/x 12345
```

**帧 / 寄存器**

```text
(lldb) frame variable
(lldb) frame variable -g
(lldb) register read
```

**线程 / 单步**

```text
(lldb) thread backtrace all
(lldb) thread step-over
(lldb) thread step-out
```

**汇编**

```text
(lldb) disassemble --frame
(lldb) disassemble -name main
(lldb) nexti
(lldb) stepi
```

**信号**

```text
(lldb) process handle SIGSEGV -s true
(lldb) process handle
```

**附加进程**

```text
(lldb) process attach -p 1234
```

---

## 5. 环境准备：设备状态与调试范围（官方矩阵）

| 设备状态 | 调试支持范围 | lldb-server 部署路径 |
|----------|--------------|----------------------|
| **root 镜像 + SELinux 关闭** | 全类型 C/C++ 应用及二进制 | 任意可执行目录 |
| **root 镜像 + SELinux 开启** | 全类型 C/C++ 应用及二进制 | `/data/local/tmp/debugserver` |
| **user 镜像 + SELinux 开启** | **DevEco 编译签名的 debug 版 HAP** | **自动部署**（建议走 IDE） |

**判定（官方）**

- **root 镜像**：`hdc shell id` 为 `uid=0(root)`，或 shell 提示符为 `#`。  
- **user 镜像**：`hdc shell id` 为 `uid=2000(shell)`，或提示符为 `$`。  
- **SELinux Enforcing**：`hdc shell getenforce` → `Enforcing`。  
- **SELinux Permissive**：→ `Permissive`。

**远程调试（主要方式）**

- **一键调试**：使用 **DevEco Studio** 的 Debug（自动处理推送与连接）。  
- **手动远程调试**：设备上需 **lldb-server**，PC 上需与套件匹配的 **lldb**。

**本地调试（受限）**

- 设备需 **root**；将**对应架构**的静态化 `lldb` 或 `lldb-server` 用 `hdc file send` 推到设备（见官方「环境准备」）。

---

## 6. 使用指导 — 本地调试（设备上直接 lldb）

示例：在 HarmonyOS 上调试 **clang** 为 `aarch64-linux-ohos` 生成的带调试信息可执行文件 `a.out`。

**编译（主机交叉编译）**

```bash
<clang>/bin/clang++ --target=aarch64-linux-ohos --sysroot=<sysroot> -g hello.cpp -o a.out
```

**步骤概要**

1. `hdc shell` 进入设备。  
2. 进入设备上 lldb 所在目录，执行 `./lldb a.out`。  
3. `(lldb) b main` → `(lldb) run` → `(lldb) continue` 等。  
4. `(lldb) quit` 退出。

**调试已启动进程（attach）**

- 终端 1：`./a.out` 运行。  
- 终端 2：`./lldb` → `(lldb) process attach --name a.out`。  
- 设断点、在终端 1 触发输入后 `(lldb) continue`。  
- `(lldb) detach` → `quit`。

（**说明**：attach 与设置断点顺序可互换。）

---

## 7. 使用指导 — 远程调试

**要点**：远程调试 = PC 上 **lldb** + 设备上 **lldb-server** 配合；Windows / Linux x86_64 / Mac 步骤一致（路径与可执行名按平台替换）。

### 7.1 root 镜像远程调试（TCP + `remote-ohos`）

示例：`hello.cpp` 编译出 `a.out`（带 `-g`），架构 **aarch64-linux-ohos**。官方建议调试时 **关闭 SELinux**：`hdc shell setenforce 0`。

**窗口 1（推送与启动 server）**

```text
hdc file send <SDK>\...\hms\native\lldb\aarch64-linux-ohos\lldb-server /data/local/tmp/debugserver/lldb-server
hdc file send a.out /data/local/tmp/debugserver/a.out
hdc shell chmod 755 /data/local/tmp/debugserver/lldb-server /data/local/tmp/debugserver/a.out
hdc shell /data/local/tmp/debugserver/lldb-server p --server --listen "*:8080"
```

> **注**：官方原文若将两条 `hdc file send` 连成一行，应拆成上述两条；`8080` 需为当前未被占用的端口。

**窗口 2（主机 lldb）**

```text
./lldb
(lldb) platform select remote-ohos
(lldb) platform connect connect://localhost:8080
(lldb) target create /data/local/tmp/debugserver/a.out
(lldb) b main
(lldb) run
(lldb) source list
...
(lldb) quit
```

> **说明**：若 `connect://localhost:8080` 无法连通，需结合 **hdc 端口转发**（如 `hdc fport`）将本机端口映到设备监听端口，具体语法以当前 **hdc 文档**为准（见 [HDC 指南](https://developer.huawei.com/consumer/en/doc/harmonyos-guides-V13/hdc-V13) 等）。

### 7.2 user 镜像远程调试（HAP + abstract socket）

**条件（官方）**

- SELinux 一般 **不可关闭**；建议 **DevEco 构建的 debug HAP**。  
- `lldb-server` 推送到 **`/data/local/tmp/debugserver/...`**（可按包名分子目录）。  
- 调试期间设备 **勿锁屏**。  

**窗口 1（示例流程 — 包名 / Ability 请替换）**

```text
hdc shell mkdir -p /data/local/tmp/debugserver/com.example.myapplication
hdc file send <SDK>\...\lldb-server /data/local/tmp/debugserver/com.example.myapplication/lldb-server
hdc shell chmod 755 /data/local/tmp/debugserver/com.example.myapplication/lldb-server

hdc shell mkdir -p /data/local/tmp/<随机或官方示例目录名>
hdc file send <本机路径>\entry-default-signed.hap /data/local/tmp/<同上目录>/
hdc shell bm install -p /data/local/tmp/<同上目录>
hdc shell aa start -a EntryAbility -b com.example.myapplication
hdc shell aa attach -b com.example.myapplication
hdc shell aa process -a EntryAbility -b com.example.myapplication -D "/data/local/tmp/debugserver/com.example.myapplication/lldb-server platform --listen unix-abstract:///lldb-server/platform.sock"
```

> **注**：官方示例中 `mkdir data/local/tmp/...` 缺省前导 `/`，在设备上通常应使用 **`/data/local/tmp/...`**；目录名、HAP 路径以你工程为准。

**窗口 2（主机 lldb）**

```text
./lldb
(lldb) platform select remote-ohos
(lldb) platform connect unix-abstract-connect:///lldb-server/platform.sock
(lldb) settings append target.exec-search-paths "<本机 CMake 输出目录>\...\arm64-v8a"
(lldb) breakpoint set --file "C:/.../napi_init.cpp" --line 6
(lldb) attach <pid>
```

在设备上操作应用触发断点后：`(lldb) continue`、`(lldb) bt` 等；结束 `quit`。

---

## 8. FAQ（官方摘录）

- 执行 `run` 若出现 **`'A' packet returned an error: 8`** 等：多为**权限不足**。  
  1. 检查设备 **开发者选项** 等调试授权。  
  2. **user** 用户通常只能调试**应用**，不能随意调试任意可执行文件。  
- `lldb-server` 报 **Permission denied**：一般为无可执行权限，对推送文件 **`chmod 755`**。

---

## 9. 与 Qt Creator / Harmony 插件对接（P2-01 提纲）

Android 参考：`src/plugins/android/androiddebugsupport.cpp`（`DebuggerRunParameters`、`AttachToRemoteServer`、`setupPortsGatherer` 等）。

| 官方流程要素 | Qt Creator 集成时关注点 |
|--------------|---------------------------|
| **`platform select remote-ohos`** | 对应 Debugger 插件中 **LLDB platform / 远程连接** 配置（**非** `remote-android`） |
| **`connect://localhost:端口` 或 unix abstract** | **端口收集**、`hdc fport` 或与 hdc 通道的衔接；user 场景多为 **abstract socket** |
| **`target.exec-search-paths`、源码路径断点** | 映射 `DebuggerRunParameters` 的 **solib 搜索路径**、**源码映射**（Qt / CMake 输出目录与 `ohpro` 产物） |
| **`aa attach` / `aa process -D lldb-server ...`** | RunWorker 需在调试前编排 **hdc shell** 顺序（与 **Run** 步骤解耦或组合） |
| **签名的 lldb-server** | 集成时**勿**替换为未签名二进制；优先与 DevEco 同源 SDK 路径 |

**实现落点（计划）**

- 新增 **`HarmonyDebugWorkerFactory`**，依赖 **Debugger** 插件。  
- 区分 **root + TCP** 与 **user + HAP + abstract** 两条配方（或自动探测 `id` / `getenforce`）。  
- 失败信息输出到 **Issues / 应用输出**，便于对照官方 FAQ。

---

## 10. 验证清单

- [ ] **root + 远程**：按 §7.1 在命令行跑通 `remote-ohos` + `run`。  
- [ ] **user + HAP**：按 §7.2 或 DevEco 一键调试验证 native 断点。  
- [ ] **模拟器**：使用 **`x86_64-linux-ohos`** 的 `lldb-server` 重复验证。  
- [ ] 记录：**LLVM/LLDB 版本**、**API Level**、**hdc 版本**、**实际 connect 串**（可脱敏写入附录）。

---

## 11. 参考链接

| 说明 | URL |
|------|-----|
| **华为 — LLDB 高性能调试器（本文依据）** | <https://developer.huawei.com/consumer/cn/doc/harmonyos-guides/debug-lldb> |
| **华为 — HDC（示例入口）** | <https://developer.huawei.com/consumer/en/doc/harmonyos-guides-V13/hdc-V13> |
| **OpenHarmony — lldb-tool（英文）** | <https://gitee.com/openharmony/docs/blob/master/en/application-dev/tools/lldb-tool.md> |

---

## 12. 修订记录

| 日期 | 说明 |
|------|------|
| 2026-03-20 | 初版框架 + 官方入口。 |
| 2026-03-20 | **全文对齐**官方《LLDB 高性能调试器》结构与命令；修正合并的 `hdc file send`、设备路径前导 `/` 等笔误；增 §9 Qt Creator 映射。 |
