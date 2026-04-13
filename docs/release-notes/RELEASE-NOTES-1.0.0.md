# Release Notes — Harmony 插件 **1.0.0**

> **首个正式版**：主链路（插件安装 → 配置 → 构建 HAP → 部署 → 运行 → Native LLDB 调试）已完成，行为与配置键稳定。  

---

## 适用环境

| 项目 | 要求 |
|------|------|
| **Qt Creator** | **19.0.0**（与插件构建所用主版本须一致）；详见 [VERSIONING.md](../VERSIONING.md) |

**本包构建信息**：

| 字段 | 值 |
|------|----|
| Qt Creator 版本 | 19.0.0 |
| 构建主机 OS / 架构 | Windows x64 / macOS x64 |
| `CMAKE_BUILD_TYPE` | RelWithDebInfo |

---

## 发布产物

每个平台提供独立压缩包，包含以下文件：

### Windows（`qtc4harmony-1.0.0-QtCreator-19.0.0-windows`）

| 文件 | 说明 |
|------|------|
| `Harmony.dll` | 插件主体 |
| `Harmony.pdb` | 调试符号（可选，供崩溃分析） |
| `lldbbridge.py` | OHOS 定制 LLDB 桥接脚本，覆盖 Qt Creator 自带版本 |
| `CMakeLists.txt` | HarmonyOS 适配新建工程向导模板，覆盖内置模板 |
| `arktsdebugbridge.exe` | ArkTS 调试桥，startup-break 调试模式所需 |
| `Qt6WebSockets.dll` | `arktsdebugbridge.exe` 运行时依赖 |

### macOS（`qtc4harmony-1.0.0-QtCreator-19.0.0-macos`）

| 文件 | 说明 |
|------|------|
| `libHarmony.dylib` | 插件主体 |
| `lldbbridge.py` | OHOS 定制 LLDB 桥接脚本，覆盖 Qt Creator 自带版本 |
| `CMakeLists.txt` | HarmonyOS 适配新建工程向导模板，覆盖内置模板 |
| `arktsdebugbridge` | ArkTS 调试桥，startup-break 调试模式所需 |
| QtWebSockets.framework | `arktsdebugbridge` 运行时依赖 |

---

## 安装（文件拷贝）

将对应平台压缩包解压后，**按下表将文件拷贝到 Qt Creator 安装目录**（`<QtCreator>` 为 Qt Creator 根目录）：

### Windows

```
Harmony.dll              →  <QtCreator>\lib\qtcreator\plugins\
Harmony.pdb              →  <QtCreator>\lib\qtcreator\plugins\       （可选）
lldbbridge.py            →  <QtCreator>\share\qtcreator\debugger\     （覆盖）
CMakeLists.txt           →  <QtCreator>\share\qtcreator\templates\wizards\projects\qtwidgetsapplication\  （覆盖）
arktsdebugbridge.exe     →  <QtCreator>\libexec\qtcreator\
Qt6WebSockets.dll        →  <QtCreator>\libexec\qtcreator\
```

### macOS

```
libHarmony.dylib       →  <QtCreator>/Qt Creator.app/Contents/PlugIns/
lldbbridge.py          →  <QtCreator>/Qt Creator.app/Contents/Resources/debugger/      （覆盖）
CMakeLists.txt         →  <QtCreator>/Qt Creator.app/Contents/Resources/templates/wizards/projects/qtwidgetsapplication/  （覆盖）
arktsdebugbridge       →  <QtCreator>/Qt Creator.app/Contents/Resources/libexec/
QtWebSockets.framework →  <QtCreator>/Qt Creator.app/Contents/Resources/libexec/
```

安装完成后启动 Qt Creator，进入 **Help → About Plugins…** 搜索 **Harmony**，确认状态为 **Loaded**。  
详细步骤见 [USER-MANUAL.md](USER-MANUAL.md) §1。

---

## 1.0.0 正式版包含的能力

### 主链路（已完成）

- **插件安装**：文件拷贝方式，无需 `-pluginpath` 外载，支持 Windows / macOS 。
- **SDK 与工具链配置**：Harmony 设置页（DevEco 路径、SDK、qmake 列表、Kit 自动同步）；内置 OpenHarmony SDK 管理器与 Qt for OH SDK 管理器；首次配置 InfoBar 引导。
- **新建 Qt for HarmonyOS 工程**：覆盖 Qt Creator 内置 Qt Widgets Application 向导模板，新建工程自动生成 OHOS 平台分支（`add_library(SHARED)` + QPA 插件链接），开箱支持 HarmonyOS。
- **CMake 工程**：Harmony Kit 联动 Qt 版本与 OHOS 工具链；Build 页 **Harmony 摘要子页**（P1-13）。
- **HAP 构建**（`HarmonyBuildHapStep`）：hvigor sync → ohpm install → assembleHap；Node/Java 环境自动注入；ohpm registry 与 strict SSL 可配置（P1-09）；`deviceTypes` 多选可配置（P1-10）；Entry library name 可覆盖（含 Apply 快速写入 `EntryAbility.ets`）；hvigor/ohpm 错误行进入 Issues 并支持源码跳转（P1-14）。
- **hdc 部署**（`HarmonyDeployQtStep`）：自动定位 HAP（`build-profile.json5` → 经典路径 → 树内最新）；部署失败证书/版本冲突弹框引导处理。
- **运行**（`HarmonyRunConfiguration` + `HarmonyMainRunSocketTask`）：`aa start` + 保活；Bundle/Ability 可覆盖；pre-launch / post-quit 设备命令；hilog 流式输出到 Application Output 面板，自动 PID 过滤，按级别着色；hilog 关键字过滤；hdc daemon Socket 优先（CLI 可回退，`QTC_HARMONY_HDC_USE_CLI` 强制切换）。
- **Native LLDB 调试**（`HarmonyDebugWorkerFactory`，P2-01）：`lldbbridge.py` 插件自有（含 `remote-ohos` attach 分支）；`arktsdebugbridge` 信号文件机制解除 ArkTS VM 阻塞；solib 路径 hvigor cmake 目录优先修正行号偏移；**断点命中与变量查看已验证**；startup-break（`aa start -D`）可关闭。

### 其他已完成项

- 全模块 `QLoggingCategory` 日志分类（P1-01）。
- hdc 输出解析健壮性（`harmonyhdctargetsparser`，含单测，P1-08）。
- 配置 Store 键统一（`Harmony.*` 前缀，兼容旧键自动迁移，P1-12）。
- hdc daemon Socket 客户端（`HdcSocketClient`）：短连接同步 shell、hilog 流式、install/uninstall Socket 优先（P2-15 阶段 1–4）。
- USB 热插拔自动检测（`usbmonitor`）。

---

## 当前阶段未包含的能力

以下项目在 1.0.0 范围内**不完整**：

| 能力 | 状态 | 说明 |
|------|------|------|
| 模拟器枚举与启动 | ⬜ 未开始 | 无统一 CLI，依赖 DevEco 独占（P2-05） |
| 完整签名向导 | ⬜ 未开始 | 签名工具仅在 DevEco 内；文档引导至 DevEco Auto Sign（P2-08） |

---

## 已知限制与注意事项

- **签名**：真机部署必须先在 DevEco Studio 完成 Auto Sign，插件不内置签名向导。构建后 HAP 由 hvigor 使用已配置证书自动签名，无需重复操作。
- **Debug HAP**：Native LLDB 调试需 debug 构建变体（含 DWARF，`.so` 未 strip）；release HAP 的 `.so` 通常已 strip，LLDB 无法解析符号。
- **`arktsdebugbridge` 依赖**：`Qt6WebSockets` 动态库已随插件包一同提供，需与 `arktsdebugbridge` 放在同一目录。若 `arktsdebugbridge` 缺失或 WebSockets 初始化失败，startup-break 调试模式下会在 Application Output 显示警告。
- **主版本绑定**：插件二进制**必须与宿主 Qt Creator 主版本匹配**，不支持跨主版本加载（见 [VERSIONING.md](../VERSIONING.md)）。
- 本插件为**社区开源项目**，**非** Qt Company 官方产品（见 [AUTHORS.md](AUTHORS.md)）。

---

## 升级说明

1. **文件拷贝替换**：将旧 `Harmony.dll`（或 `.dylib`/`.so`）替换为新版，同时替换 `arktsdebugbridge` 与 `Qt6WebSockets` 动态库。
2. **lldbbridge.py 替换**：alpha 版需手动部署 lldbbridge.py；1.0.0 已随包提供，直接覆盖 Qt Creator debugger 目录。
3. **Store 键自动迁移**：工程 `.user` 文件内旧格式键（如 `BuildTargetSdk`、`UninstallPreviousPackage`、`Harmony.AaStartArgsKey` 等）会在**首次保存工程**时自动迁移为规范 `Harmony.*` 键，无需手工修改。
4. **新建工程模板**：覆盖 `qtwidgetsapplication/CMakeLists.txt` 后，新建工程即可获得 OHOS 支持，已有工程无需改动。

---

## 校验和

| 产物                                           | sha256                                                       |
| ---------------------------------------------- | ------------------------------------------------------------ |
| qtc4harmony-1.0.0-QtCreator-19.0.0-windows.zip | 172d17b59419255dec8fe891c881360036b015db6fa6b9eea61205e400abf6d1 |
| qtc4harmony-1.0.0-QtCreator-19.0.0-macos.zip   | 3fc24fe022c338ee59cc7fb8a38a9d6a2f5e080809aff42e57a07df0cde6f779 |

---

## 标签与版本说明

```bash
v1.0.0
```

`Harmony.json` 中 `Version` = `1.0.0`，`CompatVersion` = `1.0.0`（与 Qt Creator `PluginSpec::isValidVersion` 规范一致）。
