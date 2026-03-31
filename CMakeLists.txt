cmake_minimum_required(VERSION 3.16)

project(qtc-for-harmonyos)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if (MSVC)
    add_compile_options(/utf-8)
endif()

# add the subdirectories
add_subdirectory(lib)
add_subdirectory(src)
add_subdirectory(test)
add_subdirectory(tools)
