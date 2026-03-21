# Harmony — 按 Qt Creator 大版本追加源文件（可选）
#
# 在 Qt 20 等与 19 行为不一致时，可在此 list(APPEND HARMONY_QTC_EXTRA_SOURCES ...)
# 详见 ../VERSIONING.md 与 ../src/compat/README.md
#
# 约定：仅把「大版本专有」实现放这里；19/20 共用逻辑仍放在 src/ 主文件，
#      用 harmonyqtcdefs.h 中的 QTC_IDE_AT_LEAST_* 做 #if 分岔即可。

set(HARMONY_QTC_EXTRA_SOURCES "")

# ---------------------------------------------------------------------------
# Qt Creator 20+ 示例（有独立 .cpp 时再取消注释）
# ---------------------------------------------------------------------------
# if(HARMONY_IDE_VERSION_MAJOR GREATER_EQUAL 20)
#   list(APPEND HARMONY_QTC_EXTRA_SOURCES
#     "${CMAKE_CURRENT_LIST_DIR}/../src/compat/harmony_example_qtc20.cpp")
# endif()
