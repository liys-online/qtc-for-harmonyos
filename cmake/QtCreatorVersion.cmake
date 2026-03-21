# Harmony — Qt Creator 版本探测（最低 19，向前兼容 20+）
#
# 输出变量（供 src/CMakeLists.txt 与其它 *.cmake 使用）：
#   HARMONY_IDE_VERSION          原始字符串（来自 IDE_VERSION 等）
#   HARMONY_IDE_VERSION_MAJOR    整数
#   HARMONY_IDE_VERSION_MINOR
#   HARMONY_IDE_VERSION_PATCH
#   HARMONY_QTC_MIN_MAJOR        最低主版本（默认 19）
#
# IDE_VERSION 来源：
# - 作为 qt-creator 子工程构建
# - find_package(QtCreator) 后的 branding
# - 手动 -DIDE_VERSION=20.0.0
# - 环境变量 QTC_HARMONY_IDE_VERSION

set(HARMONY_QTC_MIN_MAJOR "19" CACHE STRING
    "Harmony 插件要求的 Qt Creator 最低主版本（默认 19，一般勿改）")

set(_harmony_default_version "19.0.0")

if(DEFINED IDE_VERSION AND NOT IDE_VERSION STREQUAL "")
  set(HARMONY_IDE_VERSION "${IDE_VERSION}")
elseif(DEFINED ENV{QTC_HARMONY_IDE_VERSION} AND NOT "$ENV{QTC_HARMONY_IDE_VERSION}" STREQUAL "")
  set(HARMONY_IDE_VERSION "$ENV{QTC_HARMONY_IDE_VERSION}")
else()
  set(HARMONY_IDE_VERSION "${_harmony_default_version}")
  message(WARNING
    "Harmony 插件: 未检测到 IDE_VERSION，假定 ${HARMONY_IDE_VERSION}。"
    "请使用目标 Qt Creator 的构建目录或 SDK 作为 CMAKE_PREFIX_PATH，或传入 -DIDE_VERSION=x.y.z。")
endif()

# 解析 x.y.z（忽略后缀如 -rc1 前的数字部分）
set(HARMONY_IDE_VERSION_MAJOR "0")
set(HARMONY_IDE_VERSION_MINOR "0")
set(HARMONY_IDE_VERSION_PATCH "0")

if(HARMONY_IDE_VERSION MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)")
  set(HARMONY_IDE_VERSION_MAJOR "${CMAKE_MATCH_1}")
  set(HARMONY_IDE_VERSION_MINOR "${CMAKE_MATCH_2}")
  set(HARMONY_IDE_VERSION_PATCH "${CMAKE_MATCH_3}")
elseif(HARMONY_IDE_VERSION MATCHES "^([0-9]+)\\.([0-9]+)([^0-9].*)?$")
  set(HARMONY_IDE_VERSION_MAJOR "${CMAKE_MATCH_1}")
  set(HARMONY_IDE_VERSION_MINOR "${CMAKE_MATCH_2}")
elseif(HARMONY_IDE_VERSION MATCHES "^([0-9]+)([^0-9].*)?$")
  set(HARMONY_IDE_VERSION_MAJOR "${CMAKE_MATCH_1}")
endif()

# 最低版本：主版本 >= HARMONY_QTC_MIN_MAJOR（解析失败时 MAJOR=0 也会失败）
if(HARMONY_IDE_VERSION_MAJOR LESS "${HARMONY_QTC_MIN_MAJOR}")
  message(FATAL_ERROR
    "Harmony 插件：要求 Qt Creator 主版本 >= ${HARMONY_QTC_MIN_MAJOR}（当前解析为 ${HARMONY_IDE_VERSION_MAJOR}）。\n"
    "完整版本字符串: ${HARMONY_IDE_VERSION}\n"
    "请换用较新的 Qt Creator SDK/构建树，或参见 VERSIONING.md / src/compat/README.md。")
endif()

message(STATUS
  "Harmony plugin: Qt Creator ${HARMONY_IDE_VERSION} "
  "(parsed ${HARMONY_IDE_VERSION_MAJOR}.${HARMONY_IDE_VERSION_MINOR}.${HARMONY_IDE_VERSION_PATCH}), "
  "min major ${HARMONY_QTC_MIN_MAJOR}+")
