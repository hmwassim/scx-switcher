cmake_minimum_required(VERSION 3.16)
project(debforge-scx LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)

set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/control_tab.cpp
    src/scx_utils.cpp
    src/priv_ops.cpp
)

set(HEADERS
    src/mainwindow.h
    src/control_tab.h
    src/scx_utils.h
    src/priv_ops.h
)

add_executable(debforge-scx ${SOURCES} ${HEADERS})

target_link_libraries(debforge-scx PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
)

install(TARGETS debforge-scx RUNTIME DESTINATION /usr/bin)

install(FILES data/debforge-scx.desktop DESTINATION /usr/share/applications)
install(FILES data/debforge-scx.metainfo.xml DESTINATION /usr/share/metainfo)
install(FILES data/debforge-scx.svg DESTINATION /usr/share/icons/hicolor/scalable/apps)
install(FILES policies/com.debforge.scx.policy DESTINATION /usr/share/polkit-1/actions)
