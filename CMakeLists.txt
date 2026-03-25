cmake_minimum_required(VERSION 3.16)

project(qtc-for-harmonyos)

# 通过 target_link_libraries(Harmony … usbmonitor) 链接即可，勿使用错误的 link_directories
#（在 qt-creator 主工程内构建时并不存在 libs/usbmonitor 目录，会导致 ld 警告）。
include_directories(${CMAKE_BINARY_DIR}/libs/libusb)

if (MSVC)
    add_compile_options(/utf-8)
endif()

# add the subdirectories
add_subdirectory(lib)
add_subdirectory(src)
add_subdirectory(test)
