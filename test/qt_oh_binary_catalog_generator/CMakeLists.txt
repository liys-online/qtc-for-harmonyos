# Copyright (C) 2026 Li-Yaosong
# SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
cmake_minimum_required(VERSION 3.16)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Network)
qt_add_executable(qt-oh-binary-catalog-generator
  main.cpp
)

set_target_properties(qt-oh-binary-catalog-generator PROPERTIES
  WIN32_EXECUTABLE FALSE
  MACOSX_BUNDLE FALSE
)

target_link_libraries(qt-oh-binary-catalog-generator
  PRIVATE
    Qt::Core
    Qt::Network
)
