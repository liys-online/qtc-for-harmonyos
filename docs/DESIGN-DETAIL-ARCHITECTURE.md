# 详细设计：架构与扩展点

---

## 1. 插件边界

**Harmony 插件**（目标 `Harmony`）依赖 Qt Creator 公共模块，例如：

- `Core`, `ProjectExplorer`, `CMakeProjectManager`, `QtSupport`, `Utils`, `Tasking`/`QtTaskTree`（随版本）
- 设备与工具链：`ProjectExplorer` 中的 `IDevice`、`Toolchain` 等抽象

**不进入插件二进制**的部分：Node、hvigorw、ohpm、hdc、DevEco 安装的 SDK/NDK——通过 **进程调用 + 路径配置** 集成。

---

## 2. 与 Android 插件的对称点

| 扩展点 / 概念 | 用途（Harmony） |
|---------------|-----------------|
| `IPlugin` | 注册 KitAspect、设备工厂、BuildStep/Deploy 工厂、QtVersion 工厂等 |
| `KitAspect` | Kit 上挂载 Harmony 专用字段（SDK、签名、模块名等，随产品演进） |
| `Toolchain` | OHOS 交叉编译工具链检测与 Kit 绑定 |
| `IDevice` / `IDevice::Ptr` | 物理设备抽象；与 `HarmonyDeviceManager` 协作 |
| `BuildStep` | HAP 构建（调用 hvigor/ohpm 管线） |
| `DeployStep` | hdc 安装与同步资源 |
| `RunConfiguration` | 启动应用（后续可接调试会话） |
| `QtVersion` | 识别「带 Harmony 目标的 Qt」并与 Kit 组合 |

---

## 3. 关键数据流（构建 → 部署）

```
用户触发 Build
    → CMakeProjectManager 生成 build 目录（含 Harmony CMake 变量）
    → HarmonyBuildConfiguration 选择 BuildSteps
    → HarmonyBuildHapStep：ProcessTask / QtTaskTree 执行外部命令
    → 输出 HAP / 中间产物

用户触发 Run / Deploy
    → HarmonyDeployQtStep：解析设备、调用 hdc install
    → （后续）HarmonyRunConfiguration：启动与日志
```

与 Android 的 **Gradle → APK → adb install** 在形态上对应；差异在 **构建编排工具**（hvigor vs Gradle）与 **设备通道**（hdc vs adb）。

---

## 4. 设备发现架构（概要）

- **HarmonyDeviceManager**：维护设备列表、刷新策略；可监听 USB 插拔（`usbmonitor` + 平台 libusb）。
- **HarmonyDevice**：实现 `IDevice` 契约；属性含序列号、型号、连接类型等。
- **与 Android 差异**：无统一「Google USB 驱动」叙事；以 **hdc list targets** 与 USB 过滤为主。

---

## 5. 并发与任务模型（Qt Creator 19+）

- 构建/部署长任务使用 **`QtTaskTree`** + `Utils::ProcessTask`（及 `QSingleTaskTreeRunner` 等），与当前 Qt Creator 主线一致。
- **注意**：避免在含 `Layouting::Group` 等符号的翻译单元中 **全局** `using namespace QtTaskTree`，防止命名冲突。

---

## 6. 配置持久化

- 全局 Harmony 设置（SDK、Node、默认路径）通过 `Core::ICore` 设置体系与 Android 类似。
- Kit 级字段通过 **KitAspect** 写入 `Kit`，保证工程可重现。

---

## 7. 安全与沙箱（后续）

- 外部命令执行需校验路径与白名单参数（防命令注入）。
- 设备操作权限提示策略可对齐 Android 插件的用户预期。
