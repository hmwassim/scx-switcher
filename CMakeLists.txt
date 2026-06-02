cmake_minimum_required(VERSION 3.16)
project(debforge-scx VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)

add_executable(debforge-scx
    src/main.cpp
    src/mainwindow.h src/mainwindow.cpp
    src/control_tab.h src/control_tab.cpp
    src/scx_utils.h src/scx_utils.cpp
    src/priv_ops.h src/priv_ops.cpp
)

target_link_libraries(debforge-scx PRIVATE Qt6::Core Qt6::Widgets Qt6::Network)

target_compile_options(debforge-scx PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

install(TARGETS debforge-scx DESTINATION bin)
install(FILES data/debforge-scx.desktop DESTINATION share/applications)
install(FILES data/debforge-scx.metainfo.xml DESTINATION share/metainfo)
install(FILES data/debforge-scx.svg DESTINATION share/icons/hicolor/scalable/apps)
install(FILES policies/com.debforge.scx.policy DESTINATION share/polkit-1/actions)
