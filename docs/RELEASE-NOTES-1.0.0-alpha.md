# Release Notes — Harmony 插件 **1.0.0-alpha**

> **预发布（prerelease）**：主路径可用，行为与 API 仍可能变化。  
> **本文件**：可直接粘贴到 GitHub / GitCode **Release 正文**（按需删改矩阵与签名段落）。

---

## 适用环境

- **Qt Creator**：**19.x** 起（与插件构建所用主版本须一致）；详见 [VERSIONING.md](../VERSIONING.md)。
- **本包构建信息**（发版时填写）：Qt Creator __________；OS __________；`IDE_VERSION` __________。

---

## 安装

使用与宿主 Qt Creator **同主版本**构建的插件，启动时增加插件路径，例如：

```bash
/path/to/Qt\ Creator.app/Contents/MacOS/Qt\ Creator -pluginpath /path/to/plugin/dir -temporarycleansettings
```

产物目录见插件根目录 [README.md](../README.md) 与 [BUILD-REPRODUCIBILITY.md](BUILD-REPRODUCIBILITY.md)。

---

## 本版本包含（概要）

- Harmony / OpenHarmony：**SDK 与工具链配置**、**CMake 工程**、**HAP 构建**（hvigor / ohpm）、**hdc 设备发现与部署**、**运行**主路径。
- **hvigor/ohpm** 常见错误行进入 **Issues**；**Harmony CMake 摘要**等构建配置辅助界面。
- **Native 调试**：**MVP**（`HarmonyDebugWorkerFactory`）；**不承诺**零售机 root + fport 全自动编排（见 [HARMONY-LLDB-DEBUG.md](HARMONY-LLDB-DEBUG.md)、[DEBUG-TASKS.md](DEBUG-TASKS.md)）。
- **用户手册**：[USER-MANUAL.md](USER-MANUAL.md)（与 [OPERATIONS.md](OPERATIONS.md) 分工：前者面向日常操作步骤，后者为依赖与管线细节）。

---

## 明确不包含或未完成（降低预期）

以下能力**未**与 Qt Creator Android 插件对齐，详见 [PRIORITY-PLAN.md](PRIORITY-PLAN.md)：

- 模拟器统一枚举/启动、hilog 流、完整签名向导、ArkTS LSP、新建工程向导成熟版等。
- **Run** 链路与 Android 级 Worker 编排仍有差距（计划表 **P0-01 部分完成**）。

---

## 已知限制与风险

- 依赖 **DevEco / hdc / 签名策略** 等外部环境；**user 镜像 + Native 调试** 需 **debug HAP** 与官方流程一致（见 [OPERATIONS.md](OPERATIONS.md) §2.4–2.5）。
- 插件为**个人开源项目**，**非** The Qt Company 官方产品（见 [AUTHORS.md](AUTHORS.md)）。

---

## 校验和（可选）

发版附件填写：

```
SHA256  Harmony<...>.dylib: 
SHA256  Harmony<...>.so:
SHA256  Harmony<...>.dll:
```

---

## 标签建议

**Git Tag / Release 名称**可使用预发布语义，例如 `v1.0.0-alpha` 或 `v1.0.0-alpha.1`。

**注意**：Qt Creator 要求插件 **`Harmony.json` 的 `Version` / `CompatVersion`** 仅为数字段（如 **`1.0.0`**），**不能**写成 `1.0.0-alpha`，否则加载报错。预发布信息放在 **Release 正文**、**Description** 或 Tag 名即可。

```bash
git tag -a v1.0.0-alpha -m "Harmony plugin 1.0.0-alpha (JSON Version 1.0.0)"
git push origin v1.0.0-alpha
```
