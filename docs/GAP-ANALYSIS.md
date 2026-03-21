# HarmonyOS 插件源码分析 — 与 Android 插件对标差距报告

> 基于 `src/plugins/android`（~16,500 行，33 个 .cpp + 35 个 .h）与 `src/plugins/harmonyos`（~6,900 行，16 个 .cpp + 16 个 .h + lib/）的逐文件比对。

---

## 0. 量化概览

| 维度 | Android | Harmony | 差距 |
|------|---------|---------|------|
| 核心源码行数 | ~16,500 | ~6,900 | 约 60% 缺口 |
| 扩展点覆盖 | 15 种 | 8 种 | 缺 RunWorker、Debug、QmlTooling、Editor、OutputParser、LanguageClient、SDK Manager |
| 单元/集成测试 | 4 组 | 0 | 无测试 |
| 设备类型 | 物理 + AVD | 仅物理（简化） | 缺模拟器管理 |
| 日志分类 | `QLoggingCategory` 分模块 | 混用 `qDebug`/`writeSilently` | 无分级 |
| 跨平台 | Win/Mac/Linux | 主要 Windows | Mac/Linux 可用但多处降级 |

---

## 1. 插件入口与生命周期

### Android (`androidplugin.cpp`, 166 行)
- `initialize()` 按依赖顺序注册 **18 个 setup 函数**
- `kitsRestored()` 中检查未配置状态并弹 InfoBar 引导
- 条件编译 `#ifdef WITH_TESTS` 注册 4 组测试

### Harmony (`ohos.cpp`, 137 行)
- `initialize()` 注册 **11 个 setup 函数**
- `kitsRestored()` 中 `registerQtVersions()` 被注释
- `extensionsInitialized()` 空实现
- `addBuildHapStepForOhBuild()` 用 `MessageManager::writeSilently` 做调试输出
- `loadTranslations()` 硬编码 `:/Harmony_zh_CN.qm`，泄漏 `QTranslator*`

**差距清单：**
| # | 问题 | 建议 |
|---|------|------|
| E1 | 缺少 `RunWorkerFactory` 注册（Run 按钮无效果） | 新增 `setupHarmonyRunWorker()` |
| E2 | 缺少 Debug 支持注册 | 新增 `setupHarmonyDebugWorker()`（P2） |
| E3 | 无 InfoBar 引导 | 仿 Android `kitsRestored` 检查 SDK 是否配置 |
| E4 | `registerQtVersions()` 被注释 | 恢复或移除死代码 |
| E5 | `QTranslator` 内存泄漏 | 设 parent 或成员变量 |
| E6 | 无测试注册 | 增加 `#ifdef WITH_TESTS` 骨架 |

---

## 2. 常量与 ID 体系

### Android (`androidconstants.h`, 87 行)
- 纯常量，简洁；ID 前缀 `Android.`；无冗余

### Harmony (`ohosconstants.h`, 1062 行)
- 前 50 行为核心常量
- 后 **~1000 行** 为 `Parameter` 命名空间，列举 HarmonyOS 系统能力字符串；**与插件功能无关**
- ID 前缀混用：`Qt4ProjectManager.Harmony*`、`Harmony.*`、`LL.Harmony`

**差距清单：**
| # | 问题 | 建议 |
|---|------|------|
| C1 | `Parameter` 命名空间（~800 行）未被引用 | 移到单独的 `harmonysyscaps.h` 或完全删除 |
| C2 | ID 前缀不统一：`Qt4ProjectManager` 是 Qt 4 时代遗留 | 全部改为 `Harmony.` 前缀（注意持久化迁移） |
| C3 | `HARMONY_SETTINGS_ID = "LL.Harmony Configurations"` | 改为 `"Harmony.Settings"` |

---

## 3. 配置与全局设置

### Android (`androidconfigurations.cpp`, 1565 行)
- `AndroidConfig` 命名空间函数 + `AndroidConfigurations` 单例
- 读写 `sdk_definitions.json`、校验 SDK/NDK 目录结构
- 自动创建 Toolchain、Debugger、Kit（`updateAutomaticKitList`）
- `QLoggingCategory` 分模块日志

### Harmony (`harmonyconfigurations.cpp`, 703 行；`.h`, 245 行)
- 单例 `m_instance` 裸指针
- `HarmonyConfig::HarmonyConfigData` 用 `QStringList` 存 SDK/qmake 路径
- `getDeviceProperty()` 直接 `qDebug()` 打 hdc shell 返回值
- `deaultDevice` 拼写错误（`d-e-a-u-l-t` → 应为 `default`）
- 头文件 `.h` 每个函数都写了中文 doxygen 注释，但注释仅复述函数名

**差距清单：**
| # | 问题 | 严重度 | 建议 |
|---|------|--------|------|
| CF1 | `deaultDevice` 拼写错误 | 低 | 全局 rename |
| CF2 | `getDeviceProperty` 用 `qDebug` | 中 | 改用 `qCDebug(harmonyConfigLog)` |
| CF3 | 注释复述函数名无增量信息 | 低 | 删除或改为说明行为/副作用 |
| CF4 | `sdkSettingsFileName()` 返回 `harmony.xml`，未验证存在性 | 中 | 加 fallback 或删除 |
| CF5 | `ndkLocation()` 字符串推断路径 | 中 | 用 `oh-uni-package.json` 验证 + 提示 |
| CF6 | 头文件用 `#ifndef` 守卫而非 `#pragma once` | 低 | 统一风格 |

---

## 4. 设置页 UI

### Android (`androidsettingswidget.cpp`, 770 行)
- SDK/NDK/JDK/OpenSSL 路径
- SDK 下载按钮 + `AndroidSdkDownloader`
- NDK 多版本列表 + 自动创建 Kit 开关
- **全平台** 一致

### Harmony (`harmonysettingswidget.cpp`, 489 行)
- Make / Deveco Studio / SDK list / qmake list
- Deveco Studio 配置 **仅 Windows** (`#ifdef Q_OS_WIN`)
- 错误提示引用 Android 概念（`source.properties`、`RELEASE.TXT`）
- `findGccToolChain()` 硬编码 `x86_64 Windows MSys PE`
- 下载链接硬编码（华为、Qt、MinGW）
- qmake 文件选择过滤器 `"qmake.exe"` 仅 Windows 有效

**差距清单：**
| # | 问题 | 严重度 | 建议 |
|---|------|--------|------|
| UI1 | Deveco Studio 配置在非 Windows 不可见 | 高 | macOS/Linux 也可安装 DevEco，去掉平台限制或按平台调整路径 |
| UI2 | 错误消息引用 `source.properties` / `RELEASE.TXT` | 中 | 改为 HarmonyOS SDK 真实文件名（`oh-uni-package.json` / `toolchains/`） |
| UI3 | `findGccToolChain()` 硬编码 Win x64 | 中 | 移除或按平台选择 |
| UI4 | qmake 过滤器 `"qmake.exe"` | 低 | 改为 `qmake` + 平台可执行后缀 |
| UI5 | 无 SDK 自动下载能力 | 中 | 后续可仿 `AndroidSdkDownloader` |
| UI6 | 无「自动创建 Kit」开关 | 低 | 增加用户控制 |

---

## 5. Qt 版本

### Android (`androidqtversion.cpp`, 427 行)
- `builtWith()` 解析已编译 ABI
- 多 ABI 支持、最小 NDK 版本检测
- `isQtAbisUpToDate()` 检查缓存
- 完善的单元测试

### Harmony (`harmonyqtversion.cpp`, 139 行)
- `supportOhVersion()` 依赖 `qconfig.h` 中 `OHOS_SDK_VERSION` 宏
- `targetAbi()` 依赖 `qdevice.pri` 中 `DEFAULT_OHOS_ARCH`
- `supportsMultipleQtAbis()` 硬编码 `5.15.12`

**差距清单：**
| # | 问题 | 建议 |
|---|------|------|
| QV1 | `supportsMultipleQtAbis()` 硬编码特定版本 | 改为基于版本范围判断 |
| QV2 | 缺少 `builtWith()` / ABI 自动解析 | 参照 Android 从 Qt 构建信息提取 |
| QV3 | 无版本有效性校验 | 增加 `isValid()` 检查 |

---

## 6. 工具链

### Android (`androidtoolchain.cpp`, 276 行)
- 继承 `GccToolchain`
- `autodetectToolchains()` 按 NDK 扫描 Clang
- ABI / mkspec 清晰映射

### Harmony (`harmonytoolchain.cpp`, 992 行 — 最长的文件)
- 继承 `Toolchain`（非 GccToolchain）
- `makeCommand()` 固定返回 `mingw32-make.exe`
- `syncAutodetectedWithParentToolchains` 仅处理 Windows + Clang + MinGW
- `clangTargets()` 仅 `x86_64-linux-ohos` 和 `aarch64-linux-ohos`，无 32 位
- `ndkLocation()` 通过字符串 split 推断路径
- 包含大量 `WarningFlagAdder` 映射代码

**差距清单：**
| # | 问题 | 严重度 | 建议 |
|---|------|--------|------|
| TC1 | `makeCommand()` 仅 Windows | 高 | macOS/Linux 用 `make` / `ninja` |
| TC2 | `syncAutodetectedWithParentToolchains` 仅 Win | 高 | 添加 macOS/Linux 逻辑 |
| TC3 | 无 32 位 target | 中 | 添加 `armv7-linux-ohos` 如有需要 |
| TC4 | `ndkLocation()` 字符串推断 | 中 | 用 SDK 配置验证 |
| TC5 | 992 行过长 | 低 | 拆分 `WarningFlagAdder` 到独立文件 |

---

## 7. 设备管理

### Android (`androiddevice.cpp`, 1329 行)
- `AndroidDevice` (`IDevice`) + `AndroidFileAccess` (`UnixDeviceFileAccess`)
- 完整的设备操作：文件访问、端口转发、ABI 判断
- AVD 管理：创建、启动、监控
- `setupDevicesWatcher()` 使用 `adb track-devices` 持续监听
- `AndroidDeviceFactory` 带完整图标

### Harmony
- `harmonydevice.cpp` (143 行) — 骨架
  - `HarmonyDeviceWidget::updateDeviceFromUi()` **空实现**
  - `updateDeviceState()` **空实现**
  - 使用 **Android 图标** `androiddevicesmall.png`
  - `canHandleDeployments()` 注释拼写 `possilbe`
- `harmonydevicemanager.cpp` (134 行)
  - `handleDevicesListChange` 解析 `hdc list targets -v` 输出，格式依赖
  - `UsbMonitor` 触发全量查询，VID/PID 与 hdc 序列号无对应
- `harmonydeviceinfo.h` (38 行)
  - `avdName` 字段（Android 概念）仍保留

**差距清单：**
| # | 问题 | 严重度 | 建议 |
|---|------|--------|------|
| D1 | 使用 Android 图标 | 中 | 创建 HarmonyOS 专属图标 |
| D2 | `updateDeviceFromUi()` / `updateDeviceState()` 空 | 高 | 实现设备状态刷新 |
| D3 | `avdName` 字段无意义 | 低 | 移除或改为模拟器标识 |
| D4 | 无 `DeviceFileAccess` | 中 | 实现 `hdc file send/recv` 或 `hdc shell` 文件访问 |
| D5 | 无端口转发 | 中 | 实现 `hdc fport` |
| D6 | hdc 输出解析脆弱 | 中 | 增加格式校验 + 容错 |
| D7 | USB 变化全量刷新 | 低 | 按 VID/PID 过滤 HarmonyOS 设备 |
| D8 | 拼写 `possilbe` | 低 | 修正 |

---

## 8. 构建步骤

### Android (`androidbuildapkstep.cpp`, 1314 行)
- 调用 `androiddeployqt --gradle` 构建 APK/AAB
- Keystore 签名完整流程（路径、密码、证书列表）
- Build Tools 版本选择
- `CertificatesModel` + `LibraryListModel`
- `QLoggingCategory(buildapkstepLog)`

### Harmony (`harmonybuildhapstep.cpp`, 587 行)
- `runRecipe()` 三步：`syncProjectTask` → `ohpmInstallTask` → `defaultProcessTask`
- 签名相关代码 **全部注释**（~70 行注释块）
- 大量 `MessageManager::writeSilently` 调试输出
- `"\033[0m"` 追加到每行输出（ANSI 转义码在非 TTY 无意义）
- ohpm 源硬编码 `https://ohpm.openharmony.cn/ohpm`
- `proInfo.deviceTypes = {"2in1"}` 硬编码
- `init()` 中 `createTemplates` 信号与 `build-profile.json5` 检查用三元运算符做流程控制，可读性差
- 命名空间结尾 `namespace Harmony::Internal` 与文件其他部分 `Ohos::Internal` 不一致

**差距清单：**
| # | 问题 | 严重度 | 建议 |
|---|------|--------|------|
| B1 | 签名逻辑全部注释 | 高 | 恢复或重新实现，至少提供 DevEco 联动 |
| B2 | `"\033[0m"` 追加输出 | 中 | 移除；Qt Creator 输出窗口不解析 ANSI |
| B3 | ohpm 源硬编码 | 中 | 移到设置页或用环境变量 |
| B4 | `deviceTypes = {"2in1"}` 硬编码 | 中 | 从 Kit/设备/设置推导 |
| B5 | 三元运算符做副作用逻辑 | 中 | 改为 `if-else` |
| B6 | 命名空间结尾不匹配 | 低 | 统一为 `Ohos::Internal` |
| B7 | 无 `QLoggingCategory` | 中 | 添加 `buildHapStepLog` |
| B8 | 3 个 task 有大量重复的 Process 配置代码 | 中 | 提取公共 helper |

---

## 9. 部署步骤

### Android (`androiddeployqtstep.cpp`, 600 行)
- 支持 `androiddeployqt` 全功能部署 和 `adb install` 简化路径
- 错误分类：证书不一致、版本降级等
- 卸载再安装流程
- `QLoggingCategory(deployStepLog)`

### Harmony (`harmonydeployqtstep.cpp`, 550 行)
- 已有错误分类和卸载流程（**质量较好**，与 Android 接近）
- HAP 路径硬编码：`entry/build/default/outputs/default/entry-default-signed.hap`
- `harmonydeployqt` 相关代码全部注释
- `QMessageBox::critical(nullptr, ...)` 无父窗口
- "Install a HAP File" 按钮中多个 early return **无错误提示**

**差距清单：**
| # | 问题 | 严重度 | 建议 |
|---|------|--------|------|
| DP1 | HAP 路径硬编码 | 高 | 从构建产物/配置推导 |
| DP2 | early return 无提示 | 中 | 加 `reportWarningOrError` |
| DP3 | `QMessageBox::critical(nullptr,...)` | 低 | 改为 `Core::ICore::dialogParent()` |
| DP4 | 注释的 `harmonydeployqt` 代码 | 低 | 评估是否需要，否则清理 |

---

## 10. 运行与调试 (**最大缺口**)

### Android
- `androidrunconfiguration.cpp` (110 行) — RunConfiguration + Aspects
- `androidrunner.cpp` (154 行) — `RunWorkerFactory` 注册
- `androidrunnerworker.cpp` (859 行) — adb 启动、lldb-server、logcat、端口转发
- `androiddebugsupport.cpp` (190 行) — LLDB 远程调试
- `androidqmltoolingsupport.cpp` (40 行) — QML Profiler

### Harmony
- `harmonyrunconfiguration.cpp` (94 行) — RunConfiguration + Aspects
- **无 `RunWorkerFactory`** — Run 按钮完全不工作
- **无 Runner 实现** — 缺少对应 `androidrunnerworker`
- **无 Debug 支持** — 缺少对应 `androiddebugsupport`
- 声明的 `amStartArgs` / `preStartShellCmd` / `postStartShellCmd` **从未被使用**

**差距清单：**
| # | 问题 | 严重度 | 建议 |
|---|------|--------|------|
| R1 | **Run 完全不工作** | **致命** | 实现 `HarmonyRunWorkerFactory` + `harmonyrunnerworker` |
| R2 | 无 Debug 支持 | 高 | 实现 `HarmonyDebugWorkerFactory`（P2） |
| R3 | 无 QML Profiler 支持 | 低 | 后续扩展 |
| R4 | RunConfiguration Aspects 声明但未消费 | 中 | Runner 实现中使用 |

---

## 11. SDK 管理（Android 独有能力）

### Android
- `androidsdkmanager.cpp` (639 行) — 包管理
- `androidsdkpackage.h` (210 行) — 包抽象
- `sdkmanageroutputparser.cpp` (394 行) — 解析 sdkmanager 输出
- `androidsdkmanagerdialog.cpp` (633 行) — 对话框
- `androidsdkdownloader.cpp` (204 行) — 下载 SDK Tools
- 合计 **~2,080 行**

### Harmony
- **完全没有**

**差距清单：**
| # | 问题 | 建议 |
|---|------|------|
| SDK1 | 无 SDK 包管理 | P2 — 后续可对接 DevEco 命令行或手动安装 |
| SDK2 | 无自动下载 | P2 |

---

## 12. 编辑器与语言支持（Android 独有能力）

### Android
- `androidmanifesteditor.cpp` (619 行) — Manifest 编辑器 + hover
- `javaeditor.cpp` (115 行) — Java 编辑
- `javalanguageserver.cpp` (320 行) — JDT LS 集成
- `javaparser.cpp` (70 行) — Gradle 输出解析
- `splashscreencontainerwidget.cpp` (893 行) — 启动屏编辑
- 合计 **~2,017 行**

### Harmony
- **完全没有**

**差距清单：**
| # | 问题 | 建议 |
|---|------|------|
| ED1 | 无 `app.json5` / `module.json5` 编辑器 | P2 |
| ED2 | 无 ArkTS 语言支持 | P2（可对接外部 LS） |
| ED3 | 无构建输出解析器（OutputTaskParser） | P1 — 解析 hvigor/ohpm 错误 |

---

## 13. 向导与模板

### Android
- `manifestwizard.cpp` (318 行) — 创建 Manifest + Gradle 模板
- `avdcreatordialog.cpp` (449 行) — AVD 创建向导

### Harmony
- `lib/ohprojectecreator/` (558 行) — 创建 ohpro 工程
  - `sdkVersionMap` 硬编码 API 12–20
  - `"Hamony"` 拼写错误
  - 可能包含开发路径（`C:/Users/liys/...`）

**差距清单：**
| # | 问题 | 严重度 | 建议 |
|---|------|--------|------|
| W1 | 无「新建 HarmonyOS 项目」向导 | 中 | P1 后续 |
| W2 | `sdkVersionMap` 硬编码 | 中 | 从 SDK 目录枚举 |
| W3 | `"Hamony"` 拼写 | 低 | 修正 |
| W4 | 检查模板中是否有个人路径 | 高 | 清理，改为相对路径/占位符 |

---

## 14. 代码质量综合问题

### 14.1 命名空间不一致
- 文件大部分用 `Ohos::Internal`
- `harmonybuildhapstep.cpp` 结尾写 `namespace Harmony::Internal`
- 应全部统一

### 14.2 调试输出混乱
| 方式 | 出现次数(估) | 问题 |
|------|-------------|------|
| `Core::MessageManager::writeSilently(...)` | ~15 处 | 调试信息进用户可见区域 |
| `qDebug(...)` | ~5 处 | 无分类、无开关 |
| `qCDebug(...)` | ~3 处 | 仅 deploy 步骤有 |

**建议**：全面改为 `QLoggingCategory`，按模块分类：
- `qtc.harmony.config`
- `qtc.harmony.device`
- `qtc.harmony.build`
- `qtc.harmony.deploy`
- `qtc.harmony.run`

### 14.3 错误处理不统一
- 构建步骤：`reportWarningOrError` + `TaskHub`（deploy 已有）
- 其他步骤：仅 `writeSilently` 或静默 return
- **应统一使用 `reportWarningOrError` + `TaskHub`**

### 14.4 硬编码汇总
| 位置 | 内容 | 建议 |
|------|------|------|
| `harmonydeployqtstep.cpp:220` | `entry/build/.../entry-default-signed.hap` | 从构建配置推导 |
| `harmonybuildhapstep.cpp:203` | `"/ohpro"` | 常量化 |
| `harmonybuildhapstep.cpp:363` | `https://ohpm.openharmony.cn/ohpm` | 可配置 |
| `harmonytoolchain.cpp:564` | `mingw32-make.exe` | 按平台 |
| `harmonysettingswidget.cpp:309,315` | 下载链接 | 常量化 |
| `harmonydevice.cpp:119` | Android 图标路径 | 换图标 |
| `harmonybuildhapstep.cpp:56` | `deviceTypes = {"2in1"}` | 可配置 |

### 14.5 内存与资源
- `ohos.cpp:119` `QTranslator *translator = new QTranslator()` 无 parent → 泄漏
- `ohprojectecreator` 单例 `m_p` 与 `m_instance` 双指针，析构路径不清晰

### 14.6 拼写错误
| 位置 | 错误 | 正确 |
|------|------|------|
| `harmonyconfigurations.cpp:365` | `deaultDevice` | `defaultDevice` |
| `harmonydevice.cpp:93` | `possilbe` | `possible` |
| `ohprojectecreator.cpp` | `Hamony` | `Harmony` |
| `ohprojectecreator.h` | `ProjecteCreator` | `ProjectCreator`（或保持但统一） |

---

## 15. 与 Android 插件能力矩阵

| 能力 | Android | Harmony | 状态 |
|------|---------|---------|------|
| 插件注册 | ✓ | ✓ | 接近 |
| 设置页 | ✓ 全平台 | △ 偏 Windows | 需改进 |
| SDK/NDK 路径 | ✓ 完善 | △ 基础 | 需改进 |
| SDK 包管理 | ✓ | ✗ | 缺失 |
| SDK 下载 | ✓ | ✗ | 缺失 |
| Qt 版本 | ✓ 完善 | △ 基础 | 需改进 |
| 工具链 | ✓ 全平台 | △ 偏 Windows | 需改进 |
| 设备（物理） | ✓ 完善 | △ 骨架 | 需改进 |
| 设备（模拟器） | ✓ AVD | ✗ | 缺失 |
| CMake/构建配置 | ✓ | △ 骨架 | 需改进 |
| 构建步骤 | ✓ APK+签名 | △ HAP(无签名) | 需改进 |
| 部署步骤 | ✓ | △ 较好 | 小改 |
| **运行** | ✓ | **✗ 不工作** | **致命** |
| **调试** | ✓ LLDB+QML | **✗** | 缺失 |
| QML Profiler | ✓ | ✗ | 缺失 |
| Manifest 编辑 | ✓ | ✗ | 缺失 |
| Java/ArkTS 编辑 | ✓ | ✗ | 缺失 |
| 输出解析器 | ✓ JavaParser | ✗ | 缺失 |
| 向导 | ✓ | △ ohpro 创建 | 需改进 |
| 签名管理 | ✓ Keystore | ✗ (注释) | 缺失 |
| 日志分级 | ✓ | ✗ | 缺失 |
| 单元测试 | ✓ 4 组 | ✗ | 缺失 |
| 跨平台 | ✓ | △ | 需改进 |
