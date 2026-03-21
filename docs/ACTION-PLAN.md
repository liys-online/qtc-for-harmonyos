# HarmonyOS 插件改进行动清单

> 基于 [GAP-ANALYSIS.md](GAP-ANALYSIS.md) 的逐项问题，分优先级列出可执行的改进任务。  
> 优先级：**P0** = 阻塞基本可用；**P1** = 产品级必需；**P2** = 锦上添花。

---

## P0 — 阻塞基本可用（不做则插件不可用/不完整）

### P0-1. 实现 Run 功能（当前完全不工作）
**关联问题**：R1, R4

- [ ] 新建 `harmonyrunner.cpp` / `.h`，注册 `HarmonyRunWorkerFactory`
- [ ] 实现 `harmonyrunnerworker.cpp` / `.h`：
  - 通过 `hdc shell aa start -a <AbilityName> -b <BundleName>` 启动应用
  - 对应 `RunConfiguration` 中的 `amStartArgs`、`preStartShellCmd`、`postStartShellCmd`
  - stdout/stderr 通过 `hdc hilog` 或等价拉流
- [ ] 在 `ohos.cpp` 的 `initialize()` 中调用 `setupHarmonyRunWorker()`
- **参考**：`androidrunner.cpp` (154 行) + `androidrunnerworker.cpp` (859 行)
- **预估新增**：~500–800 行

### P0-2. 修复 HAP 路径硬编码
**关联问题**：DP1

- [ ] 从构建产物或 `build-profile.json5` 中解析实际 HAP 输出路径
- [ ] 提供 fallback 与用户提示

### P0-3. 跨平台基本可用
**关联问题**：TC1, TC2, UI1

- [ ] `harmonytoolchain.cpp`：`makeCommand()` macOS/Linux 返回 `make` 或 `ninja`
- [ ] `syncAutodetectedWithParentToolchains` 添加非 Windows 逻辑
- [ ] `harmonysettingswidget.cpp`：DevEco 配置去掉 `#ifdef Q_OS_WIN` 或按平台适配
- [ ] qmake 文件过滤器按平台调整

---

## P1 — 产品级必需（可用但不够好）

### P1-1. 统一日志体系
**关联问题**：14.2

- [ ] 为每个模块创建 `QLoggingCategory`：

```cpp
Q_LOGGING_CATEGORY(harmonyConfigLog,  "qtc.harmony.config",  QtWarningMsg)
Q_LOGGING_CATEGORY(harmonyDeviceLog,  "qtc.harmony.device",  QtWarningMsg)
Q_LOGGING_CATEGORY(harmonyBuildLog,   "qtc.harmony.build",   QtWarningMsg)
Q_LOGGING_CATEGORY(harmonyDeployLog,  "qtc.harmony.deploy",  QtWarningMsg)
Q_LOGGING_CATEGORY(harmonyRunLog,     "qtc.harmony.run",     QtWarningMsg)
```

- [ ] 将所有 `MessageManager::writeSilently` 调试输出改为 `qCDebug`
- [ ] 将所有裸 `qDebug` 改为对应类别的 `qCDebug`
- [ ] 保留 `MessageManager::writeDisrupting` 仅用于用户必须看到的关键错误

### P1-2. 统一错误处理
**关联问题**：DP2, 14.3

- [ ] 所有 BuildStep/DeployStep 的错误路径统一使用 `reportWarningOrError` + `TaskHub::addTask`
- [ ] 所有 early return 之前给用户提示
- [ ] `QMessageBox::critical(nullptr,...)` 改为 `Core::ICore::dialogParent()`

### P1-3. 实现构建输出解析器（OutputTaskParser）
**关联问题**：ED3

- [ ] 新建 `harmonyparser.cpp` / `.h`
- [ ] 解析 hvigor / ohpm 常见错误格式，映射到源文件
- [ ] 在 `ohos.cpp` 中注册
- **参考**：`javaparser.cpp` (70 行)
- **预估新增**：~100–200 行

### P1-4. 实现设备状态刷新
**关联问题**：D2, D6

- [ ] `updateDeviceState()`：调用 `hdc list targets` 更新设备状态
- [ ] `HarmonyDeviceWidget::updateDeviceFromUi()`：实现 UI 到设备模型更新
- [ ] hdc 输出解析增加格式校验 + 容错（正则或结构化解析）

### P1-5. 清理硬编码与魔法字符串
**关联问题**：B3, B4, 14.4

- [ ] ohpm 源 URL → 设置页或环境变量
- [ ] `deviceTypes = {"2in1"}` → 从 Kit/设备推导或设置页选择
- [ ] `"/ohpro"` → 使用 `Constants::HARMONY_BUILD_DIRECTORY`
- [ ] 各下载 URL → 集中到 `ohosconstants.h` 常量
- [ ] 移除输出中的 `"\033[0m"` ANSI 转义码

### P1-6. ID 前缀统一
**关联问题**：C2, C3

- [ ] 将所有 `Qt4ProjectManager.Harmony*` 改为 `Harmony.*`
- [ ] 将 `LL.Harmony Configurations` 改为 `Harmony.Settings`
- [ ] **注意**：需要处理持久化迁移（旧配置中的 ID），可在 `fromMap` 中做兼容

### P1-7. 命名空间统一
**关联问题**：B6, 14.1

- [ ] `harmonybuildhapstep.cpp` 结尾 `namespace Harmony::Internal` → `Ohos::Internal`
- [ ] 全局搜索确认无其他不一致

### P1-8. 拼写错误修正
**关联问题**：CF1, D8, W3, 14.6

- [ ] `deaultDevice` → `defaultDevice`
- [ ] `possilbe` → `possible`
- [ ] `Hamony` → `Harmony`
- [ ] `ProjecteCreator` — 评估是否改名（公共 API 需注意兼容）

### P1-9. 精简 ohosconstants.h
**关联问题**：C1

- [ ] 将 `Parameter` 命名空间（~800 行系统能力常量）移到 `harmonysyscaps.h`
- [ ] 或直接删除（当前插件代码未引用）

### P1-10. 恢复或清理签名相关代码
**关联问题**：B1

- [ ] 评估 HarmonyOS 签名流程（p12/p7b 证书 vs DevEco 签名）
- [ ] 决定：(a) 恢复并适配 Harmony 签名；(b) 保持 DevEco 联动按钮，删注释代码
- [ ] 无论哪条路，清除 `harmonybuildhapstep.cpp` 中 ~70 行注释块

### P1-11. kitsRestored 增加配置引导
**关联问题**：E3, E4

- [ ] 仿 Android 在 `kitsRestored` 中检查 SDK/Node/hdc 是否配置
- [ ] 未配置时显示 InfoBar 引导用户到设置页
- [ ] 恢复或移除注释的 `registerQtVersions()` 调用

### P1-12. 构建步骤代码重构
**关联问题**：B5, B8

- [ ] 提取 `syncProjectTask`、`ohpmInstallTask`、`defaultProcessTask` 中的 Process 配置到公共 helper
- [ ] `init()` 中的三元运算符副作用逻辑改为 if-else

---

## P2 — 锦上添花（后续演进）

### P2-1. Debug 支持
**关联问题**：R2

- [ ] 新建 `harmonydebugsupport.cpp`
- [ ] 通过 hdc 推送 lldb-server + `hdc fport` 端口转发
- [ ] 注册 `HarmonyDebugWorkerFactory`
- **参考**：`androiddebugsupport.cpp` (190 行) + `androidrunnerworker.cpp` debug 部分

### P2-2. HarmonyOS 专属图标
**关联问题**：D1

- [ ] 设计或获取 HarmonyOS 设备图标（small + normal）
- [ ] 添加资源文件并更新 `HarmonyDeviceFactory`

### P2-3. 模拟器支持
**关联问题**：Android AVD 对标

- [ ] 评估 HarmonyOS 模拟器 CLI 能力
- [ ] 实现模拟器枚举与启动

### P2-4. app.json5 / module.json5 编辑器
**关联问题**：ED1

- [ ] 仿 `AndroidManifestEditor` 实现
- [ ] 解析 Stage 模型配置文件

### P2-5. SDK 包管理
**关联问题**：SDK1, SDK2

- [ ] 评估 DevEco 命令行工具或 ohpm 自动安装能力

### P2-6. 单元测试
**关联问题**：E6

- [ ] hdc 输出解析测试（mock 数据）
- [ ] SDK 路径验证测试
- [ ] ohpm/hvigor 参数组装测试
- [ ] 在 `ohos.cpp` 中 `#ifdef WITH_TESTS` 注册

### P2-7. DeviceFileAccess
**关联问题**：D4, D5

- [ ] 实现基于 `hdc file send/recv` 的文件访问
- [ ] 实现 `hdc fport` 端口转发

### P2-8. Qt 版本增强
**关联问题**：QV1, QV2, QV3

- [ ] 移除 `supportsMultipleQtAbis()` 硬编码版本
- [ ] 实现 ABI 自动解析
- [ ] 增加 `isValid()` 校验

### P2-9. 工具链重构
**关联问题**：TC5

- [ ] 将 `WarningFlagAdder`（~300 行）拆到 `harmonytoolchain_warnings.cpp`

### P2-10. 内存泄漏修复
**关联问题**：E5, 14.5

- [ ] `QTranslator` 设 parent 为 plugin
- [ ] `OhProjecteCreator` 单例析构路径明确化

### P2-11. ohprojectecreator 清理
**关联问题**：W2, W4

- [ ] `sdkVersionMap` 改为从 SDK 目录枚举
- [ ] 检查资源模板中是否有个人绝对路径，替换为占位符

---

## 执行建议

1. **第一轮**（P0）：目标让插件在 macOS/Linux 上也能「构建 → 部署 → 运行」闭环。完成后即可做日常开发验证。
2. **第二轮**（P1-1 ~ P1-7）：代码质量层面的系统性改进，一次 PR 可包含多项。
3. **第三轮**（P1-8 ~ P1-12 + P2 按需）：产品打磨，逐步向 Android 对齐。

每项完成后建议同步更新 [DESIGN-DETAIL-ANDROID-MAPPING.md](DESIGN-DETAIL-ANDROID-MAPPING.md) 中对应模块的状态标记。
