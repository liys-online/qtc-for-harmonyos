# 详细设计：Android 插件对标映射

本文档以 **Qt Creator `src/plugins/android`** 为参照，说明 Harmony 插件的 **能力对应、实现位置与完成度**。状态约定：**✓** 已有主路径，**△** 部分/简化，**○** 规划/缺口，**—** 平台差异不适用。

---

## 1. 顶层与设置

| Android（典型路径） | 能力 | Harmony | 状态 |
|---------------------|------|---------|------|
| `androidplugin.cpp` | 插件注册、依赖 | `ohos.cpp` | ✓ |
| `androidconfigurations.*` | SDK/NDK 路径、环境 | `harmonyconfigurations.*` | △（路径模型不同，持续对齐） |
| `androidsettingswidget.*` | 设置页 UI | `harmonysettingswidget.*` | △ |

---

## 2. Qt 版本与 Kit

| Android | 能力 | Harmony | 状态 |
|---------|------|---------|------|
| `androidqtversion.*` | Qt for Android 检测 | `harmonyqtversion.*` | △ |
| `androidqtversionfactory.*` | 工厂注册 | 合入 `harmonyqtversion` 或等价 | △ |
| KitAspect（Android 侧） | ABI、SDK、签名等 | `harmonykitaspect.*` | △ |

---

## 3. 工具链

| Android | 能力 | Harmony | 状态 |
|---------|------|---------|------|
| `androidtoolchain.*` | NDK/Clang 工具链 | `harmonytoolchain.*` | △（OHOS NDK / clang 模型） |

---

## 4. 设备

| Android | 能力 | Harmony | 状态 |
|---------|------|---------|------|
| `androiddevice.*` | IDevice、属性 | `harmonydevice.*` | △ |
| `androiddevicemanager.*` | 枚举、连接 | `harmonydevicemanager.*`（hdc + libusb） | △ |
| AVD / 模拟器 | 虚拟设备 | — | ○（可后置或对接官方模拟器 CLI） |

---

## 5. 构建与 CMake

| Android | 能力 | Harmony | 状态 |
|---------|------|---------|------|
| Gradle / `androidbuildapkstep` | APK 构建步骤 | `harmonybuildhapstep.*`（hvigor/ohpm） | △ |
| `androidbuildapkwidget.*` | 步骤 UI | `harmonybuildhapwidget.*` | △ |
| CMake 与 BC | 多配置 | `harmonybuildconfiguration.*` | △ |

---

## 6. 部署与运行

| Android | 能力 | Harmony | 状态 |
|---------|------|---------|------|
| `androiddeployqtstep.*` | 打包与 adb 安装 | `harmonydeployqtstep.*`（hdc） | △ |
| `androidrunconfiguration.*` | 运行配置 | `harmonyrunconfiguration.*` | △ |
| `androidrunner.*` / debug | 启动与调试 | — | ○ |

---

## 7. 工程与资源

| Android | 能力 | Harmony | 状态 |
|---------|------|---------|------|
| Manifest / Gradle 向导 | 工程模板 | `lib/ohprojectecreator` | △（OpenHarmony 工程结构） |

---

## 8. 第三方库

| Android | Harmony | 说明 |
|---------|---------|------|
| 平台相关 adb/usb | `lib/3rdparty/libusb`、`lib/usbmonitor` | hdc 设备发现、USB 事件 |

---

## 9. 维护说明

- 对照表应随 **Android 插件目录结构变更**（大版本 Qt Creator）定期更新。  
- 若某 Android 文件在 IDE 中改名或合并，在本表 **保留「能力名」列** 为主键更新路径。
