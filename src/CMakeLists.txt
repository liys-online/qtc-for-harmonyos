cmake_minimum_required(VERSION 3.20)

# Remove when sharing with others.

if(DEFINED QtCreator)
    set(QtCreatorConfig ${QtCreator}/lib/cmake/QtCreator/QtCreatorConfig.cmake)
else()
    set(QT_INSTALL_PATH ${CMAKE_PREFIX_PATH})
    get_filename_component(PARENT_DIR ${QT_INSTALL_PATH} DIRECTORY)
    get_filename_component(GRANDPARENT_DIR ${PARENT_DIR} DIRECTORY)
    set(QtCreatorConfig ${GRANDPARENT_DIR}/Tools/QtCreator/lib/cmake/QtCreator/QtCreatorConfig.cmake)
endif()

if(EXISTS ${QtCreatorConfig})
    set(QtCreator ${GRANDPARENT_DIR}/Tools/QtCreator)
    message(STATUS "QtCreator API found at ${GRANDPARENT_DIR}/Tools/QtCreator")
else()
    message(WARNING "File ${QtCreatorConfig} does not exist.")
endif()

list(APPEND CMAKE_PREFIX_PATH ${QtCreator})

project(Harmony)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(QtCreator REQUIRED COMPONENTS Core)
find_package(Qt6 COMPONENTS Widgets REQUIRED)

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
    QtCreator::ExtensionSystem
    QtCreator::Utils
  SOURCES
    qrc.qrc
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
    ohosconstants.h
    ohostr.h
)
get_filename_component(PARENT_DIR ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)
target_include_directories(Harmony PUBLIC ${PARENT_DIR}/lib)

target_link_libraries(Harmony PUBLIC usbmonitor)
