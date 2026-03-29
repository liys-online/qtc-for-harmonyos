cmake_minimum_required(VERSION 3.16)

project(ohos_lldb_debug_cli LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Network WebSockets)

add_executable(ohos_lldb_debug_cli
    main.cpp
)

target_link_libraries(ohos_lldb_debug_cli
    PRIVATE
        Qt6::Core
        Qt6::Network
        Qt6::WebSockets
        hdcsocketclient
        arktsdebugbridge
)

include(GNUInstallDirs)
install(TARGETS ohos_lldb_debug_cli
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
