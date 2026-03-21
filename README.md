# Qt Creator — Harmony / OpenHarmony 插件

在 **Qt Creator** 中集成 **Qt for HarmonyOS / OpenHarmony** 开发能力：SDK 与工具链配置、CMake 工程、HAP 构建（hvigor / ohpm）、设备发现与 **hdc** 部署。能力划分对标 Qt Creator 内置 **Android** 插件，便于统一产品预期。

## 作者与版权说明

本插件为 **个人开源项目**，著作权与维护由 **Li-Yaosong（个人）** 负责，**并非** The Qt Company Ltd. 官方作品，也**不属于**其雇员职务成果。插件通过 Qt Creator / Qt 的公开 API 与之集成；除另有说明外，插件源码以仓库内 **LICENSE** 及各文件 SPDX 头注释为准。作者列表见 **[AUTHORS](AUTHORS)**。

---

## 文档（对外）

完整说明见 **[docs/README.md](docs/README.md)**，核心文档包括：

| 文档 | 内容 |
|------|------|
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | 架构与设计目标、扩展点、已知限制 |
| [docs/MODULES.md](docs/MODULES.md) | 源码模块索引 |
| [docs/OPERATIONS.md](docs/OPERATIONS.md) | 构建 / 部署 / 运行与外部依赖 |
| [docs/COMPARISON-PROGRESS.md](docs/COMPARISON-PROGRESS.md) | 与 Android 插件功能对标及进度 |
| [docs/PRIORITY-PLAN.md](docs/PRIORITY-PLAN.md) | 按优先级的任务计划表 |

---

## 版本要求

- **最低 Qt Creator 19.x**（基于 `QtTaskTree` 等 API）。  
- **20+**：通过版本宏与可选源码分岔兼容，详见 **[VERSIONING.md](VERSIONING.md)**。

---

## 构建

在独立构建目录中执行：

```bash
cmake -DCMAKE_PREFIX_PATH=<path_to_qtcreator_build> -DCMAKE_BUILD_TYPE=RelWithDebInfo <path_to_this_plugin>/src
cmake --build .
```

`<path_to_qtcreator_build>` 为已与 Qt Creator 匹配的构建目录或开发包路径（macOS 可为 `Qt Creator.app/Contents/Resources/`）。

若需生成 **`docs/qt-oh-binary-catalog.v1.json`**（GitCode Release → 清单协议），可加 **`-DWITH_TESTS=ON`** 后编译目标 **`qt-oh-binary-catalog-generator`**，详见 **[test/README.md](test/README.md)**。

---

## 运行（加载插件）

启动兼容版本的 Qt Creator 并指定插件路径：

```bash
-pluginpath <path_to_built_plugin>
```

常见输出路径：

- Windows / Linux：`<build>/lib/qtcreator/plugins`
- macOS：`<build>/Qt Creator.app/Contents/PlugIns`

建议使用 `-temporarycleansettings`（或 `-tcs`）避免污染全局用户配置。在 Qt Creator 内调试时可将上述参数填入运行配置的「命令行参数」。

---

## 许可证

以本目录下 [LICENSE](LICENSE) 及 Qt Creator / Qt 相关条款为准；子模块若另有许可证声明，以子模块为准。
