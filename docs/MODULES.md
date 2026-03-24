# Harmony 插件 — 源码模块说明

本文档说明 **`src/plugins/harmonyos/src/`** 下主要源文件的职责（路径省略时均相对该目录）。子库见 **`lib/`**。  
**`test/`**：在 **`WITH_TESTS`** 下构建 **`qt-oh-binary-catalog-generator`**（GitCode → `qt-oh-binary-catalog.v1.json`），见 [test/README.md](../test/README.md)。

---

## 1. 插件核心

| 单元 | 职责 |
|------|------|
| `ohos.cpp` | 插件入口：初始化、注册各工厂、Kit/工程信号 |
| `ohosconstants.h` | 插件 ID、设置键、ABI 名等常量 |
| `ohostr.h` | 翻译上下文 |
| `harmonyqtcdefs.h` | Qt Creator 版本宏（见 [VERSIONING.md](../VERSIONING.md)） |
| `harmonylogcategories.h` / `harmonylogcategories.cpp` | 统一 `QLoggingCategory`（`qtc.harmony.config` / `device` / `build` / `deploy` / `run` / `plugin` / `toolchain`）；见 [PRIORITY-PLAN.md](PRIORITY-PLAN.md) P1-01 |
| `harmonyrunner.cpp` / `harmonyrunner.h` | RunWorker 工厂注册（运行执行链） |

---

## 2. 配置与界面

| 单元 | 职责 |
|------|------|
| `harmonyconfigurations.*` | 全局配置读写、SDK/DevEco 路径、`persistSettings()`、`javaLocation()`/`nodeLocation()`、自动 Kit CMake（`OHOS_ARCH` 等）、与工具链协作；**ohpm**：`effectiveOhpmRegistryUrl()` / `ohpmStrictSsl()`（**P1-09**）；**P1-10** `ohModuleDeviceTypes()` / `setOhModuleDeviceTypes()`、`Harmony.ModuleDeviceTypes` 写入 Kit；**LLDB**：`devecoBundledSdkDefaultRoot()`、`hostLldbExecutable()`、`lldbServerExecutable()`/`staticLldbExecutable()`、`ohosLldbTripleForAbi()`（见 [DEBUG-TASKS.md](DEBUG-TASKS.md) 0.1） |
| `harmonysettingswidget.*` | 「选项」中的 Harmony 设置页；**ohpm** registry 行 + strict SSL 复选框；**P1-10** 默认 module device types **预设复选框**（`ohModuleDeviceTypePresetIds`）；**LLDB 诊断**（只读文本 + **Harmony settings** 折叠检查清单：`sdk/default`、Host LLDB、各架构 lldb-server、可选静态 lldb） |
| `harmonysdkdownloader.*` | OpenHarmony SDK **组件索引** HTTP 拉取与 JSON 解析（对标 Android `sdkmanager --list` 的数据层） |
| `harmonysdkmanagerdialog.*` | **SDK 包管理**对话框：刷新列表、多选下载、SHA-256 校验、日志（入口在设置页「Manage SDK Packages…」） |
| `harmonyqttreleasesdownloader.*` | **Qt for OpenHarmony** 发布列表：内置 GitCode/GitHub raw 双源拉取 [QT-OH-BINARY-CATALOG.md](QT-OH-BINARY-CATALOG.md) 清单（v1）；`QT_OH_BINARY_CATALOG_URL` 可覆盖主源 |
| `harmonyqttsdkmanagerdialog.*` | **Qt OH SDK 管理**对话框：按 Release/资源树选包、下载、可选 `tar` 解压（设置页「Manage Qt for OpenHarmony SDK…」） |
| `harmonysdkarchiveutils.*` | 系统 `tar` 解压（zip/tar.gz 等）；外层解压后自动继续解压**嵌套**包（解压根目录下及**一级子目录**内的压缩文件，按体积从大到小，解压到该文件所在目录后删除内层包，多轮直到无包或达上限）、探测 `isValidSdk`、查找 `bin/qmake` |

---

## 3. Qt 与工具链

| 单元 | 职责 |
|------|------|
| `harmonyqtversion.*` | Harmony Qt 版本检测、`targetAbi()`（qdevice.pri）、`addToBuildEnvironment` 注入 `JAVA_HOME`/PATH |
| `harmonytoolchain.*` | OHOS 交叉工具链检测与注册 |

---

## 4. 设备

| 单元 | 职责 |
|------|------|
| `harmonydevice.*` | `IDevice` 实现；设备 Widget 类可定义于同文件内 |
| `harmonydeviceinfo.*` | 设备信息数据结构 |
| `harmonydevicemanager.*` | hdc 枚举、USB 监听触发刷新 |
| `harmonyhdctargetsparser.*` | `hdc list targets` 单行解析与状态字符串映射（无 I/O）；设备管理器仅消费 `HdcListTargetsParseResult` |
| `harmonyhdctargetsparser_test.*` | **WITH_TESTS**：QTest mock 行（制表符 / 多空格 / 中英文状态），不调用 hdc |

---

## 5. 构建与部署

| 单元 | 职责 |
|------|------|
| `harmonybuildconfiguration.*` | CMake 构建配置扩展 |
| `harmonybuildhapstep.*` | HAP 构建步骤（sync / ohpm / assemble）、`prepareOhProDirectory`、`applyDevecoAndJavaEnv`、配置界面 `HarmonyBuildHapWidget`；**P1-10** `effectiveModuleDeviceTypes()`（步骤覆盖行 → Kit → `HarmonyConfig`）；UI：**跟随偏好/Kit** 与 **module device types 预设复选框** |
| `harmonydeployqtstep.*` | 部署步骤（hdc 安装等）；合成 build key `HARMONY_DEFAULT_RUN_BUILD_KEY` 时不要求 ProjectNode，并按需从 **applicationTargets** 解析 **OHOS_ARCH** |

---

## 6. 运行

| 单元 | 职责 |
|------|------|
| `harmonyrunconfiguration.*` | 运行配置与启动参数；若 CMake **applicationTargets** 为空（常见 Qt for OH **MODULE_LIBRARY**），仍提供默认项 **`Harmony.DefaultRunTarget`**（显示名经 `decoratedTargetName`） |
| `harmonydebugsupport.*` | **Debug 模式**：`HarmonyDebugWorkerFactory` → `debuggerRecipe`；LLDB `remote-ohos`；实际 **lldb 可执行文件** 来自 **Kit** 或 **`QTC_DEBUGGER_PATH`**；`HarmonyConfig::hostLldbExecutable()` 用于存在性检查与提示路径 |
| `harmonydevice.*` | 设备：`freePorts`、`portsGatheringRecipe`（本机 netstat）、`toolControlChannel`→localhost（debug 通道） |

---

## 7. 工具与辅助库

| 路径 | 职责 |
|------|------|
| `harmonyutils.*` | 包名、构建目录、hdc 参数；**P1-10** `ohModuleDeviceTypePresetIds`、`parseOhModuleDeviceTypesLine`、`joinOhModuleDeviceTypesLine`；**LLDB 前置** `probeNativeDebugShellEnvironment()`（`id` / `getenforce`）与 `nativeDebugRecipeKind()` |
| `harmonyhdcforward.*` | **hdc fport** TCP 转发：`hdcFportForwardTcp` / `hdcFportRemoveTcpForward` / `hdcFportList`（P2-07 / DEBUG-TASKS 2） |
| `lib/usbmonitor/` | USB 变化通知（平台相关）；日志分类 `qtc.harmony.device.usbmonitor` |
| `lib/ohprojectecreator/` | OpenHarmony 工程/模板生成辅助 |
| `lib/3rdparty/libusb/` | libusb 及平台配置 |
| `src/compat/` | 按 Qt Creator 大版本可选追加的源文件（见 [compat/README.md](../src/compat/README.md)） |

---

## 8. 构建系统

- **`src/CMakeLists.txt`**：目标 `Harmony`、插件元数据 `Harmony.json.in`、版本 CMake（`cmake/QtCreatorVersion.cmake` 等）。

---

## 9. 与 Android 插件目录习惯

Android 插件在 `android/` 下按文件大量拆分；Harmony 侧以 **`harmony*` 前缀模块** 对齐**能力名**（如 `*step`、`*configuration`），便于与 Android 能力做评审对照。详细映射见 [ANDROID-PARITY.md](ANDROID-PARITY.md)。
