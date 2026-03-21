# Harmony 插件 — 文档中心

面向 **集成方、研发与项目管理** 的公开文档入口。设计与任务状态以本目录为准；版本与兼容策略见仓库根目录 [VERSIONING.md](../VERSIONING.md)。

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
| [**OPERATIONS.md**](OPERATIONS.md) | **构建 / 部署 / 运行**：外部依赖与主流程 |
| [**ANDROID-PARITY.md**](ANDROID-PARITY.md) | **与 Android 插件模块映射**（概要） |
| [**COMPARISON-PROGRESS.md**](COMPARISON-PROGRESS.md) | **功能对标矩阵与进度**（✅ 🔄 ⬜ ➖） |
| [**PRIORITY-PLAN.md**](PRIORITY-PLAN.md) | **优先级任务计划表**（P0→P3，含状态与风险列） |

---

## 仓库内其它说明

| 路径 | 说明 |
|------|------|
| [../README.md](../README.md) | 插件简介、构建与运行命令 |
| [../VERSIONING.md](../VERSIONING.md) | Qt Creator 19+ 版本策略与 CMake 宏 |
| [../src/compat/README.md](../src/compat/README.md) | 大版本分岔源码约定 |

---

## 文档维护约定

1. **功能进度**：更新 [COMPARISON-PROGRESS.md](COMPARISON-PROGRESS.md) 中的状态符号与备注。  
2. **任务执行**：以 [PRIORITY-PLAN.md](PRIORITY-PLAN.md) 为主表，完成项更新「状态」列。  
3. **架构变更**：同步 [ARCHITECTURE.md](ARCHITECTURE.md) 与 [MODULES.md](MODULES.md)。  
4. 已确认**不实现**的能力：在 [COMPARISON-PROGRESS.md](COMPARISON-PROGRESS.md) 与 [PRIORITY-PLAN.md](PRIORITY-PLAN.md) 中注明原因，避免重复开任务。
