#!/usr/bin/env bash
# Copyright (C) 2026 Li-Yaosong
# SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
#
#
# 执行流程：
#   1. 使用 clang（macOS 原生，支持 --coverage）构建插件（-DWITH_TESTS=ON）
#   2. 运行 "Qt Creator -test Harmony" 收集覆盖率数据（.gcda/.gcno）
#   3. 用 gcovr 将覆盖率转为 SonarCloud 可读的 XML（--sonarqube）
#   4. 将 coverage.xml 和 compile_commands.json 复制到仓库根目录
#
# 前置依赖：
#   - Qt（macOS 版，通过 --qt-dir 指定，例如 ~/Qt/6.8.6/macos）
#   - Qt Creator（通过 --qtc-root 指定）
#   - CMake, Ninja：brew install cmake ninja
#   - gcovr：pip3 install gcovr
#
# 使用示例：
#   ./scripts/run-coverage.sh \
#       --qt-dir    ~/Qt/6.8.6/macos \
#       --qtc-root  ~/Qt/Tools/QtCreator

set -euo pipefail

# ── 默认值 ────────────────────────────────────────────────────────────────────

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build/coverage"
QT_DIR=""
QTC_ROOT=""

# ── 参数解析 ──────────────────────────────────────────────────────────────────

usage() {
    echo "用法：$0 --qt-dir <Qt macOS 路径> --qtc-root <Qt Creator 路径> [选项]"
    echo ""
    echo "必选："
    echo "  --qt-dir   <path>   Qt for macOS 前缀，例如 ~/Qt/6.8.6/macos"
    echo "  --qtc-root <path>   Qt Creator 安装根，例如 ~/Qt/Tools/QtCreator"
    echo ""
    echo "可选："
    echo "  --build-dir <path>  CMake 构建目录（默认：build/coverage）"
    echo "  --repo-root <path>  仓库根目录（默认：脚本所在目录的上一级）"
    exit 1
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --qt-dir)   QT_DIR="$(eval echo "$2")";    shift 2 ;;
        --qtc-root) QTC_ROOT="$(eval echo "$2")";  shift 2 ;;
        --build-dir) BUILD_DIR="$(eval echo "$2")"; shift 2 ;;
        --repo-root) REPO_ROOT="$(eval echo "$2")"; shift 2 ;;
        -h|--help)  usage ;;
        *) echo "未知参数：$1"; usage ;;
    esac
done

echo "== 仓库根目录 : ${REPO_ROOT}"
echo "== 构建目录   : ${BUILD_DIR}"

# ── 检查依赖 ──────────────────────────────────────────────────────────────────

require_cmd() {
    if ! command -v "$1" &>/dev/null; then
        echo "错误：缺少依赖 '$1'，请先安装。" >&2
        echo "  cmake/ninja：    brew install cmake ninja" >&2
        echo "  gcovr：          pip3 install gcovr" >&2
        exit 1
    fi
}

require_cmd cmake
require_cmd ninja
require_cmd gcovr

[[ -z "${QT_DIR}" ]]  && { echo "错误：请通过 --qt-dir 指定 Qt macOS 路径。"; usage; }
[[ -z "${QTC_ROOT}" ]] && { echo "错误：请通过 --qtc-root 指定 Qt Creator 路径。"; usage; }
[[ -d "${QT_DIR}" ]]  || { echo "错误：Qt 目录不存在：${QT_DIR}"; exit 1; }
[[ -d "${QTC_ROOT}" ]] || { echo "错误：Qt Creator 目录不存在：${QTC_ROOT}"; exit 1; }

# Qt Creator 可执行文件位置（安装程序版本在 .app bundle 内）
QTC_BIN=""
CANDIDATES=(
    "${QTC_ROOT}/Qt Creator.app/Contents/MacOS/Qt Creator"
    "${QTC_ROOT}/bin/qtcreator"
)
for c in "${CANDIDATES[@]}"; do
    if [[ -f "$c" ]]; then
        QTC_BIN="$c"
        break
    fi
done

if [[ -z "${QTC_BIN}" ]]; then
    echo "错误：未找到 Qt Creator 可执行文件，尝试了以下路径：" >&2
    printf '  %s\n' "${CANDIDATES[@]}" >&2
    exit 1
fi
echo "== Qt Creator     : ${QTC_BIN}"

# ── Step 1：CMake 配置（clang --coverage + 导出编译数据库）────────────────────

echo ""
echo "[1/3] CMake 配置（-DWITH_TESTS=ON --coverage）"
cmake -S "${REPO_ROOT}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_PREFIX_PATH="${QT_DIR}" \
    -DQtCreator="${QTC_ROOT}" \
    -DWITH_TESTS=ON \
    -DCMAKE_CXX_FLAGS="--coverage -fprofile-arcs -ftest-coverage" \
    -DCMAKE_C_FLAGS="--coverage -fprofile-arcs -ftest-coverage" \
    -DCMAKE_EXE_LINKER_FLAGS="--coverage" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -G Ninja

# ── Step 2：构建 ─────────────────────────────────────────────────────────────

echo ""
echo "[2/3] 构建插件"
cmake --build "${BUILD_DIR}" --parallel

# ── Step 3：运行测试（收集 .gcda 数据）───────────────────────────────────────

echo ""
echo "[3/3] 运行测试（Qt Creator -test Harmony）"
PLUGIN_PATH="${BUILD_DIR}/lib/qtcreator/plugins"
export QT_QPA_PLATFORM=offscreen

# 测试失败不阻断覆盖率收集
"${QTC_BIN}" \
    -pluginpath "${PLUGIN_PATH}" \
    -test Harmony \
    || echo "测试退出码：$?（非零不中断覆盖率收集）"

# ── Step 4：gcovr → coverage.xml ────────────────────────────────────────────

echo ""
echo "[4/4] 生成 coverage.xml（gcovr sonarqube 格式）"
COVERAGE_OUT="${REPO_ROOT}/coverage.xml"

gcovr \
    --root "${REPO_ROOT}" \
    --filter "${REPO_ROOT}/src/" \
    --filter "${REPO_ROOT}/lib/" \
    --exclude ".*_test\.cpp" \
    --exclude ".*/3rdparty/.*" \
    --exclude ".*/moc_.*" \
    --sonarqube \
    --output "${COVERAGE_OUT}"

echo "覆盖率报告已写入：${COVERAGE_OUT}"

# ── 复制 compile_commands.json 到仓库根目录 ──────────────────────────────────

SRC_CC="${BUILD_DIR}/compile_commands.json"
DST_CC="${REPO_ROOT}/compile_commands.json"

if [[ -f "${SRC_CC}" ]]; then
    cp -f "${SRC_CC}" "${DST_CC}"
    echo "compile_commands.json 已复制到：${DST_CC}"
else
    echo "警告：compile_commands.json 未找到（${SRC_CC}），SonarCloud cfamily 分析将降级。" >&2
fi

# ── 提示 git 操作 ─────────────────────────────────────────────────────────────

echo ""
echo "== 完成！请将以下文件 commit 并 push："
echo "   git add coverage.xml compile_commands.json"
echo "   git commit -m 'chore: update sonar coverage artifacts'"
echo "   git push"
echo ""
echo "CI 流水线将在 push 后自动上传到 SonarCloud。"
