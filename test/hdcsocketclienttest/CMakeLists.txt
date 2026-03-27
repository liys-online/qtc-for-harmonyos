cmake_minimum_required(VERSION 3.16)

project(hdcsocketclienttest LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Network Test)

add_executable(hdcsocketclienttest
  hdcsocketclienttest.cpp
)

target_link_libraries(hdcsocketclienttest
  PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Test
    hdcsocketclient
)

include(GNUInstallDirs)
install(TARGETS hdcsocketclienttest
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

enable_testing()
add_test(NAME hdcsocketclienttest COMMAND hdcsocketclienttest)
