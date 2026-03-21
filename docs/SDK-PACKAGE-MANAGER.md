# OpenHarmony SDK 包管理器（对标 Android SDK Manager）

> **目标**：在 Qt Creator Harmony 插件中提供与 **Android SDK Manager** 相近的体验：**浏览远程/已安装组件、选择下载目录、显示进度与校验结果**。  
> **与 Android 的差异**：Android 依赖 Google `sdkmanager` CLI（文本输出 + 许可证交互）；OpenHarmony 侧通常使用 **HTTP JSON 列表 + 直链下载 ZIP**，更接近你编写的 `ohos_sdk_downloader.py`。

---

## Qt for OpenHarmony SDK 管理器（`harmonyqttsdkmanagerdialog`）

> **清单协议**：字段定义见 [**QT-OH-BINARY-CATALOG.md**](QT-OH-BINARY-CATALOG.md)（`schema`: `qt-for-openharmony.binary-catalog`，`schemaVersion`: 1）。仓库内示例见 [`qt-oh-binary-catalog.v1.json`](qt-oh-binary-catalog.v1.json)。

- **入口**：设置 → Harmony → **Qt for Harmony Settings** → **Manage Qt for OpenHarmony SDK…**
- **发布页（参考）**：[GitCode `openharmony-sig/qt` Releases](https://gitcode.com/openharmony-sig/qt/releases)（清单中的 `downloadUrl` 可指向 Gitee/GitCode 等直链）。
- **列表数据来源**：插件内置 **主源 GitCode raw**、**备用 GitHub raw**（`ohosconstants.h` 中 `QtOhBinaryCatalogDefaultGitcodeUrl` / `QtOhBinaryCatalogDefaultGithubUrl`）。先请求 GitCode，失败或 JSON 无效时再请求 GitHub。环境变量 **`QT_OH_BINARY_CATALOG_URL`** 若非空则**仅覆盖主源 URL**（备用仍为内置 GitHub）。下载地址来自清单中各 `releases[].assets[].downloadUrl`。
- **行为**：树形勾选 Release / 资源文件 → 下载到「安装根」下的 `.temp/`，可选与 OH SDK 管理器相同逻辑的 **`tar` 解压**（`.zip` / `.tar` / `.tar.gz`）；**分卷 `.7z`** 需用户自行用 7-Zip 合并解压。列表**仅按宿主 OS** 过滤：只显示 `platform` 与当前机器（`linux` / `darwin` / `windows`）一致的条目；清单里的 **`arch` 表示 OpenHarmony 目标架构**（如真机 `aarch64` 与模拟器 `x86_64`），**不与**开发者本机 CPU 比对，故同一宿主 OS 下可能同时列出多种目标包。`kind: metadata` 源码包不显示；无 `platform` 的条目不显示。
- **解压目标**：`<安装根>/<tag>/`（tag 中的路径字符会替换为 `_`）。
- **完成后**：勾选「下载后解压」时，插件会在解压目录下递归查找 **`bin/qmake`**，找到则写入 **Qt for Harmony qmake list** 并调用 `HarmonyConfigurations::applyConfig()`（注册 Qt 版本 / Kit 等）；未找到或已存在同一路径时会在日志中说明，必要时仍可在设置里手动添加。

---

## 1. 参考实现：`ohos_sdk_downloader.py` 与 `build_qt` 实例化

**`OhosSdkDownloader` 构造参数来源**（与插件内 `HarmonySdkDownloader::defaultListRequest()` 对齐）：

| 文件 | 说明 |
|------|------|
| `build_qt/build_qt/config.py` | `Config.__init__`：`OhosSdkDownloader(url=self.ohos_sdk_list_url(), os_type=plat['osType'], os_arch=plat['osArch'], support_version=self.ohos_support_version())` |
| `build_qt/build_qt/config.py` | `ohos_sdk_list_url()`：`dependencies.ohos_sdk.url`（主 POST）与 `gc_url` / `gh_url`（备线 URL 前缀；`use_gh` 决定用哪个） |
| `build_qt/build_qt/utils.py` | `detect_platform()`：`windows` / `linux` / `darwin` + `x64` / `arm64` 等 |
| `build_qt/configure.json` | `dependencies.ohos_sdk`：`url`、`gc_url`、`gh_url`、`support_version` |

插件中默认值写在 `harmonysdkdownloader.cpp`（与当前 `configure.json` 一致）；若你更新了 `build_qt` 里的地址或 `support_version`，请同步改 C++ 或改为从设置读取。

### 1.1 协议摘要（`ohos_sdk_downloader.py`）

`build_qt/build_qt/` 中的脚本逻辑可概括为：

| 能力 | 行为 |
|------|------|
| **列表（主源）** | `POST primaryUrl`，JSON body：`osArch`、`osType`、`supportVersion`；响应为 **JSON 数组**。 |
| **列表（备源）** | `GET backupBase + '-' + osType`；响应同为 JSON 数组。 |
| **条目字段** | 每项含 `apiVersion`、`path`（组件名，如 `native` / `js` / `ets` / `toolchains`）、`archive`（`url`、`size`、`checksum`、`osArch`）。 |
| **合并策略** | 下载指定组件时：**主列表优先**，若无匹配再用备列表（与 `download_component_by_name` 一致）。 |
| **下载校验** | 按 `archive.checksum` 做 **SHA-256** 校验（脚本里以 `('sha256', checksum)` 形式传入）。 |

C++ 侧应对齐上述字段名与合并顺序，便于你继续用同一套服务端或镜像。

---

## 2. 与 Android 插件的模块对应

| Android | Harmony（规划） |
|---------|-----------------|
| `androidsdkmanager.cpp`（调 `sdkmanager`） | `harmonysdkdownloader.cpp`（`QNetworkAccessManager` + JSON） |
| `androidsdkmanagerdialog.cpp` | `harmonysdkmanagerdialog.cpp`（待做：树形/表格 + 安装目录） |
| `androidsdkdownloader.cpp`（首次向导等） | 设置页「下载 SDK」向导或独立菜单项（可与对话框合并） |
| `sdkmanageroutputparser.*` | **可选**：若未来有 CLI 日志，再加分流解析器；当前以 HTTP/文件进度为主 |

---

## 3. 建议架构（分阶段）

### 阶段 A — 网络与数据模型（当前已落库）

- **`HarmonySdkDownloader`**：`fetchPackageList(ListRequest)` → `packageListFetched` / `fetchFailed`  
- **`parsePackageListJson`**：静态解析，便于单测（后续 `WITH_TESTS`）。  
- **`ListRequest`** 需携带 **主 POST URL、备 GET URL**（完整 URL，备源等价于 Python 里拼好的 `url[1] + '-' + os_type`）。  
  - 官方端点可能变更或需合规说明：**不要写死密钥 URL**；默认可留空，由 **设置项** 或 **环境/供应商配置** 注入（下一小阶段）。

### 阶段 B — 设置与入口

- 在 `HarmonySettingsWidget` 或子对话框中配置：`supportVersion`、`osType`/`osArch`（可从 `QSysInfo` 映射）、主/备 URL。  
- 按钮：**「管理 OpenHarmony SDK 包…」** 打开包管理对话框（对标 Android「SDK Manager」）。

### 阶段 C — 包管理 UI（已实现初版）

- **设置 → Harmony**：**「HarmonyOS SDK location」**（对标 Android SDK location）：SDK Manager 将包下载到 **`<该路径>/.temp/`**，解压到 **`<该路径>/<API version>/`**。  
- **默认值**（与 Android 同类逻辑）：环境变量 **`OHOS_SDK_HOME`**，否则 **`OHOS_SDK_PATH`**；再否则 **macOS** `~/Library/OpenHarmony/sdk`，**Windows** `%LOCALAPPDATA%/OpenHarmony/Sdk`，**Linux** `~/OpenHarmony/sdk`。首次打开 Harmony 设置页且未保存过路径时，会写入上述默认；**SDK Manager** 在未保存时也会用 `HarmonyConfig::effectiveOhosSdkRoot()` 解析到同一默认。  
- **「Manage SDK Packages…」**：**按 API 版本分组的树**（父节点可勾选以全选该版本下组件）；子列含组件 / 大小 / URL。  
- **进度**：对话框内 `QProgressBar` + `QPlainTextEdit` 日志；**SHA-256** 与索引一致时校验。  
- **自动注册（已实现）**：`HarmonyConfig::registerDownloadedSdksUnder(effectiveOhosSdkRoot)` 只扫描 SDK 根下**一级子目录**（跳过 `.temp` 与隐藏目录名），对通过 `isValidSdk` 的路径自动 `addSdk`；若根目录本身是合法 SDK 也会注册。`isValidSdk` 要求存在可读的 `oh-uni-package.json`，查找顺序为：`{dir}/native`、`{dir}/default/openharmony/native`、`{dir}/openharmony/native`。若某版本目录（如 `20`）未装 **Native** 或解压不完整，会**不会**加入列表，看起来像「只扫到一个」。在 **Kits 加载后**、**打开 Harmony 设置校验时**、**SDK 根路径变更**、**SDK Manager 批量下载结束** 时触发；有新增则 `HarmonyConfigurations::applyConfig()`。仍可用「Add…」手动添加。  
- **未做**：自动解压 ZIP（非 tar）、与「已安装 SDK」列完整联动等。

### 阶段 D — 下载实现

- 大文件：优先 `QNetworkAccessManager` 写入临时文件，完成后 SHA-256 比对再解压。  
- **自动解压（已实现）**：在 **HarmonyOS SDK location** 下 **`/.temp/`** 存下载包；勾选「After each download, extract…」后对每个包执行 **`tar -xf … -C <SDK根>/<API version>/`**（macOS / Linux / Windows 10+）。同 API 多组件解压到**同一 API 子目录**以合并成一棵 SDK 树。  
- **完成后引导**：在解压结果中递归查找满足 `HarmonyConfig::isValidSdk` 的根目录；找到则提供 **「Add to SDK List」** / **「Open Harmony Settings」**；批量结束时会先执行上述**根目录扫描**，多数情况下 SDK 已自动进列表。否则提示手动处理并仍可打开设置页。  
- 失败重试、代理：待对标 `AndroidConfig`（`QNetworkProxy` 等）。

---

## 4. `osType` / `osArch` 与 Python 对齐

脚本使用字符串 `os_type`、`os_arch`，需与 **服务端约定** 一致。常见映射（以服务端为准）：

| 宿主 | `osType`（示例） | `osArch`（示例） |
|------|------------------|------------------|
| Windows | `windows` | `x64` |
| macOS | `darwin` 或 `mac`（**以后台 API 为准**） | `x64` / `arm64` |
| Linux | `linux` | `x64` |

建议在 `HarmonySdkDownloader::ListRequest` 中由调用方传入，并提供 `ListRequest::fromHost()` 辅助函数（后续可加）。

---

## 5. 合规与风险

- **下载 URL 与条款**：使用华为/OpenHarmony 官方或企业镜像时，在 UI 或文档中提示用户遵守相应许可与网络策略。  
- **P2-03 / P2-04**（[PRIORITY-PLAN.md](PRIORITY-PLAN.md)）：此前标注「可能与 sdkmanager 不对等」——采用 **HTTP 列表模型** 后，**可实现**，但需产品确认默认端点与离线/内网方案。

---

## 6. 相关源码路径

| 路径 | 说明 |
|------|------|
| `src/harmonysdkdownloader.h` / `.cpp` | 列表拉取与 JSON 解析 |
| `src/harmonysettingswidget.*` | 后续：入口按钮与 URL 配置 |
| `src/harmonyconfigurations.*` | 后续：持久化 SDK 根路径与 API 参数 |

文档索引见 [README.md](README.md)。
