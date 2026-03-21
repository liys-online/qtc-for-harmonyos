# 与 Android 插件模块映射（概要）

**目的**：为阅读 Qt Creator 源码或做代码评审时，快速将 **Android 插件**（`src/plugins/android`）与 **Harmony 插件**（`src/plugins/harmonyos`）在**模块级**对应起来。

**功能级完成度与进度**不以本文为准；请以 **[COMPARISON-PROGRESS.md](COMPARISON-PROGRESS.md)** 中的对标矩阵为准。

---

## 模块对照表

| Android（典型路径） | Harmony（主要承载） |
|---------------------|---------------------|
| `androidplugin.cpp` | `ohos.cpp` |
| `androidconfigurations.*` | `harmonyconfigurations.*` |
| `androidsettingswidget.*` | `harmonysettingswidget.*` |
| `androidqtversion.*` | `harmonyqtversion.*` |
| `androidtoolchain.*` | `harmonytoolchain.*` |
| `androiddevice.*` | `harmonydevice.*` |
| `androiddevicemanager.*`（概念） | `harmonydevicemanager.*` + `lib/usbmonitor` |
| `androidbuildapkstep.*` | `harmonybuildhapstep.*` |
| `androiddeployqtstep.*` | `harmonydeployqtstep.*` |
| `androidrunconfiguration.*` | `harmonyrunconfiguration.*` |
| `androidrunner.*` / `androidrunnerworker.*` | `harmonyrunner.*`（及后续自定义 Worker，见 [PRIORITY-PLAN.md](PRIORITY-PLAN.md)） |
| `androiddebugsupport.*` | （规划中） |
| Manifest / Gradle 向导等 | `lib/ohprojectecreator`（OpenHarmony 工程结构） |
| `androidsdkmanager*` 等 | （规划中，模型可能与 Android 不对等） |

---

## 维护说明

- Qt Creator 大版本升级时，Android 侧文件名可能调整；更新本表时以**能力名称**为主键，路径随源码变更修订。  
- 若某能力在 Harmony 侧明确不对标，在 [COMPARISON-PROGRESS.md](COMPARISON-PROGRESS.md) 中标记为 **➖** 并备注原因。
