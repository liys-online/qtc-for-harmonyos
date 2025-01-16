cmake_minimum_required(VERSION 3.16)

project(usbmonitor LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

add_library(usbmonitor STATIC
  usbmonitor.cpp
  usbmonitor.h
)
get_filename_component(PARENT_DIR ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)
target_include_directories(usbmonitor PUBLIC ${PARENT_DIR}/3rdparty)
target_link_libraries(usbmonitor PUBLIC libusb)
target_link_libraries(usbmonitor PRIVATE Qt${QT_VERSION_MAJOR}::Core)

target_compile_definitions(usbmonitor PRIVATE USBMONITOR_LIBRARY)
