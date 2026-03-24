# 手工烟测清单（主路径）

> **用途**：满足 [RELEASE-CHECKLIST.md](RELEASE-CHECKLIST.md) **§5.1–5.2**；在**干净设置**或 `-temporarycleansettings`（`-tcs`）下执行。  
> **详细依赖与步骤**仍以 [OPERATIONS.md](OPERATIONS.md) 为准。

---

## 环境前提

- [ ] Qt Creator 版本与插件构建所用 **主版本**一致。
- [ ] 已用 `-pluginpath` 加载**本次待发布**的插件二进制。
- [ ] 本机已安装文档要求的 **OpenHarmony / DevEco 工具链、hdc、Node、Java** 等（见 OPERATIONS）。

---

## 主路径（P0）

| 步骤 | 操作 | 通过 |
|------|------|------|
| 1 | **Tools → Options → Harmony**（或等价入口）：配置 SDK / DevEco / qmake 等，保存无报错 | [ ] |
| 2 | 打开或导入 **Qt for OH CMake 工程**，Kit 能选到 Harmony 相关 Kit | [ ] |
| 3 | **构建**：执行含 **Build HAP** 的步骤，hvigor/ohpm 失败时有可见任务/输出 | [ ] |
| 4 | **部署**：设备已连接（`hdc list targets` 正常），部署成功或使用插件给出的明确错误 | [ ] |
| 5 | **运行**：应用能在设备上启动；停止运行后行为符合预期（含 `postStartShellCmd` 等若已配置） | [ ] |

---

## 支持矩阵（§5.2）

在 **Release Notes 中声明**的平台/场景下，至少各做一次上表 **1–5**：

| 矩阵项 | 说明 | 本次已测 | 通过 |
|--------|------|----------|------|
| OS | 例如 macOS 14 / Windows 11 / Ubuntu 22.04 | | [ ] |
| 设备 | 实体机 / 特定 hdc 版本 | | [ ] |

---

## 自动化回归（可选，§5.3）

在 **`-DWITH_TESTS=ON`** 构建整树或插件测试目标后：

```bash
/path/to/Qt\ Creator.app/Contents/MacOS/Qt\ Creator -test Harmony
```

（Windows / Linux 换为对应 `qtcreator` 可执行文件路径。）

- [ ] `Harmony` 测试通过，或说明失败原因与是否阻塞发布。

---

## 签字

| 角色 | 姓名 | 日期 |
|------|------|------|
| 执行人 | | |
| 复核人（可选） | | |
