cmake_minimum_required(VERSION 3.16)

project(harmonypluginlogictest LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Test)

# ── Qt Creator SDK ────────────────────────────────────────────────────────────
# 作用域说明：src/CMakeLists.txt 中创建的 IMPORTED 目标仅对 src/ 及其子目录可见，
# 不会传播到同级目录 test/。因此在此单独调用 find_package(QtCreator)。
if(NOT TARGET QtCreator::Utils)
  set(_qtc_prefix "")
  if(DEFINED QtCreator)
    set(_qtc_prefix "${QtCreator}")
    # 接受 .app 根目录或 Resources 子目录（macOS）
    if(APPLE AND EXISTS "${_qtc_prefix}/Contents/Resources/lib/cmake/QtCreator/QtCreatorConfig.cmake")
      set(_qtc_prefix "${_qtc_prefix}/Contents/Resources")
    endif()
  endif()
  if(_qtc_prefix STREQUAL "" OR NOT EXISTS "${_qtc_prefix}/lib/cmake/QtCreator/QtCreatorConfig.cmake")
    # 回退：按 Qt 安装树推断（与 src/ 的回退逻辑一致）
    set(_qt_prefix "${CMAKE_PREFIX_PATH}")
    list(GET _qt_prefix 0 _qt_prefix_0)
    if(_qt_prefix_0)
      get_filename_component(_parent "${_qt_prefix_0}" DIRECTORY)
      get_filename_component(_grandparent "${_parent}" DIRECTORY)
      if(APPLE)
        set(_qtc_prefix "${_grandparent}/Qt Creator.app/Contents/Resources")
      else()
        set(_qtc_prefix "${_grandparent}/Tools/QtCreator")
      endif()
    endif()
  endif()
  if(EXISTS "${_qtc_prefix}/lib/cmake/QtCreator/QtCreatorConfig.cmake")
    list(APPEND CMAKE_PREFIX_PATH "${_qtc_prefix}")
    find_package(QtCreator QUIET)
  endif()
  unset(_qtc_prefix)
endif()

# 决定哪些 Qt Creator 目标可用
set(_QTC_AVAILABLE FALSE)
set(_QTC_LIBS "")
if(TARGET QtCreator::Utils AND TARGET QtCreator::ProjectExplorer)
    set(_QTC_AVAILABLE TRUE)
    set(_QTC_LIBS QtCreator::Utils QtCreator::ProjectExplorer)
endif()

# 被测源文件（纯逻辑，无额外 I/O 依赖）
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(SRC_DIR "${REPO_ROOT}/src")

add_executable(harmonypluginlogictest
    harmonypluginlogictest.cpp
    # 被测源文件：与插件共用同一份代码（通过 harmonypluginlogictest.cpp 中的 #include 引入）
    ${SRC_DIR}/harmonyhdctargetsparser.cpp
)

if(_QTC_AVAILABLE)
    target_sources(harmonypluginlogictest PRIVATE
        ${SRC_DIR}/harmonydeviceinfo.cpp
        ${SRC_DIR}/harmonyhvigoroutputparser.cpp
    )
endif()

target_include_directories(harmonypluginlogictest PRIVATE "${SRC_DIR}")

target_link_libraries(harmonypluginlogictest
  PRIVATE
    Qt6::Core
    Qt6::Test
    ${_QTC_LIBS}
)

# WITH_TESTS 让 *_test.cpp 中的 #ifdef WITH_TESTS 代码生效
# HAVE_QTCREATOR_LIBS 让 main() 决定是否运行 QTC-dependent 测试类
target_compile_definitions(harmonypluginlogictest PRIVATE WITH_TESTS)
if(_QTC_AVAILABLE)
    target_compile_definitions(harmonypluginlogictest PRIVATE HAVE_QTCREATOR_LIBS)
endif()

enable_testing()
add_test(NAME harmonypluginlogictest COMMAND harmonypluginlogictest)

