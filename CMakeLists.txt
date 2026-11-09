cmake_minimum_required(VERSION 3.16)
project(scx-switcher VERSION 3.0.0 LANGUAGES CXX)

# The polkit policy pins the helper's absolute path, so default to /usr
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
   set(CMAKE_INSTALL_PREFIX /usr CACHE PATH "Install prefix" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

include(GNUInstallDirs)
find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets DBus)

set(APP_ID io.github.hmwassim.scx_switcher)
set(HELPER_DIR ${CMAKE_INSTALL_LIBEXECDIR}/scx-switcher)
set(HELPER_PATH ${CMAKE_INSTALL_FULL_LIBEXECDIR}/scx-switcher/scx-switcher-helper)
configure_file(data/${APP_ID}.policy.in ${APP_ID}.policy @ONLY)

add_executable(scx-switcher
   src/main.cpp
   src/app.cpp
   src/window.cpp
   src/monitor.cpp
   src/kernel.cpp
   src/system.cpp
   src/catalog.cpp
   src/style.cpp
   src/spinner.cpp
   src/activity.cpp
)
target_link_libraries(scx-switcher PRIVATE Qt6::Widgets Qt6::DBus)
target_compile_definitions(scx-switcher PRIVATE
   APP_VERSION="${PROJECT_VERSION}"
   SCX_HELPER_PATH="${HELPER_PATH}"
)
target_compile_options(scx-switcher PRIVATE -Wall -Wextra -Wpedantic)

install(TARGETS scx-switcher)
install(PROGRAMS data/scx-switcher-helper DESTINATION ${HELPER_DIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${APP_ID}.policy
        DESTINATION ${CMAKE_INSTALL_DATADIR}/polkit-1/actions)
install(FILES data/${APP_ID}.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES data/${APP_ID}.metainfo.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
install(FILES data/scx-switcher.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
