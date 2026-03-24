# 发布后维护（清单 §9）

> 对应 [RELEASE-CHECKLIST.md](RELEASE-CHECKLIST.md) **§9**；非一次性任务，按版本迭代执行。

---

## 9.1 Issue 与路线图

- 为新反馈打标签：**environment**（环境）、**documentation**（文档）、**bug**（缺陷）、**enhancement**（改进）。
- 确认缺陷是否阻塞下一预发布；将可执行项映射到 [PRIORITY-PLAN.md](PRIORITY-PLAN.md) 或新开任务 ID。
- 高频问题可摘录到 [OPERATIONS.md](OPERATIONS.md) FAQ 段（若新增该段）或根 README「常见问题」。

---

## 9.2 链接与元数据同步

当发生以下变更时，务必同步 **`src/Harmony.json.in`** 与已发布 Release 说明中的链接：

- 默认分支名（如 `main` → `19.0-harmonyos`）；
- 仓库迁移或 **Url / DocumentationUrl** 域名变更；
- 文档入口路径调整。

并在 [BUILD-REPRODUCIBILITY.md](BUILD-REPRODUCIBILITY.md) **§5 发布记录**中保留历史一条以便追溯。
