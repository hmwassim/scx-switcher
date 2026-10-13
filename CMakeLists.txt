cmake_minimum_required(VERSION 3.16)
project(scx-switcher VERSION 2.0.0 LANGUAGES CXX)

# The PolKit policy pins the helper's absolute path, so default to /usr.
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX /usr CACHE PATH "Install prefix" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

include(GNUInstallDirs)
find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Network)

set(APP_ID io.github.hmwassim.scx_switcher)
set(HELPER_DIR libexec/scx-switcher)
set(HELPER_PATH ${CMAKE_INSTALL_PREFIX}/${HELPER_DIR}/scx-switcher-helper)
configure_file(data/${APP_ID}.policy.in ${APP_ID}.policy @ONLY)

add_executable(scx-switcher
    src/main.cpp
    src/mainwindow.cpp
    src/scx.cpp
    src/catalog.cpp
    src/distro.cpp
    src/ui.cpp
    src/installedview.cpp
)
target_link_libraries(scx-switcher PRIVATE Qt6::Widgets Qt6::Network)
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
install(FILES data/scx_loader-kernel-check.conf
        DESTINATION lib/systemd/system/scx_loader.service.d
        RENAME scx-switcher.conf)

set(CPACK_GENERATOR DEB)
set(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
set(CPACK_PACKAGE_CONTACT "HM. Wassim <hmwassim95@gmail.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Qt6 GUI for switching sched_ext CPU schedulers")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SECTION admin)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "pkexec, scx-tools")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA ${CMAKE_SOURCE_DIR}/data/postinst ${CMAKE_SOURCE_DIR}/data/postrm)
set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION ON)
set(CPACK_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
include(CPack)
