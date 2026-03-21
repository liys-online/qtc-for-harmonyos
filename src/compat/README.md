# 按 Qt Creator 大版本拆分的实现（可选）

当 **19 与 20（及以上）无法共用同一套实现** 时，用下面两种方式之一（可并存）。

## 1. 预处理器分岔（优先，改动小）

在共用 `.cpp` / `.h` 中：

```cpp
#include "harmonyqtcdefs.h"

#if QTC_IDE_AT_LEAST_MAJOR(20)
    // Qt Creator 20+ API
#else
    // 19.x 路径（当前基线）
#endif
```

跨次版本用 `QTC_IDE_AT_LEAST(20, 1)`（主.次）。

## 2. 条件编译单元（差异大时）

1. 在本目录新增 `xxx_qtc20.cpp`（仅 20+ 参与链接）。
2. 编辑 **`cmake/HarmonyQtcOptionalSources.cmake`**，在  
   `HARMONY_IDE_VERSION_MAJOR GREATER_EQUAL 20` 时  
   `list(APPEND HARMONY_QTC_EXTRA_SOURCES ...)`。
3. `src/CMakeLists.txt` 已把 `HARMONY_QTC_EXTRA_SOURCES` 并入 `add_qtc_plugin(SOURCES ...)`。

**不要**把同一符号定义在两个大版本文件里；共用声明放在头文件，用 `#if` 或 `inline` 包装。

## 宏从哪里来？

CMake 注入 `QTC_IDE_VERSION_MAJOR` / `MINOR` / `PATCH`，详见 `harmonyqtcdefs.h` 与仓库根下 `VERSIONING.md`。
