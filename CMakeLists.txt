cmake_minimum_required(VERSION 3.16)

project(qtc-for-harmonyos)

link_directories(${CMAKE_BINARY_DIR}/libs/usbmonitor)
include_directories(${CMAKE_BINARY_DIR}/libs/libusb)

# add the subdirectories
add_subdirectory(lib)
add_subdirectory(src)
add_subdirectory(test)
