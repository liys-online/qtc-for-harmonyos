# 详细设计：子模块与源码索引

以下为 **Harmony 插件 `src/` 下主要单元** 的职责说明（文件名不含路径前缀时默认在 `src/plugins/harmonyos/src/`）。

---

## 1. 插件核心

| 单元 | 职责 |
|------|------|
| `ohos.cpp` / `ohos.h` | 插件入口：`initialize`/`extensionsInitialized`、注册各工厂与 KitAspect |
| `harmonyconstants.h` | 常量：ID、显示名、MIME、设置键等 |
| `harmonytr.h` | 翻译上下文 |
| `harmonyqtcdefs.h` | Qt Creator 版本宏封装（见 `VERSIONING.md`） |

---

## 2. 配置与 UI

| 单元 | 职责 |
|------|------|
| `harmonyconfigurations.*` | 全局 Harmony 配置读写、与 Kit/Qt 版本协作 |
| `harmonysettingswidget.*` | 选项页：SDK、工具路径等 |
| `harmonykitaspect.*` | Kit 维度字段与 UI |

---

## 3. Qt 与工具链

| 单元 | 职责 |
|------|------|
| `harmonyqtversion.*` | 检测/描述「支持 Harmony 的 Qt」 |
| `harmonytoolchain.*` | OHOS 工具链检测、`DetectionSource` 等与 IDE API 对齐 |

---

## 4. 设备

| 单元 | 职责 |
|------|------|
| `harmonydevice.*` | `IDevice` 实现 |
| `harmonydevicemanager.*` | 枚举 hdc 设备、USB 事件、任务树刷新 |
| `harmonydevicewidget.*` | 设备管理 UI 片段 |

---

## 5. 构建

| 单元 | 职责 |
|------|------|
| `harmonybuildconfiguration.*` | CMake BuildConfiguration 扩展、子配置控件 |
| `harmonybuildhapstep.*` | HAP 构建步骤实现 |
| `harmonybuildhapwidget.*` | 构建步骤参数 UI |

---

## 6. 部署与运行

| 单元 | 职责 |
|------|------|
| `harmonydeployqtstep.*` | 部署步骤：hdc、资源同步 |
| `harmonyrunconfiguration.*` | 运行配置工厂与默认行为 |

---

## 7. 库与子目录

| 路径 | 职责 |
|------|------|
| `lib/usbmonitor/` | USB 设备变化通知（平台相关） |
| `lib/ohprojectecreator/` | OpenHarmony 工程/模块创建辅助 |
| `lib/3rdparty/libusb/` | libusb 构建与平台 `config.h` |
| `src/compat/` | 按 Qt Creator 大版本可选追加的 `.cpp`（见 `compat/README.md`） |

---

## 8. CMake 组织

- 根 `CMakeLists.txt`：子项目入口（若独立构建）。
- `src/CMakeLists.txt`：目标 `Harmony`、包含 `cmake/QtCreatorVersion.cmake`、`HarmonyQtcOptionalSources.cmake`、`${HARMONY_QTC_EXTRA_SOURCES}`。

---

## 9. 与 Android 目录习惯对照

Android 插件通常按 **单文件单职责** 拆在 `android/` 下大量 `.cpp`；Harmony 采用 **同名语义模块**（如 `harmony*step`、`harmony*configuration`），便于 diff 与评审时 **按能力名** 对齐而非按字母序盲对齐。
