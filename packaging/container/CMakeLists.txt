cmake_minimum_required(VERSION 3.21)

project(abuse-packages LANGUAGES NONE)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR
        NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(amd64|AMD64|x64|x86_64)$")
    message(FATAL_ERROR "Container package builds require an x86_64 Linux host")
endif()
find_program(DOCKER_EXECUTABLE NAMES docker podman REQUIRED)

get_filename_component(ABUSE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
# Keep package tags in sync with the game's version without configuring it.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${ABUSE_SOURCE_DIR}/CMakeLists.txt")
file(STRINGS "${ABUSE_SOURCE_DIR}/CMakeLists.txt" _abuse_version_line
    REGEX "^[ \t]*VERSION [0-9]+\\.[0-9]+\\.[0-9]+")
if(NOT _abuse_version_line MATCHES "VERSION ([0-9]+\\.[0-9]+\\.[0-9]+)")
    message(FATAL_ERROR "Could not read the game version from CMakeLists.txt")
endif()
set(PROJECT_VERSION "${CMAKE_MATCH_1}")

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
add_custom_target(packages-container ALL
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/packages"
    COMMAND "${DOCKER_EXECUTABLE}" build
        --file packaging/appimage/Dockerfile --target packages-artifact
        --tag "abuse-debian-package-builder:${PROJECT_VERSION}" .
    COMMAND "${DOCKER_EXECUTABLE}" run --rm --pull=never --network none
        ${_abuse_container_user_args}
        --volume "${CMAKE_BINARY_DIR}/packages:/output"
        "abuse-debian-package-builder:${PROJECT_VERSION}"
    COMMAND "${DOCKER_EXECUTABLE}" build
        --file packaging/rpm/Dockerfile
        --tag "abuse-rpm-package-builder:${PROJECT_VERSION}" .
    COMMAND "${DOCKER_EXECUTABLE}" run --rm --pull=never
        --cap-add SYS_ADMIN --cap-add NET_ADMIN
        --security-opt seccomp=unconfined
        --security-opt systempaths=unconfined
        --security-opt apparmor=unconfined
        --env "ABUSE_VERSION=${PROJECT_VERSION}"
        --env "OUTPUT_UID=${_abuse_host_uid}"
        --env "OUTPUT_GID=${_abuse_host_gid}"
        --volume "abuse-flatpak-cache:/cache"
        --volume "${CMAKE_BINARY_DIR}/packages:/output"
        "abuse-rpm-package-builder:${PROJECT_VERSION}"
    WORKING_DIRECTORY "${ABUSE_SOURCE_DIR}"
    COMMENT "Building all packages in two containers"
    USES_TERMINAL VERBATIM
)

add_custom_target(appimage-container
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/appimage"
    COMMAND "${DOCKER_EXECUTABLE}" build
        --file packaging/appimage/Dockerfile --target appimage-artifact
        --tag "abuse-appimage-builder:${PROJECT_VERSION}" .
    COMMAND "${DOCKER_EXECUTABLE}" run --rm --pull=never --network none
        ${_abuse_container_user_args}
        --volume "${CMAKE_BINARY_DIR}/appimage:/output"
        "abuse-appimage-builder:${PROJECT_VERSION}"
    WORKING_DIRECTORY "${ABUSE_SOURCE_DIR}"
    COMMENT "Building the Debian 12 AppImage with a GLIBC 2.36 baseline"
    USES_TERMINAL VERBATIM
)
