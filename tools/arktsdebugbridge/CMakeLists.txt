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

target_link_libraries(arktsdebugbridge_exe PRIVATE arktsdebugbridge)
