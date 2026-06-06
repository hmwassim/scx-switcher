cmake_minimum_required(VERSION 3.16)
project(scx-switcher VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)

add_executable(scx-switcher
    src/main.cpp
    src/mainwindow.h src/mainwindow.cpp
    src/control_tab.h src/control_tab.cpp
    src/scx_utils.h src/scx_utils.cpp
    src/priv_ops.h src/priv_ops.cpp
)

target_link_libraries(scx-switcher PRIVATE Qt6::Core Qt6::Widgets Qt6::Network)

target_compile_options(scx-switcher PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

install(TARGETS scx-switcher DESTINATION bin)
install(FILES data/scx-switcher.desktop DESTINATION share/applications)
install(FILES data/scx-switcher.metainfo.xml DESTINATION share/metainfo)
install(FILES data/scx-switcher.svg DESTINATION share/icons/hicolor/scalable/apps)
install(FILES policies/com.scx-switcher.policy DESTINATION share/polkit-1/actions)
