cmake_minimum_required(VERSION 3.16)

project(hdcsocketclient LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Network)

add_library(hdcsocketclient STATIC
  hdcsocketclient.cpp
  hdcsocketclient.h
  hdcsocketprotocol.cpp
  hdcsocketprotocol.h
)

target_include_directories(hdcsocketclient PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(hdcsocketclient PUBLIC Qt6::Core Qt6::Network)

target_compile_definitions(hdcsocketclient PRIVATE HDCSOCKETCLIENT_LIBRARY)
