# Harmony 插件 — 测试与工具

## `qt-oh-binary-catalog-generator`

根据 [GitCode OpenAPI](https://docs.gitcode.com/docs/apis/) 拉取仓库 Releases，生成符合 [**QT-OH-BINARY-CATALOG.md**](../docs/QT-OH-BINARY-CATALOG.md) 的 **`qt-oh-binary-catalog.v1.json`**。

### 构建

在配置 Harmony 插件时打开 **`WITH_TESTS`**（与插件共用同一 CMake 工程）：

```bash
cmake -S src/plugins/harmonyos/src -B build-harmony -DWITH_TESTS=ON
cmake --build build-harmony --target qt-oh-binary-catalog-generator
```

可执行文件位于构建目录下（名称：`qt-oh-binary-catalog-generator`）。

### 用法

```bash
export GITCODE_PRIVATE_TOKEN="你的令牌"
./qt-oh-binary-catalog-generator \
  -o /path/to/qt-oh-binary-catalog.v1.json
```

| 选项 | 说明 |
|------|------|
| `-o`, `--output <file>` | 输出文件；省略则打印到 **stdout** |
| `-t`, `--token <token>` | 令牌（不推荐写进 shell 历史；优先用环境变量） |
| `--repo <owner/name>` | 默认 `openharmony-sig/qt` |
| `--min-alpha <n>` | 只包含 `Alpha_v{n}` 及更高版本 tag，默认 **7** |
| `-h`, `--help` | 帮助 |

### 与仓库内 JSON

更新 **`docs/qt-oh-binary-catalog.v1.json`** 时，可将 `-o` 指向该路径（提交前自行 diff 确认）。
