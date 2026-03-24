# Harmony 插件 — 用户手册

> **读者**：在 Qt Creator 中使用本插件开发 **Qt for HarmonyOS / OpenHarmony** 的开发者（**非**插件源码贡献者）。  
> **深度说明**：依赖清单、hvigor 环境细节、调试矩阵等见 [OPERATIONS.md](OPERATIONS.md)；架构与限制见 [ARCHITECTURE.md](ARCHITECTURE.md)。  
> **配图**：演示用截图放在 [`docs/images/user-manual/`](images/user-manual/README.md)，文件名与下文章节 **图 2-x** 对应；未放入文件前 Markdown 中会显示**裂图**，属正常现象。

---

## 1. 开始前

| 项目 | 要求 |
|------|------|
| Qt Creator | **19.x 及以上**（与插件二进制所针对的**主版本**一致），见根目录 [VERSIONING.md](../VERSIONING.md) |
| 外载插件 | 使用 `-pluginpath` 指向编译好的插件目录，建议首次加 `-temporarycleansettings`（或 `-tcs`）避免污染全局配置，见 [README.md](../README.md) |
| 本机环境 | **DevEco / OpenHarmony SDK**、**Node**、**JDK**、**hdc**、**Qt for OH** 等需自行安装；插件通过设置页绑定路径，不写死绝对路径 |

**预发布版本**：若使用 **1.0.0-alpha** 等，行为可能与后续版本有差异，以发行说明为准。

---

## 2. 使用演示（截图占位）

以下按**推荐首次上手顺序**编排。请将截图保存到 `docs/images/user-manual/`，文件名请按下列示例命名以便替换占位。

### 2.1 新建并运行一个最小示例（逐步）
- 目的：从新建项目到在设备上运行最小“Hello Harmony”应用，覆盖新建、构建、部署与运行验证。
- 前提：`Qt Creator` 已安装且插件启用；已在 `Tools → Options` 中配置好 Harmony SDK；目标设备已连接。
- 步骤（逐步）：
  1. 打开新建向导：`File → New File or Project`，选择 `Application` 下的 `HarmonyOS` 模板。
	  - 截图占位：`fig-02-01-newproject-template.png`（新建向导模板选择页）。
  2. 填写项目信息：项目名 `HelloHarmony`、项目位置、包名等。
	  - 截图占位：`fig-02-02-newproject-info.png`（项目信息填写页）。
  3. 配置构建套件：在 `Projects` 面板中选择构建套件（例如 Debug/Release）。
	  - 截图占位：`fig-02-03-projects-kits.png`（构建套件选择）。
  4. 构建项目：点击 `Build` 并查看 `Compile Output`，确认生成 `.hap`。
	  - 截图占位：`fig-02-04-build-output.png`（构建输出与产物）。
  5. 部署到设备：在运行配置中选择设备并点击 `Run/Deploy` 或使用 `hdc install <app.hap>`。
	  - 截图占位：`fig-02-05-deploy-progress.png`（部署进度）。
  6. 验证：在设备上确认应用界面（例如显示“Hello Harmony”）。
	  - 截图占位：`fig-02-06-app-on-device.png`（设备上运行界面）。

### 2.2 设备管理与日志抓取（逐步）
- 目的：在插件内管理设备并抓取运行日志用于问题定位。
- 步骤（逐步）：
  1. 打开设备面板：侧边栏 `Devices` / `Harmony Device`。
	  - 截图占位：`fig-02-07-device-panel.png`（设备列表）。
  2. 列出设备：在终端或插件内运行 `hdc list`。
	  - 截图占位：`fig-02-08-hdc-list.png`（hdc list 输出）。
  3. 查看设备信息：`hdc deviceinfo <id>` 或面板详情。
	  - 截图占位：`fig-02-09-deviceinfo.png`（设备信息）。
  4. 抓取日志：在日志视图运行 `hdc dlog` / `hdc logcat` 并使用过滤器定位关键日志。
	  - 截图占位：`fig-02-10-log-view.png`（日志与过滤器）。
  5. （可选）传输或安装文件：`hdc push` / `hdc install`。
	  - 截图占位：`fig-02-11-hdc-install.png`（安装输出）。

### 2.3 生成发布包与签名（逐步）
- 目的：生成可发布的 `.hap` 并完成签名配置。
- 步骤（逐步）：
  1. 打开打包界面：`Build/Package` 或插件的 `Package` 菜单。
	  - 截图占位：`fig-02-12-package-entry.png`（打包入口）。
  2. 填写包信息：版本号、包名、目标 API 等。
	  - 截图占位：`fig-02-13-package-info.png`（包信息填写）。
  3. 配置签名证书：选择证书文件并输入密码。
	  - 截图占位：`fig-02-14-certificate-config.png`（证书配置）。
  4. 执行打包并验证签名与完整性。
	  - 截图占位：`fig-02-15-package-complete.png`（打包完成）。

更多截图规范与标注请参见 [`HARMONY-SCREENSHOT-TEMPLATES.md`](HARMONY-SCREENSHOT-TEMPLATES.md)。

## 3. 首次配置（文字要点）

与 **§2** 演示对应，便于检索：

1. **Harmony 设置**：SDK、qmake、Node/Java、ohpm、device types — 见 **图 2-3、图 2-4**。  
2. **InfoBar 引导**：缺 SDK 或未注册 Harmony Qt 时可能出现 — 可另存截图补充至 `images/user-manual/` 并在本节增加引用。  
3. **Kits** — 见 **图 2-5**。  

更细的依赖说明见 [OPERATIONS.md](OPERATIONS.md) §1–§2。

---

## 4. 工程与构建

1. 使用 **CMake** 的 Qt for OH 工程（由 Qt / 团队模板提供）。  
2. 在 **Projects** 中为该工程选择 **Harmony 相关 Kit**。  
3. **Harmony CMake 摘要**、**HAP 构建步骤**、**输出与 Issues** — 见 **§2 图 2-6～图 2-8**。  
4. 构建失败时，根据任务与输出检查 SDK、Node/Java、**ohpro** 工作目录等 — 详见 [OPERATIONS.md](OPERATIONS.md) §1–§2。

---

## 5. 设备与部署

1. USB 或网络调试下确保 **hdc** 可见设备 — 见 **图 2-9**。  
2. **部署**步骤与 HAP 路径规则见 [OPERATIONS.md](OPERATIONS.md) §3。  
3. **安装本地 HAP** 等对话框可按需另存截图并加入 `images/user-manual/`（建议命名 `fig-02-xx-deploy-dialog.png`）。

---

## 6. 运行

1. **运行配置** — 见 **图 2-10、图 2-11**。  
2. `aa start`、会话保持与 **post-quit** 行为见 [OPERATIONS.md](OPERATIONS.md) §4。  
3. 与 Android 级 Run 编排差异见 [PRIORITY-PLAN.md](PRIORITY-PLAN.md) **P0-01**。

---

## 7. 调试（Native / LLDB）

- 当前为 **MVP** 级集成：**user 镜像 + debug HAP** 等须符合华为官方与 Qt 文档；**零售机无 root** 场景下**不做** root + **fport** 全自动编排。  
- 请先阅读 [HARMONY-LLDB-DEBUG.md](HARMONY-LLDB-DEBUG.md) 与 [OPERATIONS.md](OPERATIONS.md) **§2.4–2.5**。  
- 界面演示见 **§2.8 图 2-12**（可选）。  
- 任务阶段：[DEBUG-TASKS.md](DEBUG-TASKS.md)。

---

## 8. 文档与插件元数据中的链接

- **本手册**与 [文档索引](README.md) 适合人类阅读。  
- Qt Creator **关于插件**里「文档」链接来自 **`Harmony.json` 的 `DocumentationUrl`**（由 `Harmony.json.in` 生成）；若与你所用仓库布局不一致，请向维护者反馈或自行在源码中调整 URL。

---

## 9. 常见问题（简表）

| 现象 | 建议 |
|------|------|
| 找不到 SDK / qmake | 打开 **Harmony 设置** 逐项检查；看 InfoBar 是否引导到同一页 |
| hvigor / ohpm 报找不到 node 或 java | 安装 DevEco 或设置 **JAVA_HOME**；参阅 [OPERATIONS.md](OPERATIONS.md) §1 |
| hdc 无设备 | 本机执行 `hdc list targets`；检查驱动与 USB 调试授权 |
| Issues 无法点击某条错误中的路径 | 路径可能为相对路径或工程外文件；以原始编译输出为准 |
| 期望与 Android 插件完全一致 | 能力对标见 [COMPARISON-PROGRESS.md](COMPARISON-PROGRESS.md)，路线图见 [PRIORITY-PLAN.md](PRIORITY-PLAN.md) |
| 用户手册图片不显示 | 将对应 PNG 放入 [`docs/images/user-manual/`](images/user-manual/README.md)，文件名与图注一致 |

---

## 10. 版权与性质

本插件为 **个人开源项目**，**非** The Qt Company 官方产品。详见 [AUTHORS.md](AUTHORS.md) 与根 [README.md](../README.md)。
