cmake_minimum_required(VERSION 3.16)

project(libusb LANGUAGES C)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(libusb STATIC
  core.c
  descriptor.c
  hotplug.c
  io.c
  strerror.c
  sync.c
  os/events_windows.c os/threads_windows.c os/windows_common.c os/windows_usbdk.c os/windows_winusb.c
)

target_compile_definitions(libusb PRIVATE LIBUSB_LIBRARY)
target_include_directories(libusb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
