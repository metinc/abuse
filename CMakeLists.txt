cmake_minimum_required(VERSION 3.21)

project(abuse
    VERSION 1.0.5
    DESCRIPTION "Run and gun game"
    LANGUAGES C CXX
)

include(GNUInstallDirs)

# Options
option(HAVE_NETWORK "Enable networking support" ON)
option(ABUSE_BUILD_TOOLS "Build the abuse-tool asset extraction utility" ON)

# Dependencies used by the game
find_package(SDL3 3.4.0 REQUIRED)
find_package(SDL3_mixer 3.2.0 REQUIRED)

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

# Keep install destinations relative so CMake, CPack, and AppImage staging all
# consume the same install rules. ASSETDIR remains the compiled-in fallback.
if(WIN32)
    set(ASSETDIR "." CACHE PATH "Asset installation directory")
elseif(APPLE)
    set(ASSETDIR "abuse.app/Contents/Resources/data" CACHE PATH "Asset installation directory")
else()
    set(ASSETDIR "${CMAKE_INSTALL_DATADIR}/abuse" CACHE PATH "Asset installation directory")
endif()

if(IS_ABSOLUTE "${ASSETDIR}")
    set(ABUSE_DEFAULT_ASSETDIR "${ASSETDIR}")
else()
    set(ABUSE_DEFAULT_ASSETDIR "${CMAKE_INSTALL_PREFIX}/${ASSETDIR}")
endif()

add_compile_definitions(
    ASSETDIR="${ABUSE_DEFAULT_ASSETDIR}"
    PACKAGE_NAME="abuse"
    PACKAGE_VERSION="${PROJECT_VERSION}"
    HAVE_CONFIG_H
)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(src)
add_subdirectory(data)

# One canonical installation layout. Platform packagers and the AppImage
# staging target below consume these same rules.
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
    # Record the dependency set without adding libraries to regular system
    # installs. The AppImage-only component consumes it below.
    install(TARGETS abuse
        RUNTIME_DEPENDENCY_SET abuse_appimage_dependencies
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

    set(APPDIR "${CMAKE_BINARY_DIR}/AppDir")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/AppRun.in"
        "${CMAKE_CURRENT_BINARY_DIR}/AppRun"
        @ONLY
    )
    file(CHMOD "${CMAKE_CURRENT_BINARY_DIR}/AppRun" PERMISSIONS
        OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE)

    # SDL_mixer loads FluidSynth at runtime, so it is not visible in abuse's
    # normal link dependency graph. Add it explicitly when available, then let
    # CMake resolve its transitive runtime dependencies.
    find_library(FLUIDSYNTH_RUNTIME_LIBRARY NAMES fluidsynth)
    if(FLUIDSYNTH_RUNTIME_LIBRARY)
        add_library(abuse_fluidsynth_runtime SHARED IMPORTED)
        set_target_properties(abuse_fluidsynth_runtime PROPERTIES
            IMPORTED_LOCATION "${FLUIDSYNTH_RUNTIME_LIBRARY}")
        install(IMPORTED_RUNTIME_ARTIFACTS abuse_fluidsynth_runtime
            RUNTIME_DEPENDENCY_SET abuse_appimage_dependencies
            LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            COMPONENT AppImage
            EXCLUDE_FROM_ALL)
        # Preserve FluidSynth's SONAME symlink chain. Installing an imported
        # library alone only copies the path returned by find_library().
        install(CODE
            "file(COPY \"${FLUIDSYNTH_RUNTIME_LIBRARY}\" DESTINATION \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}\" FOLLOW_SYMLINK_CHAIN)"
            COMPONENT AppImage
            EXCLUDE_FROM_ALL)
    else()
        message(STATUS "FluidSynth was not found; AppImage MIDI playback will use another available SDL_mixer backend")
    endif()

    install(RUNTIME_DEPENDENCY_SET abuse_appimage_dependencies
        PRE_EXCLUDE_REGEXES
            "^ld-linux.*"
            "^libc\\.so.*"
            "^libdl\\.so.*"
            "^libm\\.so.*"
            "^libpthread\\.so.*"
            "^librt\\.so.*"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        COMPONENT AppImage
        EXCLUDE_FROM_ALL
    )

    add_custom_target(appdir
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${APPDIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${APPDIR}"
        COMMAND ${CMAKE_COMMAND} -E env "DESTDIR=${APPDIR}"
            ${CMAKE_COMMAND} --install "${CMAKE_BINARY_DIR}" --prefix /usr
        COMMAND ${CMAKE_COMMAND} -E env "DESTDIR=${APPDIR}"
            ${CMAKE_COMMAND} --install "${CMAKE_BINARY_DIR}" --prefix /usr --component AppImage
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_BINARY_DIR}/AppRun" "${APPDIR}/AppRun"
        COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/data/freedesktop/com.github.Xenoveritas.abuse.desktop"
            "${APPDIR}/com.github.Xenoveritas.abuse.desktop"
        COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/data/freedesktop/com.github.Xenoveritas.abuse.png"
            "${APPDIR}/com.github.Xenoveritas.abuse.png"
        COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/data/freedesktop/com.github.Xenoveritas.abuse.png"
            "${APPDIR}/.DirIcon"
        DEPENDS abuse
        COMMENT "Staging the canonical install tree in ${APPDIR}"
        VERBATIM
    )

    find_program(APPIMAGETOOL appimagetool)
    if(APPIMAGETOOL)
        string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" APPIMAGE_ARCH)
        if(APPIMAGE_ARCH MATCHES "^(amd64|x64|x86_64)$")
            set(APPIMAGE_ARCH x86_64)
        elseif(APPIMAGE_ARCH MATCHES "^(aarch64|arm64)$")
            set(APPIMAGE_ARCH aarch64)
        elseif(APPIMAGE_ARCH MATCHES "^(i[3-6]86|x86)$")
            set(APPIMAGE_ARCH i686)
        endif()

        add_custom_target(appimage
            COMMAND ${CMAKE_COMMAND} -E env "ARCH=${APPIMAGE_ARCH}"
                "${APPIMAGETOOL}" --no-appstream "${APPDIR}"
                "${CMAKE_BINARY_DIR}/Abuse-${PROJECT_VERSION}-${APPIMAGE_ARCH}.AppImage"
            DEPENDS appdir
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            COMMENT "Creating Abuse-${PROJECT_VERSION}-${APPIMAGE_ARCH}.AppImage"
            VERBATIM
        )
    else()
        message(STATUS "appimagetool was not found; the appdir target is available, but appimage is disabled")
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
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libsdl3-0, libsdl3-mixer0")
    set(CPACK_DEBIAN_PACKAGE_SECTION "games")
    set(CPACK_RPM_PACKAGE_LICENSE "GPL-2.0")
    set(CPACK_RPM_PACKAGE_GROUP "Amusements/Games")
    set(CPACK_RPM_PACKAGE_REQUIRES "SDL3, SDL3_mixer")
endif()

include(CPack)
