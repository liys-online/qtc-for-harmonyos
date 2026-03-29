cmake_minimum_required(VERSION 3.16)

project(arktsdebugbridge LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Core WebSockets)

add_library(arktsdebugbridge STATIC
    arktsdebugbridge.cpp
    arktsdebugbridge.h
)

target_include_directories(arktsdebugbridge PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(arktsdebugbridge PUBLIC Qt6::Core Qt6::WebSockets)
