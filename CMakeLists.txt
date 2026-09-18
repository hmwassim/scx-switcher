cmake_minimum_required(VERSION 3.16)
project(scx-switcher VERSION 1.1.5 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

include(GNUInstallDirs)

# The privileged helper must live alongside the app and the PolKit policy must
# reference the exact same path, so derive it from the configured prefix.
# "libexec" is kept literal (not CMAKE_INSTALL_LIBEXECDIR, which resolves to
# "lib" on Debian) to avoid relocating the helper on existing installs.
set(SCX_LIBEXECDIR "${CMAKE_INSTALL_PREFIX}/libexec/scx-switcher")
set(SCX_CTL_FULL "${SCX_LIBEXECDIR}/scx-switcher-ctl")

configure_file(
    policies/io.github.hmwassim.scx_switcher.policy.in
    ${CMAKE_CURRENT_BINARY_DIR}/io.github.hmwassim.scx_switcher.policy
    @ONLY
)

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
    src/distro.h       src/distro.cpp
    src/processrunner.h src/processrunner.cpp
    src/schedcatalog.h src/schedcatalog.cpp
    src/appcontroller.h src/appcontroller.cpp
    src/schedulercontroller.h src/schedulercontroller.cpp
)

target_link_libraries(scx-switcher PRIVATE Qt6::Core Qt6::Widgets Qt6::Network)

target_compile_definitions(scx-switcher PRIVATE
    APP_VERSION_STR="${PROJECT_VERSION}"
    SCX_CTL_PATH="${SCX_CTL_FULL}"
)

target_compile_options(scx-switcher PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

install(TARGETS scx-switcher DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/scx-switcher.desktop         DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES data/scx-switcher.metainfo.xml    DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
install(FILES data/scx-switcher.svg             DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
install(FILES data/scx_loader-kernel-check.conf DESTINATION ${CMAKE_INSTALL_DATADIR}/scx-switcher)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/io.github.hmwassim.scx_switcher.policy
    DESTINATION ${CMAKE_INSTALL_DATADIR}/polkit-1/actions)
install(PROGRAMS libexec/scx-switcher/scx-switcher-ctl DESTINATION ${SCX_LIBEXECDIR})
