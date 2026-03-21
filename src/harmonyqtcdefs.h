#pragma once

/**
 * Qt Creator 版本宏（构建时由 CMake 注入 QTC_IDE_VERSION_{MAJOR,MINOR,PATCH}）。
 * 仅 IDE 浏览、未配置工程时下列默认值为占位。
 *
 * 策略：最低 **19+**；20 起若 API 变化，请使用 QTC_IDE_AT_LEAST_* 或 compat/ 条件源文件。
 */
#if !defined(QTC_IDE_VERSION_MAJOR)
#  define QTC_IDE_VERSION_MAJOR 19
#endif
#if !defined(QTC_IDE_VERSION_MINOR)
#  define QTC_IDE_VERSION_MINOR 0
#endif
#if !defined(QTC_IDE_VERSION_PATCH)
#  define QTC_IDE_VERSION_PATCH 0
#endif

/** 主版本比较：例如 QTC_IDE_AT_LEAST_MAJOR(20) */
#define QTC_IDE_AT_LEAST_MAJOR(maj) (QTC_IDE_VERSION_MAJOR >= (maj))

/**
 * 主 + 次版本：QTC_IDE_AT_LEAST(20, 0) 表示 >= 20.0
 */
#define QTC_IDE_AT_LEAST(maj, min)                                                                 \
    (QTC_IDE_VERSION_MAJOR > (maj)                                                                 \
     || (QTC_IDE_VERSION_MAJOR == (maj) && QTC_IDE_VERSION_MINOR >= (min)))

/** 19+ 基线均使用 QtTaskTree 路线（与旧 Tasking 相对）；若未来某版废弃再改 CMake */
#if !defined(HARMONY_QTC_USE_QTTASKTREE)
#  define HARMONY_QTC_USE_QTTASKTREE 1
#endif
