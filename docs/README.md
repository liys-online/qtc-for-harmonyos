# Harmony 插件设计文档索引

本目录存放 **HarmonyOS 插件** 的设计说明，**对标 Qt Creator `android` 插件** 的能力边界与演进路线。文档按「概要 → 分主题详细」拆分，避免单文件过长。

## 文档列表

| 文档 | 内容 |
|------|------|
| [**DESIGN-OVERVIEW.md**](DESIGN-OVERVIEW.md) | **概要设计**：目标、原则、总体架构、模块全景、与 Android 对齐策略 |
| [**DESIGN-DETAIL-ANDROID-MAPPING.md**](DESIGN-DETAIL-ANDROID-MAPPING.md) | **详细设计**：Android 源码模块 ↔ Harmony 实现/缺口对照 |
| [**DESIGN-DETAIL-ARCHITECTURE.md**](DESIGN-DETAIL-ARCHITECTURE.md) | **详细设计**：分层架构、依赖、关键数据流、Qt Creator 扩展点 |
| [**DESIGN-DETAIL-MODULES.md**](DESIGN-DETAIL-MODULES.md) | **详细设计**：子模块职责、主要类与文件路径 |
| [**DESIGN-DETAIL-BUILD-DEPLOY-RUN.md**](DESIGN-DETAIL-BUILD-DEPLOY-RUN.md) | **详细设计**：构建 / 部署 / 运行链路及外部工具 |
| [**DESIGN-DETAIL-ROADMAP.md**](DESIGN-DETAIL-ROADMAP.md) | **详细设计**：能力缺口、里程碑（P0–P2）、风险与假设 |

## 相关仓库内文档

- 根目录 [**README.md**](../README.md)：构建、运行、版本要求入口  
- [**VERSIONING.md**](../VERSIONING.md)：Qt Creator 版本策略（19+）  
- [**src/compat/README.md**](../src/compat/README.md)：大版本分岔源码约定  

阅读顺序建议：`DESIGN-OVERVIEW.md` → 按需打开各 `DESIGN-DETAIL-*.md`。
