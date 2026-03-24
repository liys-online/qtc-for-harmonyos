# 构建可复现性记录（Release 用）

> **用途**：满足 [RELEASE-CHECKLIST.md](RELEASE-CHECKLIST.md) **§4**；每次对外发布前由**维护者填写**「发布记录」小节并随 Tag / Release 存档（可粘贴到 GitHub Release 正文附录）。

---

## 1. 标准 CMake 命令（插件独立构建）

在**空构建目录**中执行（将占位路径换成本机实际路径）：

```bash
cmake -S <path_to_plugin>/src -B build \
  -DCMAKE_PREFIX_PATH=<path_to_qtcreator_prefix> \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DIDE_VERSION=<matching_creator_semver>

cmake --build build --parallel
```

| 变量 | 说明 |
|------|------|
| `<path_to_qtcreator_prefix>` | 含 `lib/cmake/QtCreator/QtCreatorConfig.cmake` 的目录。macOS 常为 `Qt Creator.app/Contents/Resources` |
| `<matching_creator_semver>` | 与目标 Qt Creator **一致**的版本字符串，例如 `19.0.2`（见 [VERSIONING.md](../VERSIONING.md)） |

**可选（单测与附属工具）**：

```bash
cmake -S <path_to_plugin>/src -B build-tests \
  -DCMAKE_PREFIX_PATH=<path_to_qtcreator_prefix> \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DIDE_VERSION=<matching_creator_semver> \
  -DWITH_TESTS=ON

cmake --build build-tests --parallel
```

---

## 2. 插件二进制与宿主版本关系（P0）

- 插件 **必须与** 加载它的 Qt Creator **主版本**匹配构建（同一 MAJOR，见 [VERSIONING.md](../VERSIONING.md)）。
- `IDE_VERSION` 未设置时，CMake 可能回退默认并 **WARNING**；**发布前务必显式传入**与安装包一致的版本。

---

## 3. 产物路径（与根 [README.md](../README.md) 一致）

| 平台 | 典型路径（相对构建目录） |
|------|---------------------------|
| Windows / Linux | `lib/qtcreator/plugins/`（以实际 `add_qtc_plugin` 输出为准） |
| macOS | `Qt Creator.app/Contents/PlugIns/`（或等价 `.dylib` 所在 `PlugIns`） |

发布后建议在 Release 中写明：**本包针对 Qt Creator x.y 在 <OS> 上构建**。

---

## 4. 构建后复核 `Harmony.json`（清单 §3.5）

配置阶段会将 `Harmony.json.in` 处理为带依赖列表的 `Harmony.json`。发布前在构建树中打开生成的 `Harmony.json`，确认：

- `Version` / `CompatVersion` 与 Git Tag 一致；
- `License` / `Description` / `Url` / `DocumentationUrl` 正确；
- `Dependencies`（或等价字段）非空且合理。

示例（路径随构建目录变化）：

```bash
find build -name Harmony.json -not -path '*/CMakeFiles/*'
```

---

## 5. 发布记录模板（每次发版复制填写）

**版本**：`Harmony.json` Version = _______________  
**Git Tag**：_______________  
**Qt Creator**：_______________（精确到 x.y.z）  
**IDE_VERSION CMake 传参**：_______________  
**CMAKE_BUILD_TYPE**：_______________  
**构建主机 OS / 架构**：_______________  
**构建命令**：见上文 §1（可贴完整一行）  
**SHA256（可选）**：附件 `Harmony*.so` / `*.dylib` / `*.dll`：_______________  

**维护者签名 / 日期**：_______________  
