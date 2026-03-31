# Remove when sharing with others.

# Qt Creator 安装前缀（目录下应有 lib/cmake/QtCreator/，且含 QtCreatorConfig.cmake）：
# - macOS .app：通常传 .../Qt Creator.app/Contents/Resources，或传 .app 根（下面会自动解析）
# - 其它：SDK / Tools 根目录
# - 未指定时：按 Qt 安装树 .../Tools/QtCreator 推断（与 Maintenance Tool 布局一致）

set(_HARMONY_QTC_FROM_CACHE FALSE)
if(DEFINED QtCreator)
  set(_HARMONY_QTC_FROM_CACHE TRUE)
  set(_qtc_user "${QtCreator}")
  if(APPLE AND EXISTS "${_qtc_user}/Contents/Resources/lib/cmake/QtCreator/QtCreatorConfig.cmake")
    set(QtCreator "${_qtc_user}/Contents/Resources")
  elseif(EXISTS "${_qtc_user}/lib/cmake/QtCreator/QtCreatorConfig.cmake")
    set(QtCreator "${_qtc_user}")
  else()
    set(QtCreator "${_qtc_user}")
  endif()
  set(QtCreatorConfig "${QtCreator}/lib/cmake/QtCreator/QtCreatorConfig.cmake")
else()
  set(QT_INSTALL_PATH "${CMAKE_PREFIX_PATH}")
  get_filename_component(PARENT_DIR "${QT_INSTALL_PATH}" DIRECTORY)
  get_filename_component(GRANDPARENT_DIR "${PARENT_DIR}" DIRECTORY)
  if(APPLE)
    set(QtCreator "${GRANDPARENT_DIR}/Qt Creator.app/Contents/Resources")
  else()
    set(QtCreator "${GRANDPARENT_DIR}/Tools/QtCreator")
  endif()
  set(QtCreatorConfig "${QtCreator}/lib/cmake/QtCreator/QtCreatorConfig.cmake")
endif()

if(EXISTS "${QtCreatorConfig}")
  message(STATUS "QtCreator API found at ${QtCreator}")
  list(APPEND CMAKE_PREFIX_PATH "${QtCreator}")
  find_package(QtCreator REQUIRED)
else()
  message(WARNING "File ${QtCreatorConfig} does not exist.")
endif()

# Qt Creator 版本框架：最低 19+，可扩展 20+（见 ../VERSIONING.md）
include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/QtCreatorVersion.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/HarmonyQtcOptionalSources.cmake")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network LinguistTools)

# Add a CMake option that enables building your plugin with tests.
# You don't want your released plugin binaries to contain tests,
# so make that default to 'NO'.
# Enable tests by passing -DWITH_TESTS=ON to CMake.
option(WITH_TESTS "Builds with tests" NO)

if(WITH_TESTS)
  enable_testing()
  add_subdirectory(../test/qt_oh_binary_catalog_generator qt_oh_binary_catalog_generator)
endif()

add_qtc_plugin(Harmony
  PLUGIN_DEPENDS
    QtCreator::Core
    QtCreator::Debugger
    QtCreator::ProjectExplorer
    QtCreator::QtSupport
    QtCreator::CMakeProjectManager
  DEPENDS
    Qt::Widgets
    Qt::Network
    QtCreator::ExtensionSystem
    QtCreator::Utils
  SOURCES
    Harmony.json.in
    ../lib/ohprojectecreator/resource.qrc
    ohos.cpp
    harmonydevice.cpp
    harmonydevice.h
    harmonysettingswidget.cpp
    harmonysettingswidget.h
    harmonyconfigurations.cpp
    harmonyconfigurations.h
    harmonytoolchain.cpp
    harmonytoolchain.h
    harmonyqtversion.cpp
    harmonyqtversion.h
    harmonydevicemanager.cpp
    harmonydevicemanager.h
    harmonyhdctargetsparser.cpp
    harmonyhdctargetsparser.h
    harmonyutils.h harmonyutils.cpp
    harmonylogcategories.h harmonylogcategories.cpp
    harmonysdkdownloader.h harmonysdkdownloader.cpp
    harmonysdkmanagerdialog.h harmonysdkmanagerdialog.cpp
    harmonyqttreleasesdownloader.h harmonyqttreleasesdownloader.cpp
    harmonyqttsdkmanagerdialog.h harmonyqttsdkmanagerdialog.cpp
    harmonysdkarchiveutils.h harmonysdkarchiveutils.cpp
    harmonybuildhapstep.h harmonybuildhapstep.cpp
    harmonyhvigoroutputparser.h harmonyhvigoroutputparser.cpp
    harmonybuildconfiguration.h harmonybuildconfiguration.cpp
    harmonydeviceinfo.h harmonydeviceinfo.cpp
    harmonydeployqtstep.h harmonydeployqtstep.cpp
    harmonyrunconfiguration.h harmonyrunconfiguration.cpp
    harmonymainrunsockettask.h harmonymainrunsockettask.cpp
    harmonyrunner.h harmonyrunner.cpp
    harmonydebugsupport.h harmonydebugsupport.cpp
    ohosconstants.h
    ohostr.h
    harmonyqtcdefs.h
    ${HARMONY_QTC_EXTRA_SOURCES}
)

if(WITH_TESTS)
  extend_qtc_plugin(Harmony
    DEFINES WITH_TESTS
    SOURCES
      ../test/harmonypluginlogictest/harmonyhdctargetsparser_test.h
      ../test/harmonypluginlogictest/harmonyhdctargetsparser_test.cpp
      ../test/harmonypluginlogictest/harmonydeviceinfo_test.h
      ../test/harmonypluginlogictest/harmonydeviceinfo_test.cpp
      ../test/harmonypluginlogictest/harmonyhvigoroutputparser_test.h
      ../test/harmonypluginlogictest/harmonyhvigoroutputparser_test.cpp
  )
  find_package(Qt6 REQUIRED COMPONENTS Test)
  target_link_libraries(Harmony PRIVATE Qt6::Test)
endif()

get_filename_component(PARENT_DIR ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)
target_include_directories(Harmony PRIVATE ${PARENT_DIR}/lib)

target_link_libraries(Harmony PRIVATE usbmonitor hdcsocketclient ohprojectecreator)

target_compile_definitions(Harmony PRIVATE
  QTC_IDE_VERSION_MAJOR=${HARMONY_IDE_VERSION_MAJOR}
  QTC_IDE_VERSION_MINOR=${HARMONY_IDE_VERSION_MINOR}
  QTC_IDE_VERSION_PATCH=${HARMONY_IDE_VERSION_PATCH}
    HARMONY_QTC_USE_QTTASKTREE=1
)

# 更新 .ts：构建 harmony_ts_zh_CN 或 harmony_ts_all（非默认 ALL，需手动选该目标）。
add_translation_targets(Harmony
  TARGETS Harmony
  TS_LANGUAGES zh_CN
  QM_LANGUAGES zh_CN
  INSTALL_DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/harmony_qm_install"
  OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
  TS_TARGET_PREFIX harmony_ts_
  QM_TARGET_PREFIX harmony_qm_
  ALL_QM_TARGET harmony_generate_qm_files
)

set(_harmony_zh_cn_qm "${CMAKE_CURRENT_BINARY_DIR}/Harmony_zh_CN.qm")
# Qt 6：绝对路径的资源文件必须设 QT_RESOURCE_ALIAS，否则与 ohos.cpp 中 :/Harmony_zh_CN.qm 一致。
set_source_files_properties("${_harmony_zh_cn_qm}" PROPERTIES
  GENERATED TRUE
  QT_RESOURCE_ALIAS "Harmony_zh_CN.qm")
qt_add_resources(Harmony harmony_i18n_qm
  PREFIX "/"
  FILES "${_harmony_zh_cn_qm}")
add_dependencies(Harmony harmony_generate_qm_files)

# 将 OHOS 定制化的 lldbbridge.py 部署到构建树的 debugger/ 目录，
# 在 LLDB attach 时由 Qt Creator LLDB 引擎加载，使其支持 remote-ohos 平台。
# 源文件位于插件的 scripts/ 目录，以便与 Qt Creator 上游 lldbbridge.py 区分。
# 使用 stamp 文件作为 OUTPUT 避免与上游规则声明相同 OUTPUT 产生的 ninja 冲突。
set(_harmony_lldbbridge_src "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/lldbbridge.py")
set(_harmony_lldbbridge_dst "${CMAKE_BINARY_DIR}/${IDE_DATA_PATH}/debugger/lldbbridge.py")
set(_harmony_lldbbridge_stamp "${CMAKE_CURRENT_BINARY_DIR}/HarmonyLldbBridge.stamp")
add_custom_command(
    OUTPUT  "${_harmony_lldbbridge_stamp}"
    COMMAND "${CMAKE_COMMAND}" -E copy "${_harmony_lldbbridge_src}" "${_harmony_lldbbridge_dst}"
    COMMAND "${CMAKE_COMMAND}" -E touch "${_harmony_lldbbridge_stamp}"
    DEPENDS "${_harmony_lldbbridge_src}"
    COMMENT "Deploying OHOS lldbbridge.py to debugger resources"
)
add_custom_target(HarmonyLldbBridge ALL DEPENDS "${_harmony_lldbbridge_stamp}")
# 确保在 Qt Creator 上游 copy 之后执行，避免被覆盖
if(TARGET copy_share_files_to_builddir)
    add_dependencies(HarmonyLldbBridge copy_share_files_to_builddir)
endif()
add_dependencies(Harmony HarmonyLldbBridge)
