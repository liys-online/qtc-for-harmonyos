# 详细设计：构建、部署与运行

---

## 1. 外部依赖清单

| 工具 | 用途 | 典型来源 |
|------|------|----------|
| Node.js | 运行 hvigor/ohpm 生态 | 官方安装包 / 系统包管理器 |
| hvigorw / ohpm | HAP 构建与依赖解析 | Harmony/OpenHarmony SDK 附带工程模板 |
| hdc | 设备通信、安装 HAP | SDK `toolchains` 下 |
| CMake + Ninja/Make | IDE 侧工程生成与 native 构建 | Qt Creator 已集成 |
| Qt for Harmony | 交叉 Qt 库与 qmake/cmake 描述 | Qt 官方或合作方发行 |

**设计约束**：插件内 **不写死** 绝对路径；以 **设置页 + 环境变量 + Kit** 解析为准，与 Android SDK/NDK 选择模式一致。

---

## 2. 构建管线（HAP）

### 2.1 输入

- CMake 工程（`CMakeLists.txt`）中由 **Qt / Harmony CMake 包** 提供的变量与目标。
- Kit 选定的 **Qt 版本、工具链、设备 ABI**（若多 ABI，与 Android 多 ABI 策略可对齐）。

### 2.2 步骤职责（HarmonyBuildHapStep）

1. 校验 **工作目录、ohpm、hvigor** 是否可用。
2. 组装命令行（或调用包装脚本）；**stderr/stdout** 回传至 Qt Creator 输出。
3. 失败时返回结构化错误（便于 Issues 面板，目标与 Android Gradle 步骤一致）。
4. 使用 **QtTaskTree** 表达子步骤（依赖、并行策略随实现演进）。

### 2.3 与 Android 差异

- Android：**Gradle** 主导 Java/Kotlin + 资源；Harmony：**hvigor** 主导 Stage 模型 + ArkTS/资源。
- 插件侧仍保持 **「BuildStep = 黑盒命令 + 日志 + 退出码」** 抽象，便于替换 CLI 版本。

---

## 3. 部署管线（hdc）

### 3.1 HarmonyDeployQtStep

- 解析当前 **目标设备**（`HarmonyDevice`）。
- 调用 `hdc install` / `hdc shell` 等（具体参数与 HAP 路径由构建产物与 CMake 安装规则决定）。
- 处理 **设备离线、证书、多设备** 等错误码，提示用户（对齐 `androiddeployqt` 的用户体验目标）。

### 3.2 产物路径

- 以 **BuildConfiguration 的 buildDirectory** 与 CMake 安装/`CMAKE_RUNTIME_OUTPUT_DIRECTORY` 约定为准；文档化「官方模板期望目录」，减少魔法字符串。

---

## 4. 运行配置（HarmonyRunConfiguration）

### 4.1 当前阶段

- 定义 **可启动的 RunConfiguration**，与 **Deploy** 步骤链组合。
- 启动方式可能为 **hdc shell 启动 Ability** 或等价命令（随模板演进）。

### 4.2 后续

- **调试**：对接 C++/JS 调试器与 Qt Creator **Debugger** 插件（里程碑见 `DESIGN-DETAIL-ROADMAP.md`）。
- **日志**：`hdc hilog` 或等价拉流至 IDE 输出（可选）。

---

## 5. 测试建议（工程侧）

- **单元测试**：路径拼接、参数转义、设备列表解析（mock hdc 输出）。
- **集成测试**：在 CI 用固定 SDK 版本镜像跑「干跑」命令（`--dry-run` 若工具支持）。

---

## 6. 日志与诊断

- 统一前缀，例如 `[Harmony]`，便于过滤。
- 在设置页提供 **「诊断」按钮**：打印版本、hdc targets、环境变量（脱敏）。
