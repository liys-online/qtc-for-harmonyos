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

# 推导 -DQtCreator 的 CMake 路径：
#   - .app bundle → 传 .app 根（CMakeLists 会补 Contents/Resources/）
#   - 普通安装  → 传二进制上两级目录
if [[ "${QTC_BIN}" == *".app/Contents/MacOS/"* ]]; then
    _tmp="${QTC_BIN%.app/Contents/MacOS/*}"
    QTC_CMAKE_PREFIX="${_tmp}.app"
else
    QTC_CMAKE_PREFIX="$(dirname "$(dirname "${QTC_BIN}")")"
fi
echo "== QtCreator CMake: ${QTC_CMAKE_PREFIX}"

# ── Step 1：CMake 配置（clang --coverage + 导出编译数据库）────────────────────

echo ""
echo "[1/3] CMake 配置（-DWITH_TESTS=ON --coverage）"
cmake -S "${REPO_ROOT}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_PREFIX_PATH="${QT_DIR}" \
    -DQtCreator="${QTC_CMAKE_PREFIX}" \
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
echo "[3/3] 运行测试"

export QT_QPA_PLATFORM=offscreen

# 3a：独立单元测试可执行文件（直接在进程内写 .gcda，最可靠）
echo "  [3a] 独立测试可执行文件"
_STANDALONE_TESTS=(
    "${BUILD_DIR}/test/usbmonitortest/usbmonitortest"
    "${BUILD_DIR}/test/hdcsocketclienttest/hdcsocketclienttest"
    "${BUILD_DIR}/test/ohprocreatetest/ohprocreatetest"
    "${BUILD_DIR}/test/harmonypluginlogictest/harmonypluginlogictest"
)
for _t in "${_STANDALONE_TESTS[@]}"; do
    if [[ -x "${_t}" ]]; then
        echo "    运行：${_t}"
        # harmonypluginlogictest 链接了 Qt Creator 的 Utils/ProjectExplorer（内置 Qt 6.10.2）。
        # 用 DYLD_FRAMEWORK_PATH 让 Qt Creator 的 Qt 优先，避免与构建用 Qt 的 ABI 冲突。
        if [[ "${_t}" == *harmonypluginlogictest* ]]; then
            DYLD_FRAMEWORK_PATH="${QTC_BIN%/Contents/MacOS/*}/Contents/Frameworks" \
                "${_t}" || echo "    [警告] 退出码 $?（不阻断覆盖率收集）"
        else
            "${_t}" || echo "    [警告] 退出码 $?（不阻断覆盖率收集）"
        fi
    else
        echo "    [跳过] 未找到：${_t}"
    fi
done

# 3b：Qt Creator 插件测试（覆盖插件主逻辑；退出不正常则 .gcda 可能丢失）
echo "  [3b] Qt Creator 插件测试（-test Harmony）"
# 插件输出路径：macOS 构建产物在 .app bundle 内
PLUGIN_PATH="${BUILD_DIR}/Qt Creator.app/Contents/PlugIns/qtcreator"
if [[ ! -d "${PLUGIN_PATH}" ]]; then
    # 备用：非 bundle 布局
    PLUGIN_PATH="${BUILD_DIR}/lib/qtcreator/plugins"
fi
"${QTC_BIN}" \
    -pluginpath "${PLUGIN_PATH}" \
    -test Harmony \
    || echo "  [警告] Qt Creator 退出码：$?（不阻断覆盖率收集）"

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
    \
    `# ── 需要真实设备 / 物理硬件 ──────────────────────────────────────` \
    --exclude ".*/arktsdebugbridge\.cpp$" \
    --exclude ".*/harmonymainrunsockettask\.cpp$" \
    --exclude ".*/harmonydebugsupport\.cpp$" \
    --exclude ".*/harmonyrunner\.cpp$" \
    --exclude ".*/harmonydeployqtstep\.cpp$" \
    \
    `# ── 需要外部网络服务（GitCode / GitHub / SDK CDN）──────────────` \
    --exclude ".*/harmonysdkdownloader\.cpp$" \
    --exclude ".*/harmonyqttreleasesdownloader\.cpp$" \
    \
    `# ── 纯 UI 对话框，需人工交互──────────────────────────────────────` \
    --exclude ".*/harmonysdkmanagerdialog\.cpp$" \
    --exclude ".*/harmonyqttsdkmanagerdialog\.cpp$" \
    \
    --sonarqube "${COVERAGE_OUT}"

# gcovr --sonarqube 不支持 --xml-pretty，用 python3 格式化并清理空 <file/> 元素
# （无 <lineToCover> 子元素的 header 文件在新版 SonarCloud scanner 中会导致解析错误）
python3 - "${COVERAGE_OUT}" <<'PYEOF'
import sys, xml.etree.ElementTree as ET

path = sys.argv[1]
ET.register_namespace('', '')
tree = ET.parse(path)
root = tree.getroot()

# 移除没有任何 <lineToCover> 子元素的 <file> 节点
to_remove = [child for child in root if child.tag == 'file' and len(child) == 0]
for child in to_remove:
    root.remove(child)

# 写回，手动补 XML 声明（ET 默认不加）
xml_str = ET.tostring(root, encoding='unicode', xml_declaration=False)

# 格式化缩进（Python 3.9+）
try:
    ET.indent(root, space='  ')
    xml_str = ET.tostring(root, encoding='unicode', xml_declaration=False)
except AttributeError:
    pass  # Python < 3.9，跳过格式化

with open(path, 'w', encoding='utf-8') as f:
    f.write('<?xml version="1.0" encoding="utf-8"?>\n')
    f.write(xml_str)
    f.write('\n')
PYEOF

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
