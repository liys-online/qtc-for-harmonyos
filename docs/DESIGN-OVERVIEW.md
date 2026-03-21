# HarmonyOS 插件 — 概要设计

> **对标对象**：Qt Creator 内置插件 `src/plugins/android`（以下简称 **Android 插件**）。  
> **本文档性质**：概要设计（目标、范围、架构、策略）；细节见同目录下 `DESIGN-DETAIL-*.md`。

---

## 1. 背景与目标

### 1.1 背景

在 Qt Creator 中开发 **Qt for HarmonyOS / OpenHarmony** 应用时，需要与 **SDK、NDK、工具链、设备、构建系统（CMake + hvigor/ohpm）、部署（hdc）** 打通。Android 插件已形成较完整的平台支持闭环；Harmony 插件以 **同等产品目标** 为参照，分阶段实现能力对齐。

### 1.2 设计目标

| 编号 | 目标 | 说明 |
|------|------|------|
| G1 | **平台闭环** | 配置 → Kit → 构建 → 部署 →（后续）运行/调试，主路径可用、可诊断 |
| G2 | **与 Android 插件对称** | 模块划分、扩展点使用方式、用户心智与 Android 侧尽量一致 |
| G3 | **可演进** | Qt Creator 大版本升级（19→20+）可通过版本宏与可选源码隔离（见 `VERSIONING.md`、`compat/`） |
| G4 | **依赖清晰** | 明确区分「IDE 内逻辑」与「外部 CLI/SDK」边界，便于测试与排错 |

### 1.3 非目标（当前概要阶段声明）

- 不替代 **DevEco Studio** 的全部能力（深度 UI 设计、完整模拟器管理等可后置）。
- 不在插件内重新实现 **ohpm/hvigor/hdc** 协议，以调用官方/本地安装的工具为准。
- 不承诺与 **所有** HarmonyOS 发行版工具链逐版本锁定兼容（通过文档与检测提示降低期望）。

---

## 2. 设计原则

1. **扩展点优先**：优先使用 Qt Creator 已有 **KitAspect、Toolchain、BuildStep、DeployConfiguration、RunConfiguration、IDevice** 等机制，避免平行造轮子。
2. **Android 对标表驱动**：以 Android 插件文件/能力为检查表，逐项标记 **已实现 / 部分 / 未实现 / 不适用（N/A）**（见 `DESIGN-DETAIL-ANDROID-MAPPING.md`）。
3. **失败可观测**：构建与部署错误尽量进入 **输出窗口 / Issues**，关键步骤记录日志（与 Android 的 `TaskHub`、deploy 错误解析思路一致）。
4. **最小可用路径先行**：P0 完成「真机 + CMake + HAP 构建 + 安装」再扩展模拟器、SDK 商店式管理、调试器深度集成。

---

## 3. 总体架构（逻辑分层）

```
┌─────────────────────────────────────────────────────────────┐
│                    Qt Creator 核心 / 公共插件                 │
│  (ProjectExplorer, CMakeProjectManager, QtSupport, …)       │
└────────────────────────────┬────────────────────────────────┘
                             │
┌────────────────────────────▼────────────────────────────────┐
│                    Harmony 插件 (libHarmony)                  │
│  ┌─────────────┐ ┌──────────────┐ ┌────────────────────────┐ │
│  │ 配置与持久化  │ │ Kit/Qt/工具链  │ │ 设备发现 (hdc+USB)      │ │
│  └─────────────┘ └──────────────┘ └────────────────────────┘ │
│  ┌─────────────┐ ┌──────────────┐ ┌────────────────────────┐ │
│  │ CMake/BC 扩展 │ │ Build HAP    │ │ Deploy / Run (演进)    │ │
│  └─────────────┘ └──────────────┘ └────────────────────────┘ │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │ lib: usbmonitor, ohprojectecreator, libusb (平台相关)      │ │
│  └─────────────────────────────────────────────────────────┘ │
└────────────────────────────┬────────────────────────────────┘
                             │
┌────────────────────────────▼────────────────────────────────┐
│  外部：Node, hvigorw, ohpm, hdc, Qt for Harmony, OHOS NDK …   │
└─────────────────────────────────────────────────────────────┘
```

**与 Android 的对应关系**：Android 将 SDK 管理、AVD、Gradle/APK、adb 安装、runner/debug 等分层落在 `android` 目录各模块；Harmony 采用 **相同分层意图**，实现文件与完成度见详细设计文档。

---

## 4. 模块全景（概要）

| 逻辑模块 | Harmony 主要承载 | Android 参照（示意） |
|----------|------------------|----------------------|
| 插件入口与生命周期 | `ohos.cpp` | `androidplugin.cpp` |
| 全局/Kit 配置 | `harmonyconfigurations`, `harmonysettingswidget` | `androidconfigurations`, `androidsettingswidget` |
| Qt 版本 | `harmonyqtversion` | `androidqtversion` |
| 工具链 | `harmonytoolchain` | `androidtoolchain` |
| 设备 | `harmonydevice`, `harmonydevicemanager` | `androiddevice`, AVD 相关 |
| CMake 构建配置 | `harmonybuildconfiguration` | iOS/Android 下 CMake 扩展思路 |
| 构建步骤 | `harmonybuildhapstep` | `androidbuildapkstep` |
| 部署 | `harmonydeployqtstep` | `androiddeployqtstep` |
| 运行 | `harmonyrunconfiguration` | `androidrunconfiguration` + runner |
| 工程辅助 | `lib/ohprojectecreator` | Manifest/Gradle 相关模块（目标不同） |

---

## 5. Qt Creator 版本与构建

- **最低 Qt Creator 19**（`QtTaskTree` 等 API）。  
- 版本探测与宏注入见 **`../VERSIONING.md`**、`../cmake/QtCreatorVersion.cmake`、`../src/harmonyqtcdefs.h`。

---

## 6. 文档体系说明

| 层级 | 文件 | 说明 |
|------|------|------|
| 索引 | [docs/README.md](README.md) | 本目录导航 |
| 概要 | **本文档** | 目标、原则、架构、模块一览 |
| 详细 | `DESIGN-DETAIL-*.md` | 分主题展开，见 [docs/README.md](README.md) 表格 |

---

## 7. 修订记录（建议维护）

| 版本 | 日期 | 说明 |
|------|------|------|
| 0.1 | （填写） | 初稿：概要设计 + 分文件详细设计 |
