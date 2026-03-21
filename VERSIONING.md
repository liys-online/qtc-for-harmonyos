# Qt Creator 版本策略（19+ 基线，可扩展到 20+）

## 目标

- **最低**：Qt Creator **19.x**（`IDE_VERSION` 主版本 &lt; 19 时 configure **失败**）。
- **向前兼容**：20 及以后若 API 再变，在本框架内用 **宏分岔** 或 **按大版本追加 `.cpp`**，无需推翻工程结构。

## CMake 流程

1. `src/CMakeLists.txt` 依次包含：
   - `cmake/QtCreatorVersion.cmake` — 解析 `IDE_VERSION`，校验最低版本，设置 `HARMONY_IDE_VERSION_*`。
   - `cmake/HarmonyQtcOptionalSources.cmake` — 维护者可按大版本追加 `HARMONY_QTC_EXTRA_SOURCES`。
2. 可调缓存变量：`HARMONY_QTC_MIN_MAJOR`（默认 `19`），一般保持默认即可。

版本来源：`IDE_VERSION` 或环境变量 `QTC_HARMONY_IDE_VERSION`；未设置时假定 `19.0.0` 并 **WARNING**。

```bash
cmake -DCMAKE_PREFIX_PATH=/path/to/QtCreator -DIDE_VERSION=20.0.0 ...
```

## 编译期宏（目标 `Harmony`）

| 宏 | 含义 |
|----|------|
| `QTC_IDE_VERSION_MAJOR` / `MINOR` / `PATCH` | 来自当前配置的 Qt Creator 版本 |
| `HARMONY_QTC_USE_QTTASKTREE` | 当前恒为 `1`（19+ 基线）；若某大版本弃用可再在 CMake 中分岔 |

头文件 **`src/harmonyqtcdefs.h`** 提供：

- `QTC_IDE_AT_LEAST_MAJOR(20)`  
- `QTC_IDE_AT_LEAST(20, 1)`（主.次）

## 接到 Qt Creator 20 时怎么做？

1. **小改**：在现有 `.cpp` 里 `#include "harmonyqtcdefs.h"`，用 `#if QTC_IDE_AT_LEAST_MAJOR(20)` 包新/旧调用。
2. **大改**：在 `src/compat/` 增加 `foo_qtc20.cpp`，并在 **`cmake/HarmonyQtcOptionalSources.cmake`** 里按  
   `HARMONY_IDE_VERSION_MAJOR GREATER_EQUAL 20` 加入该文件。详见 **`src/compat/README.md`**。

## 运行

插件 **必须与加载它的 Qt Creator 大版本匹配**（同一主版本 SDK 编出）。多版本并存时，各用各自的 `-pluginpath`。
