# Copyright (C) 2026 Li-Yaosong
# SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

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
