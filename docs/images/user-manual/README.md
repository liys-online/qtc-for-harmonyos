# 用户手册配图目录

将截图保存为下表中的 **文件名**（建议 **PNG**，宽度约 **1280～1920 px**，关键区域可局部放大另存为补充图）。

路径相对于 **`docs/USER-MANUAL.md`** 的引用为：`images/user-manual/<文件名>`。

| 文件名 | 建议画面内容 | 对应手册 |
|--------|----------------|----------|
| `fig-02-01-pluginpath-terminal.png` | 终端中启动 Qt Creator 并带 `-pluginpath`（可选，用于高级用户） | §2 图 2-1 |
| `fig-02-02-about-plugins-harmony.png` | **Help → About Plugins**，列表中选中 **Harmony**，可见版本与描述 | §2 图 2-2 |
| `fig-02-03-preferences-harmony-overview.png` | **Preferences → Harmony** 主界面（SDK、qmake、Node/Java 等可见） | §2 图 2-3 |
| `fig-02-04-preferences-harmony-ohpm-device-types.png` | 同一页中 **ohpm registry / strict SSL** 与 **module device types** 多选区域（若过长可分两张图，第二张命名 `fig-02-04b-...`） | §2 图 2-4 |
| `fig-02-05-preferences-kits-harmony.png` | **Preferences → Kits**，选中 Harmony 相关 Kit，Qt 版本与设备/CMake 摘要可见 | §2 图 2-5 |
| `fig-02-06-projects-build-harmony-summary.png` | **Projects → Build**，打开 **Harmony** 子页（CMake 摘要） | §2 图 2-6 |
| `fig-02-07-build-steps-hap.png` | 构建配置中的 **Harmony HAP** 步骤及子步骤/选项 | §2 图 2-7 |
| `fig-02-08-build-output-issues.png` | **编译输出** 与 **Issues** 中出现 hvigor/ohpm 可解析错误行（示例工程即可） | §2 图 2-8 |
| `fig-02-09-devices-harmony.png` | **设备**视图：Harmony 设备在线、`刷新` 可用（可选附 `hdc list targets` 终端） | §2 图 2-9 |
| `fig-02-10-run-configuration-harmony.png` | **运行配置**中 Harmony 目标、Bundle/Ability 等字段 | §2 图 2-10 |
| `fig-02-11-run-and-app-on-device.png` | 点击运行后，**应用程序输出**或设备侧已启动应用（可分两张） | §2 图 2-11 |
| `fig-02-12-debug-session-optional.png` | （可选）**Debug** 会话已附加、断点命中或 LLDB 相关提示 | §2 图 2-12 |

**命名扩展**：若需补充图，请用 `fig-02-13-*.png` 顺延，并在 `USER-MANUAL.md` 中增加对应图注与引用。

**隐私**：截图前请去除账号、内网路径、序列号等敏感信息。
