cmake_minimum_required(VERSION 3.16)

project(arktsdebugbridge_exe LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Core WebSockets)

add_qtc_executable(arktsdebugbridge_exe
    SOURCES
    main.cpp
)

set_target_properties(arktsdebugbridge_exe PROPERTIES OUTPUT_NAME arktsdebugbridge)

target_link_libraries(arktsdebugbridge_exe
    PRIVATE
        Qt6::Core
        Qt6::WebSockets
        arktsdebugbridge
)

include(GNUInstallDirs)

if(DEFINED IDE_LIBEXEC_PATH)
    set(_HARMONY_LIBEXEC_DEST "${IDE_LIBEXEC_PATH}")
else()
    set(_HARMONY_LIBEXEC_DEST "${CMAKE_INSTALL_BINDIR}")
endif()

install(TARGETS arktsdebugbridge_exe
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${_HARMONY_LIBEXEC_DEST}
)
