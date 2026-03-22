# Harmony 插件 — 文档中心

面向 **集成方、研发与项目管理** 的公开文档入口。设计与任务状态以本目录为准；版本与兼容策略见仓库根目录 [VERSIONING.md](../VERSIONING.md)。

> **著作权**：本插件为 **Li-Yaosong** 的个人项目，**与 The Qt Company Ltd. 无隶属关系**；文档与源码归属以作者及仓库许可证为准。

---

## 建议阅读路径

| 角色 | 建议顺序 |
|------|----------|
| **产品 / 项目管理** | [ARCHITECTURE.md](ARCHITECTURE.md) → [COMPARISON-PROGRESS.md](COMPARISON-PROGRESS.md) → [PRIORITY-PLAN.md](PRIORITY-PLAN.md) |
| **研发 / 代码评审** | [ARCHITECTURE.md](ARCHITECTURE.md) → [MODULES.md](MODULES.md) → [OPERATIONS.md](OPERATIONS.md) → [ANDROID-PARITY.md](ANDROID-PARITY.md) |
| **技术支持 / 运维** | [OPERATIONS.md](OPERATIONS.md) → [VERSIONING.md](../VERSIONING.md) |

---

## 文档一览

| 文档 | 说明 |
|------|------|
| [**ARCHITECTURE.md**](ARCHITECTURE.md) | **架构与设计**：目标、原则、分层、扩展点、数据流、已知限制 |
| [**MODULES.md**](MODULES.md) | **源码模块**：主要文件职责与 `lib/` 说明 |
| [**OPERATIONS.md**](OPERATIONS.md) | **构建 / 部署 / 运行**：外部依赖与主流程；**§2.4–2.5** Native 调试符号、debug HAP 与 user/签名风险 |
| [**ANDROID-PARITY.md**](ANDROID-PARITY.md) | **与 Android 插件模块映射**（概要） |
| [**COMPARISON-PROGRESS.md**](COMPARISON-PROGRESS.md) | **功能对标矩阵与进度**（✅ 🔄 ⬜ ➖） |
| [**PRIORITY-PLAN.md**](PRIORITY-PLAN.md) | **优先级任务计划表**（P0→P3，含状态与风险列） |
| [**SDK-PACKAGE-MANAGER.md**](SDK-PACKAGE-MANAGER.md) | **OpenHarmony SDK 包管理器** + **Qt for OpenHarmony SDK 管理器**（Gitee API / [GitCode 发布页](https://gitcode.com/openharmony-sig/qt/releases)） |
| [**QT-OH-BINARY-CATALOG.md**](QT-OH-BINARY-CATALOG.md) | **Qt for OH 二进制清单 JSON 协议**（设计稿 + 仓库内 `qt-oh-binary-catalog.v1.json`） |
| [**HARMONY-LLDB-DEBUG.md**](HARMONY-LLDB-DEBUG.md) | **Native LLDB 调试与 Qt Creator 对接**：官方文档入口、流程归纳、P2-01 映射与验证清单（命令以官方为准） |
| [**DEBUG-TASKS.md**](DEBUG-TASKS.md) | **调试功能任务清单**：分阶段 checkbox（P2-01 / P2-07），从命令行验收到 `HarmonyDebugWorkerFactory` |

---

## 仓库内其它说明

| 路径 | 说明 |
|------|------|
| [../README.md](../README.md) | 插件简介、构建与运行命令 |
| [../VERSIONING.md](../VERSIONING.md) | Qt Creator 19+ 版本策略与 CMake 宏 |
| [../src/compat/README.md](../src/compat/README.md) | 大版本分岔源码约定 |

---

## 最近更新（文档）

| 日期 | 说明 |
|------|------|
| 2025-03-20 | 根据当前实现更新 [PRIORITY-PLAN.md](PRIORITY-PLAN.md)（P0-10、§7 近期落地、§4 下一阶段建议）、[COMPARISON-PROGRESS.md](COMPARISON-PROGRESS.md)、[OPERATIONS.md](OPERATIONS.md) |
| 2026-03-20 | 新增 [HARMONY-LLDB-DEBUG.md](HARMONY-LLDB-DEBUG.md)；后同步官方《LLDB 高性能调试器》全文结构、路径表、root/user 远程步骤与 FAQ（仍以 [官网](https://developer.huawei.com/consumer/cn/doc/harmonyos-guides/debug-lldb) 为准） |
| 2026-03-20 | 新增 [DEBUG-TASKS.md](DEBUG-TASKS.md)（P2-01 / P2-07 分阶段任务清单） |
| 2026-03-20 | [OPERATIONS.md](OPERATIONS.md) 增 §2.4–2.5（Native 调试构建约定、user/签名风险）；[DEBUG-TASKS.md](DEBUG-TASKS.md) 阶段 0.3–0.4 完成 |

---

## 文档维护约定

1. **功能进度**：更新 [COMPARISON-PROGRESS.md](COMPARISON-PROGRESS.md) 中的状态符号与备注。  
2. **任务执行**：以 [PRIORITY-PLAN.md](PRIORITY-PLAN.md) 为主表，完成项更新「状态」列。  
3. **架构变更**：同步 [ARCHITECTURE.md](ARCHITECTURE.md) 与 [MODULES.md](MODULES.md)。  
4. 已确认**不实现**的能力：在 [COMPARISON-PROGRESS.md](COMPARISON-PROGRESS.md) 与 [PRIORITY-PLAN.md](PRIORITY-PLAN.md) 中注明原因，避免重复开任务。
