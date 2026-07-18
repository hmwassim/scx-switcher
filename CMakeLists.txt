cmake_minimum_required(VERSION 3.16)
project(scx-switcher VERSION 1.1.4 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Network is required for QLocalServer/QLocalSocket (single-instance guard)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)

# Validate required resources exist
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/data/scx-switcher.svg")
    message(FATAL_ERROR "Icon file missing: data/scx-switcher.svg")
endif()

add_executable(scx-switcher
    src/main.cpp
    src/mainwindow.h   src/mainwindow.cpp
    src/controltab.h   src/controltab.cpp
    src/scxutils.h     src/scxutils.cpp
    src/privops.h      src/privops.cpp
    src/processrunner.h src/processrunner.cpp
    src/schedcatalog.h src/schedcatalog.cpp
    src/appcontroller.h src/appcontroller.cpp
    src/schedulercontroller.h src/schedulercontroller.cpp
    src/config.h
)

target_link_libraries(scx-switcher PRIVATE Qt6::Core Qt6::Widgets Qt6::Network)

target_compile_definitions(scx-switcher PRIVATE
    APP_VERSION_STR="${PROJECT_VERSION}"
)

target_compile_options(scx-switcher PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

install(TARGETS scx-switcher DESTINATION bin)
install(FILES data/scx-switcher.desktop         DESTINATION share/applications)
install(FILES data/scx-switcher.metainfo.xml    DESTINATION share/metainfo)
install(FILES data/scx-switcher.svg             DESTINATION share/icons/hicolor/scalable/apps)
install(FILES data/scx_loader-kernel-check.conf DESTINATION share/scx-switcher)
install(FILES policies/io.github.hmwassim.scx_switcher.policy  DESTINATION share/polkit-1/actions)
install(PROGRAMS libexec/scx-switcher/write-config     DESTINATION libexec/scx-switcher)
install(PROGRAMS libexec/scx-switcher/toggle-autostart DESTINATION libexec/scx-switcher)
