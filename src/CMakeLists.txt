cmake_minimum_required(VERSION 3.20)

# Remove when sharing with others.

# Qt Creator 路径：
# - 命令行 -DQtCreator=... 指向含 lib/cmake/QtCreator 的根（如 CI 解压的 qtcreator_dev）
# - 否则按「Qt 安装根/Tools/QtCreator」推断（与 Qt Maintenance Tool 布局一致）
if(DEFINED QtCreator)
  if(APPLE)
    set(QtCreatorConfig ${QtCreator}/QtCreator/Contents/Resources/lib/cmake/QtCreator/QtCreatorConfig.cmake)
  else()
    set(QtCreatorConfig ${QtCreator}/lib/cmake/QtCreator/QtCreatorConfig.cmake)
  endif()
    set(_HARMONY_QTC_FROM_CACHE TRUE)
else()
    set(_HARMONY_QTC_FROM_CACHE FALSE)
    set(QT_INSTALL_PATH ${CMAKE_PREFIX_PATH})
    get_filename_component(PARENT_DIR ${QT_INSTALL_PATH} DIRECTORY)
    get_filename_component(GRANDPARENT_DIR ${PARENT_DIR} DIRECTORY)
    set(QtCreatorConfig ${GRANDPARENT_DIR}/Tools/QtCreator/lib/cmake/QtCreator/QtCreatorConfig.cmake)
endif()

if(EXISTS ${QtCreatorConfig})
    if(NOT _HARMONY_QTC_FROM_CACHE)
        set(QtCreator ${GRANDPARENT_DIR}/Tools/QtCreator)
    endif()
    message(STATUS "QtCreator API found at ${QtCreator}")
else()
    message(WARNING "File ${QtCreatorConfig} does not exist.")
endif()

list(APPEND CMAKE_PREFIX_PATH ${QtCreator})

project(Harmony)

# Qt Creator 版本框架：最低 19+，可扩展 20+（见 ../VERSIONING.md）
include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/QtCreatorVersion.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/HarmonyQtcOptionalSources.cmake")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# find_package(QtCreator REQUIRED COMPONENTS Core)
find_package(Qt6 COMPONENTS Widgets Network REQUIRED)

# Add a CMake option that enables building your plugin with tests.
# You don't want your released plugin binaries to contain tests,
# so make that default to 'NO'.
# Enable tests by passing -DWITH_TESTS=ON to CMake.
option(WITH_TESTS "Builds with tests" NO)

if(WITH_TESTS)
  # Look for QtTest
  find_package(Qt6 REQUIRED COMPONENTS Test)

  # Tell CMake functions like add_qtc_plugin about the QtTest component.
  set(IMPLICIT_DEPENDS Qt::Test)

  # Enable ctest for auto tests.
  enable_testing()

  add_subdirectory(../test/qt_oh_binary_catalog_generator qt_oh_binary_catalog_generator)
endif()

find_package(Qt6 REQUIRED COMPONENTS LinguistTools)
qt_standard_project_setup(I18N_TRANSLATED_LANGUAGES zh_CN)
qt_add_translations(TARGETS Harmony)

add_qtc_plugin(Harmony
  PLUGIN_DEPENDS
    QtCreator::Core
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
    qrc.qrc
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
    harmonyutils.h harmonyutils.cpp
    harmonysdkdownloader.h harmonysdkdownloader.cpp
    harmonysdkmanagerdialog.h harmonysdkmanagerdialog.cpp
    harmonyqttreleasesdownloader.h harmonyqttreleasesdownloader.cpp
    harmonyqttsdkmanagerdialog.h harmonyqttsdkmanagerdialog.cpp
    harmonysdkarchiveutils.h harmonysdkarchiveutils.cpp
    harmonybuildhapstep.h harmonybuildhapstep.cpp
    harmonybuildconfiguration.h harmonybuildconfiguration.cpp
    harmonydeviceinfo.h harmonydeviceinfo.cpp
    harmonydeployqtstep.h harmonydeployqtstep.cpp
    harmonyrunconfiguration.h harmonyrunconfiguration.cpp
    harmonyrunner.h harmonyrunner.cpp
    ohosconstants.h
    ohostr.h
    harmonyqtcdefs.h
    ${HARMONY_QTC_EXTRA_SOURCES}
)

get_filename_component(PARENT_DIR ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)
target_include_directories(Harmony PRIVATE ${PARENT_DIR}/lib)

target_link_libraries(Harmony PRIVATE usbmonitor ohprojectecreator)

target_compile_definitions(Harmony PRIVATE
  QTC_IDE_VERSION_MAJOR=${HARMONY_IDE_VERSION_MAJOR}
  QTC_IDE_VERSION_MINOR=${HARMONY_IDE_VERSION_MINOR}
  QTC_IDE_VERSION_PATCH=${HARMONY_IDE_VERSION_PATCH}
  HARMONY_QTC_USE_QTTASKTREE=1
)
