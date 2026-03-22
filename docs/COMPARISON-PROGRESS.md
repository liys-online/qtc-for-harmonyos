# 功能对标矩阵：Android 插件 vs Harmony 插件

> **参照**：`src/plugins/android`  
> **对象**：`src/plugins/harmonyos`  
> **进度图例**：✅ 已完成（主路径可用） | 🔄 部分完成 / 简化实现 | ⬜ 未开始 | ➖ 不适用（Harmony 无对等或暂不规划）

**维护**：每完成一项对齐，更新本表对应行；Qt Creator 大版本升级时复核 Android 侧路径。模块级文件映射见 [ANDROID-PARITY.md](ANDROID-PARITY.md)；架构与已知限制见 [ARCHITECTURE.md](ARCHITECTURE.md)。

---

## 图例与统计（可随表更新）

| 图例 | 含义 |
|------|------|
| ✅ | 与 Android 对标能力在 Harmony 侧**主路径已具备** |
| 🔄 | **有实现但不完整**（需继续对齐体验/健壮性/全平台） |
| ⬜ | **尚未实现** |
| ➖ | **不对标**或当前阶段明确不做 |

---

## 1. 插件入口与生命周期

| # | Android 能力 / 典型文件 | Harmony 对应 | 进度 | 备注 |
|---|---------------------------|--------------|------|------|
| 1.1 | `IPlugin` 注册各模块 `setup*()` | `ohos.cpp` | ✅ | 已注册配置/设备/Qt/设置/BC/工具链/构建/部署/运行等 |
| 1.2 | `Q_PLUGIN_METADATA` + `*.json.in` 依赖声明 | `Harmony.json.in` + `CMakeLists.txt` 列入 `SOURCES` | ✅ | 需与主工程 `add_qtc_plugin` 流程一致 |
| 1.3 | `kitsRestored` 后 InfoBar 引导未配置 SDK | `ohos.cpp::kitsRestored` | ⬜ | 当前无 Android 式引导条 |
| 1.4 | `extensionsInitialized` 收尾逻辑 | `ohos.cpp` | ⬜ | 当前多为空 |
| 1.5 | `#ifdef WITH_TESTS` 注册测试 | — | ⬜ | Harmony 侧暂无 |
| 1.6 | 翻译加载 | `loadTranslations()` | 🔄 | 需确认资源与生命周期（如 translator 归属） |

---

## 2. 全局配置与持久化

| # | Android 能力 / 典型文件 | Harmony 对应 | 进度 | 备注 |
|---|---------------------------|--------------|------|------|
| 2.1 | `AndroidConfig` + QSettings 分组 | `HarmonyConfig` / `HarmonyConfigurations` | ✅ | 路径/SDK/qmake 等 mutator 已 `persistSettings()` 写回 QSettings（与 Android 持久化习惯对齐） |
| 2.2 | `android.xml` 等辅助持久化 | `harmony.xml`（若使用） | 🔄 | 需确认路径与是否存在默认文件 |
| 2.3 | SDK/NDK 校验与版本读取 | `isValidSdk`、`oh-uni-package.json` 等 | 🔄 | 错误提示已部分从 Android 文案改为 OHOS |
| 2.4 | 自动创建 Kit / 工具链联动 | `updateAutomaticKitList` 等 | 🔄 | 与 qmake/SDK 列表联动，需持续验证 |

---

## 3. 设置页（Options）

| # | Android 能力 / 典型文件 | Harmony 对应 | 进度 | 备注 |
|---|---------------------------|--------------|------|------|
| 3.1 | SDK/NDK/JDK/OpenSSL 等完整表单 | `harmonysettingswidget.*` | 🔄 | 字段集合不同；Deveco/SDK/qmake/Make |
| 3.2 | 全平台一致 UI | 同上 | 🔄 | Deveco 路径已扩展到非 Windows；仍需按平台验证可执行路径 |
| 3.3 | SDK 下载 / 引导安装 | `HarmonySdkDownloader`（列表 JSON） | 🔄 | 见 [SDK-PACKAGE-MANAGER.md](SDK-PACKAGE-MANAGER.md)；直链下载与向导待接 |
| 3.4 | SDK 包管理对话框 | `harmonysdkmanagerdialog` + 设置页入口 | 🔄 | 列表/下载/校验已有；解压与已安装检测待补 |
| 3.5 | 校验摘要（Summary） | `SummaryWidget` | 🔄 | 已有，规则需与 OHOS 工具链一致 |
| 3.6 | qmake 选择过滤器跨平台 | `addQmakeItem` | 🔄 | 已改为 `qmake*`，可再细化可执行后缀 |

---

## 4. Qt 版本

| # | Android 能力 / 典型文件 | Harmony 对应 | 进度 | 备注 |
|---|---------------------------|--------------|------|------|
| 4.1 | `AndroidQtVersion` 检测 ABI / builtWith | `HarmonyQtVersion` | 🔄 | qconfig / qdevice.pri；**已收紧** `targetAbi()` 解析（跳过 `#` 注释、支持 `OHOS_ARCH=`） |
| 4.2 | 多 ABI / 版本边界判断 | `supportsMultipleQtAbis` 等 | 🔄 | 存在硬编码版本号风险 |
| 4.3 | Qt 版本工厂注册 | `harmonyqtversion` | ✅ | 与 Android 模式一致 |

---

## 5. Kit / KitAspect

| # | Android 能力 / 典型文件 | Harmony 对应 | 进度 | 备注 |
|---|---------------------------|--------------|------|------|
| 5.1 | Android 专用 KitAspect（ABI、签名等） | 无独立 `harmonykitaspect` 模块 | ⬜ | 能力分散在配置/Kit 自动创建中，待对标拆分 |
| 5.2 | Kit 与设备、Qt、工具链组合 | Kit + `QtKitAspect` 等 | 🔄 | 自动 Kit CMake 项含 **`OHOS_ARCH` 规范化**、Unix **`CMAKE_MAKE_PROGRAM`**；与 qmake 列表联动仍待全场景验证 |

---

## 6. 工具链

| # | Android 能力 / 典型文件 | Harmony 对应 | 进度 | 备注 |
|---|---------------------------|--------------|------|------|
| 6.1 | NDK Clang 自动检测 | `harmonytoolchain.*` | 🔄 | Windows 偏重；make/ninja 等待全平台对齐 |
| 6.2 | 与宿主工具链父项同步 | Android `syncAutodetected...` | 🔄 | Harmony 侧逻辑需继续按平台补全 |
| 6.3 | 工厂注册 | `setupHarmonyToolchain` | ✅ | — |

---

## 7. 设备（IDevice）

| # | Android 能力 / 典型文件 | Harmony 对应 | 进度 | 备注 |
|---|---------------------------|--------------|------|------|
| 7.1 | `AndroidDevice` 完整属性与行为 | `HarmonyDevice` | 🔄 | Widget 更新、状态刷新等仍偏骨架 |
| 7.2 | 设备工厂 + 图标 | `HarmonyDeviceFactory` | 🔄 | 仍可能使用 Android 图标资源 |
| 7.3 | 物理设备枚举 | `HarmonyDeviceManager` + `hdc` | 🔄 | 解析依赖 hdc 输出格式 |
| 7.4 | `adb track-devices` 式持续监听 | USB + 轮询 hdc | 🔄 | USB 与序列号映射较粗 |
| 7.5 | AVD / 模拟器创建与启动 | — | ⬜ | 对标 `avdmanager` / `avdcreatordialog` |
| 7.6 | 设备文件访问 / 端口转发 | `AndroidFileAccess` 等 | ⬜ | 可对 `hdc file` / `fport` |

---

## 8. 构建步骤

| # | Android 能力 / 典型文件 | Harmony 对应 | 进度 | 备注 |
|---|---------------------------|--------------|------|------|
| 8.1 | `AndroidPackageInstallationStep`（install 到 android-build） | ➖ / 部分由 CMake | ➖ | OHOS 侧工程结构不同 |
| 8.2 | `AndroidBuildApkStep`（androiddeployqt + Gradle） | `HarmonyBuildHapStep`（hvigor/ohpm） | 🔄 | 签名 UI 大量注释；sync→ohpm→assemble；**已加固** Node 多路径、Java 自动、`DEVECO_SDK_HOME`/`JAVA_HOME`、`ohpro` 目录与 `PWD`/`INIT_CWD` |
| 8.3 | Keystore / 签名向导 | — / DevEco 按钮 | 🔄 | 对标 `KeystoreCertificateDialog` 未完整 |
| 8.4 | 构建输出解析（错误进 Issues） | `HarmonyBuildHapStep` 配置类失败 | 🔄 | 配置类失败已 `addOutput`+`BuildSystemTask`；**hvigor 编译期日志进 Issues 仍待做**（对标 `JavaParser`/Gradle） |
| 8.5 | 日志分类 `QLoggingCategory` | `harmonyBuildHapLog` 等 | 🔄 | 已部分引入，未全文件统一 |

---

## 9. 部署步骤

| # | Android 能力 / 典型文件 | Harmony 对应 | 进度 | 备注 |
|---|---------------------------|--------------|------|------|
| 9.1 | `androiddeployqt` + `adb install` 双路径 | `HarmonyDeployQtStep`（以 hdc 为主） | 🔄 | `harmonydeployqt` 分支仍注释 |
| 9.2 | 安装失败错误码解析与重试 | 已有类似结构 | 🔄 | 部分文案仍偏 adb 语境 |
| 9.3 | 卸载再安装 | 支持 | ✅ | — |
| 9.4 | HAP/APK 产物路径 | 自动探测 `*.hap` + 默认路径 | 🔄 | 已减少硬编码，边界 case 需继续测 |
| 9.5 | 自定义包安装按钮 | 「Install a HAP File…」 | ✅ | 前置校验 + 对话框提示；模拟器明确说明不支持 |

---

## 10. 运行配置与 RunWorker

| # | Android 能力 / 典型文件 | Harmony 对应 | 进度 | 备注 |
|---|---------------------------|--------------|------|------|
| 10.1 | `AndroidRunConfiguration` + Aspects | `HarmonyRunConfiguration` | 🔄 | 字段齐全度接近 |
| 10.2 | `RunWorkerFactory` + 实际启动 | `harmonyrunner.cpp` + `ProcessRunnerFactory` | 🔄 | 已能发起 `hdc shell` 启动；非 Android 级编排 |
| 10.3 | `am start` / `aa start` 参数与多步骤 shell | `aa start` + `preStartShellCmd` 合并；`postStartShellCmd` 在 hdc 会话结束后执行 | 🔄 | 与 Android 编排粒度仍不同，行为已对齐 |
| 10.4 | 部署前运行 / 设备未连处理 | Android `androidKicker` 等 | ⬜ | 需按 RunControl 模式补 |
| 10.5 | QML 调试端口等 | — | ⬜ | 对标 AndroidRunner QML 通道 |

---

## 11. 调试与分析

| # | Android 能力 / 典型文件 | Harmony 对应 | 进度 | 备注 |
|---|---------------------------|--------------|------|------|
| 11.1 | `AndroidDebugWorkerFactory`（LLDB 远程） | — | ⬜ | — |
| 11.2 | `AndroidQmlToolingSupportFactory` | — | ⬜ | — |
| 11.3 | RunnerWorker 内日志（logcat） | — | ⬜ | 可对 `hilog` |

---

## 12. 编辑器与语言

| # | Android 能力 / 典型文件 | Harmony 对应 | 进度 | 备注 |
|---|---------------------------|--------------|------|------|
| 12.1 | `AndroidManifestEditor` | — | ⬜ | 可对 `app.json5` / `module.json5` |
| 12.2 | `JavaEditor` + JLS | — | ⬜ | ArkTS/ETS 另议 |
| 12.3 | Manifest / Gradle 向导 | `ohprojectecreator` | 🔄 | 模板与版本表需维护 |

---

## 13. 其它 Android 独有模块（清单式）

| # | Android 文件（示例） | Harmony | 进度 |
|---|----------------------|---------|------|
| 13.1 | `splashscreencontainerwidget.*` | — | ⬜ |
| 13.2 | `androidsdkdownloader.*` | — | ⬜ |
| 13.3 | `sdkmanageroutputparser.*` | — | ⬜ |
| 13.4 | `androidpackageinstallationstep.*` | ➖ | ➖ |

---

## 14. 工程与基础设施

| # | 项 | 进度 | 备注 |
|---|-----|------|------|
| 14.1 | 统一 `QLoggingCategory` 替代调试向 `writeSilently` | 🔄 | 设备/插件/构建已部分完成 |
| 14.2 | `ohosconstants.h` 巨型未用常量清理 | ⬜ | 可对 `Parameter` 等拆分或删除 |
| 14.3 | ID 前缀统一（`Qt4ProjectManager.*` → `Harmony.*`） | ⬜ | 需迁移旧设置 |
| 14.4 | 单元测试 | ⬜ | 对标 Android 若干 `*_test.cpp` |

---

## 15. 快速总览（与 Android 闭环对比）

| 闭环阶段 | Android | Harmony 进度 |
|----------|---------|----------------|
| 配置 SDK/工具 | ✅ 成熟 | 🔄（qmake/SDK 等持久化与 hvigor 环境已加强，仍差 P0-09 引导等） |
| 创建/维护 Kit | ✅ | 🔄 |
| 构建产物 | ✅ APK/AAB | 🔄 HAP |
| 部署到设备 | ✅ | 🔄 |
| 运行 | ✅ | 🔄（已打通基础，缺完整 Worker 行为） |
| 调试 | ✅ | ⬜ |
| 编辑器/向导 | ✅ 较全 | 🔄 / ⬜ |

---

**相关文档**：[PRIORITY-PLAN.md](PRIORITY-PLAN.md)（执行任务与风险列）、[ARCHITECTURE.md](ARCHITECTURE.md)（设计说明与已知限制）、[README.md](README.md)（文档索引）。
