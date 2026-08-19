cmake_minimum_required(VERSION 3.21)

if(POLICY CMP0177)
    cmake_policy(SET CMP0177 NEW)
endif()

project(abuse
    VERSION 1.0.5
    DESCRIPTION "Dark 2D side-scrolling run-and-gun game"
    HOMEPAGE_URL "https://github.com/metinc/abuse"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)

# The existing platform-specific sources use WIN32, while MinGW guarantees
# _WIN32. Keep the project-wide platform check consistent for cross builds.
if(WIN32)
    add_compile_definitions(WIN32)
endif()

# Options
option(ABUSE_BUILD_TOOLS "Build the abuse-tool asset extraction utility" ON)
option(ABUSE_ALLOW_FOREIGN_PACKAGE_BUILD
    "Allow DEB/RPM creation outside their native distribution family" OFF)
option(ABUSE_BUNDLE_SDL_RUNTIME
    "Install SDL3, SDL3_mixer, and SDL3_net in a private package directory" OFF)
set(ABUSE_SDL3_LICENSE_FILE "" CACHE FILEPATH
    "SDL3 license file installed when ABUSE_BUNDLE_SDL_RUNTIME is enabled")
set(ABUSE_SDL3_MIXER_LICENSE_FILE "" CACHE FILEPATH
    "SDL3_mixer license file installed when ABUSE_BUNDLE_SDL_RUNTIME is enabled")
set(ABUSE_SDL3_NET_LICENSE_FILE "" CACHE FILEPATH
    "SDL3_net license file installed when ABUSE_BUNDLE_SDL_RUNTIME is enabled")
set(ABUSE_HMI2MID_EXECUTABLE "" CACHE FILEPATH
    "Host executable used to convert HMI music while cross-compiling")
set(ABUSE_WINDOWS_RUNTIME_DIR "" CACHE PATH
    "Directory containing additional Windows runtime DLLs")

# Distribution packages need a few native layout decisions before the install
# rules are declared. Debian policy reserves /usr/games for game executables;
# RPM-family distributions use the regular /usr/bin location.
set(ABUSE_HOST_DISTRIBUTION_IDS "")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND EXISTS "/etc/os-release")
    file(STRINGS "/etc/os-release" _abuse_os_release_ids
        REGEX "^(ID|ID_LIKE)=")
    foreach(_abuse_os_release_id IN LISTS _abuse_os_release_ids)
        string(REGEX REPLACE "^[^=]+=" "" _abuse_os_release_id
            "${_abuse_os_release_id}")
        string(REPLACE "\"" "" _abuse_os_release_id
            "${_abuse_os_release_id}")
        string(APPEND ABUSE_HOST_DISTRIBUTION_IDS
            " ${_abuse_os_release_id}")
    endforeach()
    string(STRIP "${ABUSE_HOST_DISTRIBUTION_IDS}"
        ABUSE_HOST_DISTRIBUTION_IDS)
endif()

if(ABUSE_HOST_DISTRIBUTION_IDS MATCHES "(^| )(debian|ubuntu)( |$)")
    set(_abuse_default_install_bindir "games")
else()
    set(_abuse_default_install_bindir "${CMAKE_INSTALL_BINDIR}")
endif()
set(ABUSE_INSTALL_BINDIR "${_abuse_default_install_bindir}" CACHE STRING
    "Directory below the install prefix for the game executable")

# Dependencies used by the game
find_package(SDL3 3.4.0 REQUIRED)
find_package(SDL3_mixer 3.2.0 REQUIRED)
find_package(SDL3_net 3.0.0 REQUIRED)

# A selected SoundFont is passed to SDL_mixer's FluidSynth decoder. Some Linux
# distributions build SDL_mixer with another MIDI backend instead; those
# builds remain usable, but custom SoundFonts are unavailable.
if(DEFINED SDLMIXER_MIDI_FLUIDSYNTH AND NOT SDLMIXER_MIDI_FLUIDSYNTH)
    message(STATUS
        "SDL3_mixer has no FluidSynth decoder; custom SoundFonts will be disabled")
endif()
if(WIN32 AND SDLMIXER_MIDI_FLUIDSYNTH AND NOT ABUSE_WINDOWS_RUNTIME_DIR)
    find_package(FluidSynth CONFIG REQUIRED)
endif()

find_package(toml11 4.4 CONFIG QUIET)
if(NOT TARGET toml11::toml11)
    include(FetchContent)
    FetchContent_Declare(
        toml11
        GIT_REPOSITORY https://github.com/ToruNiina/toml11.git
        GIT_TAG v4.4.0
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(toml11)
endif()

# System checks
include(CheckIncludeFiles)

check_include_files(unistd.h HAVE_UNISTD_H)

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
file(TO_CMAKE_PATH "${CMAKE_CURRENT_BINARY_DIR}/data" ABUSE_GENERATED_ASSETDIR)

add_compile_definitions(
    ASSETDIR="${ABUSE_DEFAULT_ASSETDIR}"
    ABUSE_ASSETDIR_FROM_BASE="${ABUSE_ASSETDIR_FROM_BASE}"
    ABUSE_DEVELOPMENT_ASSETDIR="${ABUSE_DEVELOPMENT_ASSETDIR}"
    ABUSE_GENERATED_ASSETDIR="${ABUSE_GENERATED_ASSETDIR}"
    PACKAGE_VERSION="${PROJECT_VERSION}"
    HAVE_CONFIG_H
)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(src)
add_subdirectory(data)

# Container-built distribution packages carry the pinned SDL runtime in a
# private directory. This keeps the native packages usable on distributions
# that do not yet ship all three SDL3 libraries while still allowing their
# package managers to derive the remaining system-library dependencies.
if(ABUSE_BUNDLE_SDL_RUNTIME)
    if(WIN32 OR APPLE)
        message(FATAL_ERROR
            "ABUSE_BUNDLE_SDL_RUNTIME is only supported for Linux packages")
    endif()

    set(ABUSE_PRIVATE_LIBDIR "${CMAKE_INSTALL_LIBDIR}/abuse")
    set_target_properties(abuse PROPERTIES
        INSTALL_RPATH "\$ORIGIN/../${ABUSE_PRIVATE_LIBDIR}")

    set(_abuse_bundled_sdl_targets "")
    foreach(_abuse_sdl_target IN ITEMS
            SDL3::SDL3 SDL3_mixer::SDL3_mixer SDL3_net::SDL3_net)
        get_target_property(_abuse_sdl_aliased_target
            "${_abuse_sdl_target}" ALIASED_TARGET)
        if(_abuse_sdl_aliased_target)
            list(APPEND _abuse_bundled_sdl_targets
                "${_abuse_sdl_aliased_target}")
        else()
            list(APPEND _abuse_bundled_sdl_targets "${_abuse_sdl_target}")
        endif()
    endforeach()
    install(IMPORTED_RUNTIME_ARTIFACTS ${_abuse_bundled_sdl_targets}
        LIBRARY DESTINATION "${ABUSE_PRIVATE_LIBDIR}")

    foreach(_abuse_sdl_license IN ITEMS
            ABUSE_SDL3_LICENSE_FILE
            ABUSE_SDL3_MIXER_LICENSE_FILE
            ABUSE_SDL3_NET_LICENSE_FILE)
        if(NOT EXISTS "${${_abuse_sdl_license}}")
            message(FATAL_ERROR
                "${_abuse_sdl_license} must name an existing license file "
                "when ABUSE_BUNDLE_SDL_RUNTIME is enabled")
        endif()
    endforeach()
    install(FILES "${ABUSE_SDL3_LICENSE_FILE}"
        DESTINATION "${CMAKE_INSTALL_DOCDIR}/third-party"
        RENAME SDL3.txt)
    install(FILES "${ABUSE_SDL3_MIXER_LICENSE_FILE}"
        DESTINATION "${CMAKE_INSTALL_DOCDIR}/third-party"
        RENAME SDL3_mixer.txt)
    install(FILES "${ABUSE_SDL3_NET_LICENSE_FILE}"
        DESTINATION "${CMAKE_INSTALL_DOCDIR}/third-party"
        RENAME SDL3_net.txt)
endif()

# One canonical installation layout. Platform packagers and linuxdeploy consume
# these same rules.
if(WIN32)
    if(CMAKE_CROSSCOMPILING)
        install(TARGETS abuse RUNTIME DESTINATION .)
        set(windows_runtime_dlls
            "$<TARGET_FILE:SDL3::SDL3>"
            "$<TARGET_FILE:SDL3_mixer::SDL3_mixer>"
            "$<TARGET_FILE:SDL3_net::SDL3_net>"
        )
        if(ABUSE_WINDOWS_RUNTIME_DIR)
            file(GLOB windows_dependency_dlls LIST_DIRECTORIES FALSE
                "${ABUSE_WINDOWS_RUNTIME_DIR}/*.dll")
            if(NOT windows_dependency_dlls)
                message(FATAL_ERROR
                    "No DLLs found in ABUSE_WINDOWS_RUNTIME_DIR: ${ABUSE_WINDOWS_RUNTIME_DIR}")
            endif()
            list(APPEND windows_runtime_dlls ${windows_dependency_dlls})
        elseif(TARGET FluidSynth::libfluidsynth)
            list(APPEND windows_runtime_dlls "$<TARGET_FILE:FluidSynth::libfluidsynth>")
        else()
            message(FATAL_ERROR "FluidSynth runtime target not found for the Windows package")
        endif()
        install(FILES ${windows_runtime_dlls} DESTINATION .)

        set(mingw_runtime_dirs "")
        foreach(root_path IN LISTS CMAKE_FIND_ROOT_PATH)
            list(APPEND mingw_runtime_dirs
                "${root_path}/bin"
                "${root_path}/sys-root/mingw/bin")
        endforeach()
        foreach(runtime_dll
                libgcc_s_seh-1.dll
                libstdc++-6.dll
                libwinpthread-1.dll)
            unset(runtime_dll_path)
            find_file(runtime_dll_path
                NAMES "${runtime_dll}"
                PATHS ${mingw_runtime_dirs}
                NO_DEFAULT_PATH
                NO_CACHE
            )
            if(runtime_dll_path)
                install(FILES "${runtime_dll_path}" DESTINATION .)
            else()
                message(FATAL_ERROR "MinGW runtime library not found: ${runtime_dll}")
            endif()
        endforeach()
    else()
        install(TARGETS abuse
            RUNTIME_DEPENDENCIES
                PRE_EXCLUDE_REGEXES "api-ms-.*" "ext-ms-.*"
                POST_EXCLUDE_REGEXES ".*[Ww]indows[/\\\\][Ss]ystem32.*"
            RUNTIME DESTINATION .
        )
    endif()
    install(FILES COPYING COPYING.toml11 DESTINATION .)
elseif(APPLE)
    install(TARGETS abuse BUNDLE DESTINATION .)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/macOS/abuse.icns"
        DESTINATION "abuse.app/Contents/Resources")
    install(FILES COPYING.toml11 DESTINATION "abuse.app/Contents/Resources")
else()
    install(TARGETS abuse
        RUNTIME DESTINATION "${ABUSE_INSTALL_BINDIR}"
    )

    install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/data/freedesktop/com.github.metinc.abuse.desktop"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
    install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/data/freedesktop/com.github.metinc.abuse.png"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/160x160/apps")
    install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/data/freedesktop/com.github.metinc.abuse-128.png"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/128x128/apps"
        RENAME "com.github.metinc.abuse.png")
    install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/data/freedesktop/com.github.metinc.abuse.appdata.xml"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/metainfo")
    find_program(GZIP_EXECUTABLE gzip REQUIRED)
    set(ABUSE_GENERATED_DOCDIR "${CMAKE_CURRENT_BINARY_DIR}/packaging/doc")
    file(MAKE_DIRECTORY "${ABUSE_GENERATED_DOCDIR}")
    foreach(_abuse_compressed_document IN ITEMS
            "CHANGELOG.md;changelog.gz"
            "doc/abuse.6;abuse.6.gz")
        list(GET _abuse_compressed_document 0 _abuse_document_source)
        list(GET _abuse_compressed_document 1 _abuse_document_output)
        execute_process(
            COMMAND "${GZIP_EXECUTABLE}" -9 -n -c
                "${CMAKE_CURRENT_SOURCE_DIR}/${_abuse_document_source}"
            OUTPUT_FILE
                "${ABUSE_GENERATED_DOCDIR}/${_abuse_document_output}"
            RESULT_VARIABLE _abuse_gzip_result)
        if(NOT _abuse_gzip_result EQUAL 0)
            message(FATAL_ERROR
                "Could not compress ${_abuse_document_source}")
        endif()
    endforeach()

    install(FILES AUTHORS DESTINATION "${CMAKE_INSTALL_DOCDIR}")
    install(FILES "${ABUSE_GENERATED_DOCDIR}/changelog.gz"
        DESTINATION "${CMAKE_INSTALL_DOCDIR}")
    install(FILES "${ABUSE_GENERATED_DOCDIR}/abuse.6.gz"
        DESTINATION "${CMAKE_INSTALL_MANDIR}/man6")

    if(ABUSE_HOST_DISTRIBUTION_IDS MATCHES
            "(^| )(debian|ubuntu)( |$)")
        configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/packaging/debian/changelog.in"
            "${ABUSE_GENERATED_DOCDIR}/changelog.Debian"
            @ONLY)
        execute_process(
            COMMAND "${GZIP_EXECUTABLE}" -9 -n -c
                "${ABUSE_GENERATED_DOCDIR}/changelog.Debian"
            OUTPUT_FILE "${ABUSE_GENERATED_DOCDIR}/changelog.Debian.gz"
            RESULT_VARIABLE _abuse_gzip_result)
        if(NOT _abuse_gzip_result EQUAL 0)
            message(FATAL_ERROR "Could not compress the Debian changelog")
        endif()
        install(FILES packaging/debian/copyright
            DESTINATION "${CMAKE_INSTALL_DOCDIR}")
        install(FILES "${ABUSE_GENERATED_DOCDIR}/changelog.Debian.gz"
            DESTINATION "${CMAKE_INSTALL_DOCDIR}")
        install(FILES packaging/debian/lintian-overrides
            DESTINATION "${CMAKE_INSTALL_DATADIR}/lintian/overrides"
            RENAME abuse)
    else()
        # RPM packages carry their license texts because RPM distributions do
        # not provide Debian's /usr/share/common-licenses directory.
        install(FILES COPYING
            DESTINATION "${CMAKE_INSTALL_DOCDIR}"
            RENAME copyright)
        install(FILES COPYING.GPL COPYING.WTFPL COPYING.toml11
            DESTINATION "${CMAKE_INSTALL_DOCDIR}")
    endif()

    # SDL_mixer loads FluidSynth at runtime, so it is not visible in abuse's
    # normal link dependency graph. Pass it to linuxdeploy explicitly when the
    # installed SDL_mixer package exposes that decoder.
    if(NOT DEFINED SDLMIXER_MIDI_FLUIDSYNTH OR SDLMIXER_MIDI_FLUIDSYNTH)
        find_library(FLUIDSYNTH_RUNTIME_LIBRARY NAMES fluidsynth)
    endif()
    string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" APPIMAGE_ARCH)
    if(APPIMAGE_ARCH MATCHES "^(amd64|x64|x86_64)$")
        set(APPIMAGE_ARCH x86_64)
    elseif(APPIMAGE_ARCH MATCHES "^(aarch64|arm64)$")
        set(APPIMAGE_ARCH aarch64)
    elseif(APPIMAGE_ARCH MATCHES "^(i[3-6]86|x86)$")
        set(APPIMAGE_ARCH i686)
    endif()

    # The newest glibc symbol used by any executable or bundled library defines
    # an AppImage's minimum compatible glibc. Keep this check next to the
    # packaging target so a build on a rolling distribution cannot silently
    # produce an incompatible image.
    set(ABUSE_APPIMAGE_GLIBC_BASELINE "2.36" CACHE STRING
        "Newest GLIBC symbol version allowed in an AppImage")
    find_program(READELF_EXECUTABLE NAMES readelf REQUIRED)

    find_program(DOCKER_EXECUTABLE NAMES docker podman)
    if(DOCKER_EXECUTABLE AND APPIMAGE_ARCH STREQUAL "x86_64")
        set(_abuse_container_user_args "")
        find_program(ID_EXECUTABLE NAMES id)
        if(ID_EXECUTABLE)
            execute_process(COMMAND "${ID_EXECUTABLE}" -u
                OUTPUT_VARIABLE _abuse_host_uid
                OUTPUT_STRIP_TRAILING_WHITESPACE)
            execute_process(COMMAND "${ID_EXECUTABLE}" -g
                OUTPUT_VARIABLE _abuse_host_gid
                OUTPUT_STRIP_TRAILING_WHITESPACE)
            if(_abuse_host_uid MATCHES "^[0-9]+$" AND
                    _abuse_host_gid MATCHES "^[0-9]+$")
                set(_abuse_container_user_args
                    --user "${_abuse_host_uid}:${_abuse_host_gid}")
            endif()
        endif()
        add_custom_target(appimage-container
            COMMAND "${CMAKE_COMMAND}" -E make_directory
                "${CMAKE_BINARY_DIR}/appimage"
            COMMAND "${DOCKER_EXECUTABLE}" build
                --file packaging/appimage/Dockerfile
                --target appimage-artifact
                --tag "abuse-appimage-builder:${PROJECT_VERSION}"
                .
            COMMAND "${DOCKER_EXECUTABLE}" run --rm
                ${_abuse_container_user_args}
                --volume "${CMAKE_BINARY_DIR}/appimage:/output"
                "abuse-appimage-builder:${PROJECT_VERSION}"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            COMMENT "Building the Debian 12 AppImage with a GLIBC 2.36 baseline"
            USES_TERMINAL
            VERBATIM
        )

        add_custom_target(packages-container
            COMMAND "${CMAKE_COMMAND}" -E remove_directory
                "${CMAKE_BINARY_DIR}/packages"
            COMMAND "${CMAKE_COMMAND}" -E make_directory
                "${CMAKE_BINARY_DIR}/packages"
            COMMAND "${DOCKER_EXECUTABLE}" build
                --file packaging/appimage/Dockerfile
                --target packages-artifact
                --tag "abuse-debian-package-builder:${PROJECT_VERSION}"
                .
            COMMAND "${DOCKER_EXECUTABLE}" run --rm
                ${_abuse_container_user_args}
                --volume "${CMAKE_BINARY_DIR}/packages:/output"
                "abuse-debian-package-builder:${PROJECT_VERSION}"
            COMMAND "${DOCKER_EXECUTABLE}" build
                --file packaging/rpm/Dockerfile
                --tag "abuse-rpm-package-builder:${PROJECT_VERSION}"
                .
            COMMAND "${DOCKER_EXECUTABLE}" run --rm
                --cap-add SYS_ADMIN
                --cap-add NET_ADMIN
                --security-opt seccomp=unconfined
                --security-opt systempaths=unconfined
                --env "ABUSE_VERSION=${PROJECT_VERSION}"
                --env "OUTPUT_UID=${_abuse_host_uid}"
                --env "OUTPUT_GID=${_abuse_host_gid}"
                --volume "${CMAKE_BINARY_DIR}/packages:/output"
                "abuse-rpm-package-builder:${PROJECT_VERSION}"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            COMMENT "Building all packages in two containers"
            USES_TERMINAL
            VERBATIM
        )
    elseif(NOT DOCKER_EXECUTABLE)
        message(STATUS
            "docker/podman was not found; the container package targets are disabled")
    endif()

    find_program(LINUXDEPLOY
        NAMES linuxdeploy "linuxdeploy-${APPIMAGE_ARCH}.AppImage"
        DOC "Path to linuxdeploy or its official AppImage")
    if(LINUXDEPLOY)
        set(APPDIR "${CMAKE_BINARY_DIR}/AppDir")
        set(LINUXDEPLOY_APPDIR_ARGS
            --appdir "${APPDIR}"
            --deploy-deps-only "${APPDIR}/usr/${ABUSE_INSTALL_BINDIR}/abuse")
        if(FLUIDSYNTH_RUNTIME_LIBRARY)
            list(APPEND LINUXDEPLOY_APPDIR_ARGS --library "${FLUIDSYNTH_RUNTIME_LIBRARY}")
        else()
            message(STATUS
                "No separate FluidSynth runtime was found; assuming SDL3_mixer contains the decoder without an external library")
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
            COMMAND "${CMAKE_COMMAND}"
                "-DAPPDIR=${APPDIR}"
                "-DREADELF=${READELF_EXECUTABLE}"
                "-DMAX_GLIBC=${ABUSE_APPIMAGE_GLIBC_BASELINE}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/packaging/appimage/CheckGlibcBaseline.cmake"
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

    find_program(FLATPAK_EXECUTABLE
        NAMES flatpak
        DOC "Path to the flatpak command-line tool")
    find_program(FLATPAK_BUILDER_EXECUTABLE
        NAMES flatpak-builder
        DOC "Path to flatpak-builder")
    if(FLATPAK_EXECUTABLE)
        if(FLATPAK_BUILDER_EXECUTABLE)
            set(FLATPAK_BUILDER_COMMAND "${FLATPAK_BUILDER_EXECUTABLE}")
        else()
            set(FLATPAK_BUILDER_COMMAND
                "${FLATPAK_EXECUTABLE}" run org.flatpak.Builder)
            message(STATUS "flatpak-builder was not found; the flatpak target will use org.flatpak.Builder")
        endif()

        set(FLATPAK_APP_ID "com.github.metinc.abuse")
        set(FLATPAK_MANIFEST
            "${CMAKE_CURRENT_SOURCE_DIR}/packaging/flatpak/${FLATPAK_APP_ID}.yaml")
        set(FLATPAK_STATE_DIR "${CMAKE_BINARY_DIR}/.flatpak-builder")
        set(FLATPAK_BUILD_DIR "${CMAKE_BINARY_DIR}/flatpak-build")
        set(FLATPAK_REPOSITORY "${CMAKE_BINARY_DIR}/flatpak-repo")
        set(FLATPAK_OUTPUT
            "${CMAKE_BINARY_DIR}/Abuse-${PROJECT_VERSION}-${APPIMAGE_ARCH}.flatpak")

        add_custom_target(flatpak
            COMMAND ${FLATPAK_BUILDER_COMMAND}
                --user
                --install-deps-from=flathub
                --force-clean
                "--state-dir=${FLATPAK_STATE_DIR}"
                "--repo=${FLATPAK_REPOSITORY}"
                "${FLATPAK_BUILD_DIR}"
                "${FLATPAK_MANIFEST}"
            COMMAND ${CMAKE_COMMAND} -E rm -f "${FLATPAK_OUTPUT}"
            COMMAND "${FLATPAK_EXECUTABLE}" build-bundle
                "--arch=${APPIMAGE_ARCH}"
                "${FLATPAK_REPOSITORY}"
                "${FLATPAK_OUTPUT}"
                "${FLATPAK_APP_ID}"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            COMMENT "Creating Abuse-${PROJECT_VERSION}-${APPIMAGE_ARCH}.flatpak"
            VERBATIM
        )
    else()
        message(STATUS "flatpak was not found; the flatpak target is disabled")
    endif()
endif()

# Packaging
set(CPACK_PACKAGE_NAME "Abuse")
set(CPACK_PACKAGE_VENDOR "Crack-Dot-Com")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Dark 2D side-scrolling run-and-gun game")
set(ABUSE_PACKAGE_DESCRIPTION
    "Abuse is an SDL port of the 1995 run-and-gun game.\nIt includes the original campaign, multiplayer levels, and add-ons.\nMusic, sound effects, and several SoundFonts are bundled as well.")
set(CPACK_PACKAGE_DESCRIPTION "${ABUSE_PACKAGE_DESCRIPTION}")
set(CPACK_PACKAGE_HOMEPAGE_URL "${PROJECT_HOMEPAGE_URL}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")
set(CPACK_PACKAGE_EXECUTABLES "abuse;Abuse")
set(CPACK_PACKAGE_INSTALL_DIRECTORY ${CPACK_PACKAGE_NAME})

if(WIN32)
    set(CPACK_PACKAGE_FILE_NAME "Abuse-${PROJECT_VERSION}-win64")
    if(CMAKE_CROSSCOMPILING)
        set(CPACK_GENERATOR "ZIP")
        set(ABUSE_WINDOWS_OUTPUT_DIR "${CMAKE_BINARY_DIR}" CACHE PATH
            "Output directory for Windows packages")
        set(CPACK_PACKAGE_DIRECTORY "${ABUSE_WINDOWS_OUTPUT_DIR}")
    else()
        set(CPACK_GENERATOR "WIX;ZIP")
    endif()
    set(CPACK_WIX_UPGRADE_GUID "3A89DEB1-B520-4DF5-B44D-96A0EBB27DC0")
    set(CPACK_WIX_LICENSE_RTF "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")
    set(CPACK_WIX_PRODUCT_ICON "${CMAKE_CURRENT_SOURCE_DIR}/doc/icon.ico")
    set(CPACK_WIX_PROGRAM_MENU_FOLDER "Abuse")
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop;TGZ")
    set(CPACK_PACKAGING_INSTALL_PREFIX .)
    set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
else()
    # A native package must be built against the ABI and package database of
    # its target distribution. Select only the host's native generator for the
    # default package target; cpack -G remains available for explicit builds.
    set(CPACK_GENERATOR "TGZ")
    if(ABUSE_HOST_DISTRIBUTION_IDS MATCHES
            "(^| )(debian|ubuntu)( |$)")
        list(APPEND CPACK_GENERATOR DEB)
    elseif(ABUSE_HOST_DISTRIBUTION_IDS MATCHES
            "(^| )(fedora|rhel|centos|rocky|almalinux|suse|opensuse|mageia)( |$)")
        list(APPEND CPACK_GENERATOR RPM)
    endif()
    set(CPACK_PACKAGE_CONTACT "Metin Çelik <metincelik88@gmail.com>")

    # dpkg-shlibdeps derives libc, libstdc++, libgcc and the three SDL runtime
    # dependencies (including their minimum ABI versions) from the packaged
    # executable. This intentionally requires DEB packages to be built on the
    # target Debian-family distribution rather than from an unrelated host.
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "${ABUSE_PACKAGE_DESCRIPTION}")
    set(ABUSE_DEBIAN_RUNTIME_DEPENDS "" CACHE STRING
        "Additional runtime-loaded dependencies for Debian packages")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "${ABUSE_DEBIAN_RUNTIME_DEPENDS}")
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_DEBIAN_PACKAGE_RELEASE 1)
    set(CPACK_DEBIAN_PACKAGE_SECTION "games")
    set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION ON)

    # RPM's automatic dependency generator records all direct ELF SONAME and
    # symbol-version requirements. Keep explicit requirements only for the
    # minimum SDL API versions. For native builds, optional decoder dependencies
    # are owned by the distribution's SDL3_mixer package. The container build
    # links FluidSynth directly so this scanner records it automatically.
    set(CPACK_RPM_PACKAGE_AUTOREQ ON)
    set(CPACK_RPM_PACKAGE_DESCRIPTION "${ABUSE_PACKAGE_DESCRIPTION}")
    set(ABUSE_RPM_RUNTIME_REQUIRES
        "SDL3 >= 3.4.0, SDL3_mixer >= 3.2.0, SDL3_net >= 3.0.0"
        CACHE STRING "Additional runtime requirements for RPM packages")
    set(CPACK_RPM_PACKAGE_REQUIRES "${ABUSE_RPM_RUNTIME_REQUIRES}")
    set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
    set(CPACK_RPM_PACKAGE_RELEASE 1)
    set(CPACK_RPM_PACKAGE_LICENSE
        "GPL-2.0-or-later AND GPL-2.0-only AND MIT AND WTFPL AND LicenseRef-Abuse-Data")
    set(CPACK_RPM_PACKAGE_GROUP "Amusements/Games")
    set(CPACK_RPM_USER_FILELIST
        "%license /usr/${CMAKE_INSTALL_DOCDIR}/copyright"
        "%license /usr/${CMAKE_INSTALL_DOCDIR}/COPYING.GPL"
        "%license /usr/${CMAKE_INSTALL_DOCDIR}/COPYING.WTFPL"
        "%license /usr/${CMAKE_INSTALL_DOCDIR}/COPYING.toml11")

    # Release packages must not accidentally contain an unoptimized Debug
    # executable. The per-generator CPack hook enforces this at package time.
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/CPackProjectConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/packaging/CPackProjectConfig.cmake"
        @ONLY)
    set(CPACK_PROJECT_CONFIG_FILE
        "${CMAKE_CURRENT_BINARY_DIR}/packaging/CPackProjectConfig.cmake")
    set(CPACK_STRIP_FILES ON)
endif()

include(CPack)

if(WIN32 AND CMAKE_CROSSCOMPILING)
    find_program(WIXL_EXECUTABLE NAMES wixl DOC "Path to wixl")
    find_program(WIXL_HEAT_EXECUTABLE NAMES wixl-heat DOC "Path to wixl-heat")

    set(WINDOWS_ZIP_OUTPUT
        "${ABUSE_WINDOWS_OUTPUT_DIR}/Abuse-${PROJECT_VERSION}-win64.zip")
    add_custom_target(windows-zip
        COMMAND "${CMAKE_CPACK_COMMAND}"
            --config "${CMAKE_BINARY_DIR}/CPackConfig.cmake"
            -G ZIP
            -B "${ABUSE_WINDOWS_OUTPUT_DIR}"
        DEPENDS abuse
        BYPRODUCTS "${WINDOWS_ZIP_OUTPUT}"
        COMMENT "Creating Abuse-${PROJECT_VERSION}-win64.zip"
        VERBATIM
    )

    if(WIXL_EXECUTABLE AND WIXL_HEAT_EXECUTABLE)
        set(WINDOWS_STAGE_DIR "${CMAKE_BINARY_DIR}/windows-msi-stage")
        set(WINDOWS_MSI_OUTPUT
            "${ABUSE_WINDOWS_OUTPUT_DIR}/Abuse-${PROJECT_VERSION}-win64.msi")
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/packaging/windows")
        configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/packaging/windows/abuse.wxs.in"
            "${CMAKE_BINARY_DIR}/packaging/windows/abuse.wxs"
            @ONLY
        )

        add_custom_target(windows-msi
            COMMAND "${CMAKE_COMMAND}" -E remove_directory "${WINDOWS_STAGE_DIR}"
            COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}"
                --config Release --prefix "${WINDOWS_STAGE_DIR}"
            COMMAND "${CMAKE_COMMAND}"
                "-DWIXL_EXECUTABLE=${WIXL_EXECUTABLE}"
                "-DWIXL_HEAT_EXECUTABLE=${WIXL_HEAT_EXECUTABLE}"
                "-DSTAGING_DIR=${WINDOWS_STAGE_DIR}"
                "-DPRODUCT_WXS=${CMAKE_BINARY_DIR}/packaging/windows/abuse.wxs"
                "-DOUTPUT=${WINDOWS_MSI_OUTPUT}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/packaging/windows/build-msi.cmake"
            DEPENDS abuse
            BYPRODUCTS "${WINDOWS_MSI_OUTPUT}"
            COMMENT "Creating Abuse-${PROJECT_VERSION}-win64.msi"
            VERBATIM
        )
        add_custom_target(windows-package-artifacts
            DEPENDS windows-zip windows-msi)
    else()
        message(STATUS "wixl or wixl-heat was not found; the windows-msi target is disabled")
    endif()
endif()

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux" AND NOT CMAKE_CROSSCOMPILING)
    set(VCPKG_ROOT "$ENV{VCPKG_ROOT}" CACHE PATH "Path to a vcpkg ports checkout")
    find_program(HOST_VCPKG_EXECUTABLE NAMES vcpkg)
    find_program(MINGW_C_COMPILER NAMES x86_64-w64-mingw32-gcc)
    find_program(MINGW_CXX_COMPILER NAMES x86_64-w64-mingw32-g++)
    find_program(MINGW_RC_COMPILER NAMES x86_64-w64-mingw32-windres)
    find_program(HOST_WIXL_EXECUTABLE NAMES wixl)
    find_program(HOST_WIXL_HEAT_EXECUTABLE NAMES wixl-heat)

    if(EXISTS "${VCPKG_ROOT}/.vcpkg-root" AND HOST_VCPKG_EXECUTABLE
            AND MINGW_C_COMPILER AND MINGW_CXX_COMPILER AND MINGW_RC_COMPILER
            AND HOST_WIXL_EXECUTABLE AND HOST_WIXL_HEAT_EXECUTABLE)
        set(WINDOWS_CROSS_BUILD_DIR "${CMAKE_BINARY_DIR}/windows-x64")
        set(WINDOWS_VCPKG_INSTALLED_DIR
            "${WINDOWS_CROSS_BUILD_DIR}/vcpkg_installed")
        add_custom_target(windows-packages
            COMMAND "${CMAKE_COMMAND}" -E env VCPKG_DISABLE_METRICS=1
                "${HOST_VCPKG_EXECUTABLE}" install
                --triplet x64-mingw-dynamic
                --host-triplet x64-linux
                "--vcpkg-root=${VCPKG_ROOT}"
                "--x-manifest-root=${CMAKE_CURRENT_SOURCE_DIR}"
                "--x-install-root=${WINDOWS_VCPKG_INSTALLED_DIR}"
                --x-wait-for-lock
            COMMAND "${CMAKE_COMMAND}"
                -S "${CMAKE_CURRENT_SOURCE_DIR}"
                -B "${WINDOWS_CROSS_BUILD_DIR}"
                -G "${CMAKE_GENERATOR}"
                -DCMAKE_BUILD_TYPE=Release
                "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_CURRENT_SOURCE_DIR}/packaging/windows/x86_64-w64-mingw32.cmake"
                "-DCMAKE_PREFIX_PATH=${WINDOWS_VCPKG_INSTALLED_DIR}/x64-mingw-dynamic"
                -DABUSE_BUILD_TOOLS=OFF
                "-DABUSE_HMI2MID_EXECUTABLE=$<TARGET_FILE:abuse-hmi2mid>"
                "-DABUSE_WINDOWS_OUTPUT_DIR=${CMAKE_BINARY_DIR}"
                "-DWIXL_EXECUTABLE=${HOST_WIXL_EXECUTABLE}"
                "-DWIXL_HEAT_EXECUTABLE=${HOST_WIXL_HEAT_EXECUTABLE}"
            COMMAND "${CMAKE_COMMAND}" --build "${WINDOWS_CROSS_BUILD_DIR}"
                --config Release --target windows-package-artifacts --parallel
            DEPENDS abuse-hmi2mid
            COMMENT "Cross-building Windows ZIP and MSI"
            USES_TERMINAL
            VERBATIM
        )
    else()
        message(STATUS
            "vcpkg with its ports checkout, MinGW-w64, or msitools was not found; the windows-packages target is disabled")
    endif()
endif()
