cmake_minimum_required(VERSION 3.16)

# Remove when sharing with others.
list(APPEND CMAKE_PREFIX_PATH "D:/Qt/Tools/QtCreator")

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
    ohosconstants.h
    ohostr.h
)
