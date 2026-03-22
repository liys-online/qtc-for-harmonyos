cmake_minimum_required(VERSION 3.16)

project(libusb LANGUAGES C)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(LIBUSB_COMMON_SOURCES
  core.c
  descriptor.c
  hotplug.c
  io.c
  strerror.c
  sync.c
)

if(WIN32)
  set(LIBUSB_PLATFORM_SOURCES
    os/events_windows.c
    os/threads_windows.c
    os/windows_common.c
    os/windows_usbdk.c
    os/windows_winusb.c
  )
elseif(APPLE)
  set(LIBUSB_PLATFORM_SOURCES
    os/events_posix.c
    os/threads_posix.c
    os/darwin_usb.c
  )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(LIBUSB_PLATFORM_SOURCES
    os/events_posix.c
    os/threads_posix.c
    os/linux_usbfs.c
    os/linux_netlink.c
  )
else()
  message(FATAL_ERROR "Bundled libusb: unsupported system \"${CMAKE_SYSTEM_NAME}\"")
endif()

add_library(libusb STATIC
  ${LIBUSB_COMMON_SOURCES}
  ${LIBUSB_PLATFORM_SOURCES}
)

target_compile_definitions(libusb PRIVATE LIBUSB_LIBRARY)
target_include_directories(libusb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(APPLE)
  find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
  find_library(IOKIT_FRAMEWORK IOKit REQUIRED)
  find_library(SECURITY_FRAMEWORK Security REQUIRED)
  find_library(OBJC_LIBRARY objc REQUIRED)
  target_link_libraries(libusb PRIVATE
    ${COREFOUNDATION_FRAMEWORK}
    ${IOKIT_FRAMEWORK}
    ${SECURITY_FRAMEWORK}
    ${OBJC_LIBRARY}
  )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  # linux_netlink.c: SCM_CREDENTIALS / struct ucred（glibc 需在含头文件前启用 GNU 扩展）
  target_compile_definitions(libusb PRIVATE _GNU_SOURCE)
  target_link_libraries(libusb PRIVATE pthread)
endif()
