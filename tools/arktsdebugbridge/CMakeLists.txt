cmake_minimum_required(VERSION 3.16)

project(arktsdebugbridge_exe LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Core WebSockets)
find_package(QtCreator REQUIRED)


add_qtc_executable(arktsdebugbridge_exe
    DEPENDS
        Qt::Core
        Qt::WebSockets
    SOURCES
    main.cpp
)

set_target_properties(arktsdebugbridge_exe PROPERTIES OUTPUT_NAME arktsdebugbridge)

if(APPLE)
    set_target_properties(arktsdebugbridge_exe PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Qt Creator.app/Contents/Resources/libexec"
    )
else()
    set_target_properties(arktsdebugbridge_exe PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
endif()
target_link_libraries(arktsdebugbridge_exe PRIVATE arktsdebugbridge)
