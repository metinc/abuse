cmake_minimum_required(VERSION 3.21)

project(abuse
    VERSION 1.0.5
    DESCRIPTION "Run and gun game"
    LANGUAGES C CXX
)

include(GNUInstallDirs)

# Options
option(ABUSE_BUILD_TOOLS "Build the abuse-tool asset extraction utility" ON)

# Dependencies used by the game
find_package(SDL3 3.4.0 REQUIRED)
find_package(SDL3_mixer 3.2.0 REQUIRED)
find_package(SDL3_net 3.0.0 REQUIRED)

# OpenCV is only used by abuse-tool, not by the game.
if(ABUSE_BUILD_TOOLS)
    find_package(OpenCV REQUIRED COMPONENTS core imgcodecs)
endif()

# System checks
include(CheckIncludeFiles)

check_include_files(unistd.h HAVE_UNISTD_H)
check_include_files("sys/time.h" HAVE_SYS_TIME_H)

if(TCPIP_DEBUG)
    add_compile_definitions(TCPIP_DEBUG)
endif()

if(NO_CHECK)
    add_compile_definitions(NO_CHECK)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

# Keep install destinations relative so CMake, CPack, and linuxdeploy staging all
# consume the same install rules. ASSETDIR remains the compiled-in fallback.
if(WIN32)
    set(ASSETDIR "." CACHE PATH "Asset installation directory")
    set(ABUSE_ASSETDIR_FROM_BASE ".")
elseif(APPLE)
    set(ASSETDIR "abuse.app/Contents/Resources/data" CACHE PATH "Asset installation directory")
    # SDL_GetBasePath() returns Contents/Resources for an application bundle.
    set(ABUSE_ASSETDIR_FROM_BASE "data")
else()
    set(ASSETDIR "${CMAKE_INSTALL_DATADIR}/abuse" CACHE PATH "Asset installation directory")
endif()

if(IS_ABSOLUTE "${ASSETDIR}")
    set(ABUSE_DEFAULT_ASSETDIR "${ASSETDIR}")
else()
    set(ABUSE_DEFAULT_ASSETDIR "${CMAKE_INSTALL_PREFIX}/${ASSETDIR}")
endif()

if(NOT DEFINED ABUSE_ASSETDIR_FROM_BASE)
    file(RELATIVE_PATH ABUSE_ASSETDIR_FROM_BASE
        "${CMAKE_INSTALL_FULL_BINDIR}" "${ABUSE_DEFAULT_ASSETDIR}")
    file(TO_CMAKE_PATH "${ABUSE_ASSETDIR_FROM_BASE}" ABUSE_ASSETDIR_FROM_BASE)
endif()

# Keep an explicit last-resort path for running the uninstalled executable.
# Installed and packaged builds find their assets relative to SDL_GetBasePath()
# first, so this path is only used when running directly from the build tree.
file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/data" ABUSE_DEVELOPMENT_ASSETDIR)

add_compile_definitions(
    ASSETDIR="${ABUSE_DEFAULT_ASSETDIR}"
    ABUSE_ASSETDIR_FROM_BASE="${ABUSE_ASSETDIR_FROM_BASE}"
    ABUSE_DEVELOPMENT_ASSETDIR="${ABUSE_DEVELOPMENT_ASSETDIR}"
    PACKAGE_NAME="abuse"
    PACKAGE_VERSION="${PROJECT_VERSION}"
    HAVE_CONFIG_H
)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(src)
add_subdirectory(data)

# One canonical installation layout. Platform packagers and linuxdeploy consume
# these same rules.
if(WIN32)
    install(TARGETS abuse
        RUNTIME_DEPENDENCIES
            PRE_EXCLUDE_REGEXES "api-ms-.*" "ext-ms-.*"
            POST_EXCLUDE_REGEXES ".*[Ww]indows[/\\\\][Ss]ystem32.*"
        RUNTIME DESTINATION .
    )
elseif(APPLE)
    install(TARGETS abuse BUNDLE DESTINATION .)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/macOS/abuse.icns"
        DESTINATION "abuse.app/Contents/Resources")
else()
    install(TARGETS abuse
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/data/freedesktop/com.github.Xenoveritas.abuse.desktop"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
    install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/data/freedesktop/com.github.Xenoveritas.abuse.png"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/160x160/apps")
    install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/data/freedesktop/com.github.Xenoveritas.abuse.appdata.xml"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/metainfo")

    # SDL_mixer loads FluidSynth at runtime, so it is not visible in abuse's
    # normal link dependency graph. Pass it to linuxdeploy explicitly so that
    # it and its transitive runtime dependencies are bundled.
    find_library(FLUIDSYNTH_RUNTIME_LIBRARY NAMES fluidsynth)
    string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" APPIMAGE_ARCH)
    if(APPIMAGE_ARCH MATCHES "^(amd64|x64|x86_64)$")
        set(APPIMAGE_ARCH x86_64)
    elseif(APPIMAGE_ARCH MATCHES "^(aarch64|arm64)$")
        set(APPIMAGE_ARCH aarch64)
    elseif(APPIMAGE_ARCH MATCHES "^(i[3-6]86|x86)$")
        set(APPIMAGE_ARCH i686)
    endif()

    find_program(LINUXDEPLOY
        NAMES linuxdeploy "linuxdeploy-${APPIMAGE_ARCH}.AppImage"
        DOC "Path to linuxdeploy or its official AppImage")
    if(LINUXDEPLOY)
        set(APPDIR "${CMAKE_BINARY_DIR}/AppDir")
        set(LINUXDEPLOY_APPDIR_ARGS
            --appdir "${APPDIR}"
            --deploy-deps-only "${APPDIR}/usr/${CMAKE_INSTALL_BINDIR}/abuse")
        if(FLUIDSYNTH_RUNTIME_LIBRARY)
            list(APPEND LINUXDEPLOY_APPDIR_ARGS --library "${FLUIDSYNTH_RUNTIME_LIBRARY}")
        else()
            message(STATUS "FluidSynth was not found; AppImage MIDI playback will use another available SDL_mixer backend")
        endif()

        add_custom_target(appdir
            COMMAND ${CMAKE_COMMAND} -E remove_directory "${APPDIR}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${APPDIR}"
            COMMAND ${CMAKE_COMMAND} -E env "DESTDIR=${APPDIR}"
                ${CMAKE_COMMAND} --install "${CMAKE_BINARY_DIR}" --prefix /usr
            COMMAND ${CMAKE_COMMAND} -E env
                "APPIMAGE_EXTRACT_AND_RUN=1"
                "NO_STRIP=1"
                "${LINUXDEPLOY}" ${LINUXDEPLOY_APPDIR_ARGS}
            DEPENDS abuse
            COMMENT "Staging and deploying ${APPDIR} with linuxdeploy"
            VERBATIM
        )

        set(APPIMAGE_OUTPUT "${CMAKE_BINARY_DIR}/Abuse-${PROJECT_VERSION}-${APPIMAGE_ARCH}.AppImage")
        add_custom_target(appimage
            COMMAND ${CMAKE_COMMAND} -E env
                "LDAI_NO_APPSTREAM=1"
                "LDAI_OUTPUT=${APPIMAGE_OUTPUT}"
                "LINUXDEPLOY_OUTPUT_VERSION=${PROJECT_VERSION}"
                "APPIMAGE_EXTRACT_AND_RUN=1"
                "NO_STRIP=1"
                "${LINUXDEPLOY}" --appdir "${APPDIR}" --output appimage
            DEPENDS appdir
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            COMMENT "Creating Abuse-${PROJECT_VERSION}-${APPIMAGE_ARCH}.AppImage"
            VERBATIM
        )
    else()
        message(STATUS "linuxdeploy was not found; the appdir and appimage targets are disabled")
    endif()
endif()

# Packaging
set(CPACK_PACKAGE_NAME "Abuse")
set(CPACK_PACKAGE_VENDOR "Crack-Dot-Com")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Abuse game")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")
set(CPACK_PACKAGE_EXECUTABLES "abuse;Abuse")
set(CPACK_PACKAGE_INSTALL_DIRECTORY ${CPACK_PACKAGE_NAME})

if(WIN32)
    set(CPACK_GENERATOR "WIX;ZIP")
    set(CPACK_WIX_UPGRADE_GUID "3A89DEB1-B520-4DF5-B44D-96A0EBB27DC0")
    set(CPACK_WIX_LICENSE_RTF "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")
    set(CPACK_WIX_PRODUCT_ICON "${CMAKE_CURRENT_SOURCE_DIR}/doc/icon.ico")
    set(CPACK_WIX_PROGRAM_MENU_FOLDER "Abuse")
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop;TGZ")
    set(CPACK_PACKAGING_INSTALL_PREFIX .)
    set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
else()
    set(CPACK_GENERATOR "TGZ;DEB;RPM")
    set(CPACK_PACKAGE_CONTACT "Metin Çelik <metincelik88@gmail.com>")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libsdl3-0, libsdl3-mixer0, libsdl3-net0")
    set(CPACK_DEBIAN_PACKAGE_SECTION "games")
    set(CPACK_RPM_PACKAGE_LICENSE "GPL-2.0")
    set(CPACK_RPM_PACKAGE_GROUP "Amusements/Games")
    set(CPACK_RPM_PACKAGE_REQUIRES "SDL3, SDL3_mixer, SDL3_net")
endif()

include(CPack)
