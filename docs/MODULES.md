# Harmony 插件 — 源码模块说明

本文档说明 **`src/plugins/harmonyos/src/`** 下主要源文件的职责（路径省略时均相对该目录）。子库见 **`lib/`**。

---

## 1. 插件核心

| 单元 | 职责 |
|------|------|
| `ohos.cpp` | 插件入口：初始化、注册各工厂、Kit/工程信号 |
| `ohosconstants.h` | 插件 ID、设置键、ABI 名等常量 |
| `ohostr.h` | 翻译上下文 |
| `harmonyqtcdefs.h` | Qt Creator 版本宏（见 [VERSIONING.md](../VERSIONING.md)） |
| `harmonyrunner.cpp` / `harmonyrunner.h` | RunWorker 工厂注册（运行执行链） |

---

## 2. 配置与界面

| 单元 | 职责 |
|------|------|
| `harmonyconfigurations.*` | 全局配置读写、SDK/DevEco 路径、与 Kit/工具链协作 |
| `harmonysettingswidget.*` | 「选项」中的 Harmony 设置页 |

---

## 3. Qt 与工具链

| 单元 | 职责 |
|------|------|
| `harmonyqtversion.*` | Harmony 相关 Qt 版本检测与注册 |
| `harmonytoolchain.*` | OHOS 交叉工具链检测与注册 |

---

## 4. 设备

| 单元 | 职责 |
|------|------|
| `harmonydevice.*` | `IDevice` 实现；设备 Widget 类可定义于同文件内 |
| `harmonydeviceinfo.*` | 设备信息数据结构 |
| `harmonydevicemanager.*` | hdc 枚举、USB 监听触发刷新 |

---

## 5. 构建与部署

| 单元 | 职责 |
|------|------|
| `harmonybuildconfiguration.*` | CMake 构建配置扩展 |
| `harmonybuildhapstep.*` | HAP 构建步骤及步骤内配置界面（如 `HarmonyBuildHapWidget`） |
| `harmonydeployqtstep.*` | 部署步骤（hdc 安装等） |

---

## 6. 运行

| 单元 | 职责 |
|------|------|
| `harmonyrunconfiguration.*` | 运行配置与启动参数 |

---

## 7. 工具与辅助库

| 路径 | 职责 |
|------|------|
| `harmonyutils.*` | 包名、构建目录、hdc 参数等共用函数 |
| `lib/usbmonitor/` | USB 变化通知（平台相关） |
| `lib/ohprojectecreator/` | OpenHarmony 工程/模板生成辅助 |
| `lib/3rdparty/libusb/` | libusb 及平台配置 |
| `src/compat/` | 按 Qt Creator 大版本可选追加的源文件（见 [compat/README.md](../src/compat/README.md)） |

---

## 8. 构建系统

- **`src/CMakeLists.txt`**：目标 `Harmony`、插件元数据 `Harmony.json.in`、版本 CMake（`cmake/QtCreatorVersion.cmake` 等）。

---

## 9. 与 Android 插件目录习惯

Android 插件在 `android/` 下按文件大量拆分；Harmony 侧以 **`harmony*` 前缀模块** 对齐**能力名**（如 `*step`、`*configuration`），便于与 Android 能力做评审对照。详细映射见 [ANDROID-PARITY.md](ANDROID-PARITY.md)。
