cmake_minimum_required(VERSION 3.16)

project(ohprocreatetest LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

add_executable(ohprocreatetest
  main.cpp
  ../../lib/ohprojectecreator/resource.qrc
)
target_link_libraries(ohprocreatetest Qt${QT_VERSION_MAJOR}::Core)

include(GNUInstallDirs)
install(TARGETS ohprocreatetest
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
get_filename_component(PARENT_DIR ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)
get_filename_component(PARENT_DIR ${PARENT_DIR} DIRECTORY)
target_include_directories(ohprocreatetest PUBLIC ${PARENT_DIR}/lib)

target_link_libraries(ohprocreatetest ohprojectecreator)
