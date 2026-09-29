# Shared by the standalone package project and the native development build.
set(ABUSE_DOS_BUILD_JOBS "8" CACHE STRING "Parallel build jobs inside the DOS container")
if(NOT ABUSE_DOS_BUILD_JOBS MATCHES "^[1-9][0-9]*$")
    message(FATAL_ERROR "ABUSE_DOS_BUILD_JOBS must be a positive integer")
endif()
get_filename_component(_abuse_dos_source_dir "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

set(_abuse_dos_container_commands
    COMMAND "${DOCKER_EXECUTABLE}" build
        --file packaging/dos/Dockerfile
        --build-arg "JOBS=${ABUSE_DOS_BUILD_JOBS}"
        --tag "abuse-dos-package-builder:${PROJECT_VERSION}" .
    COMMAND "${DOCKER_EXECUTABLE}" run --rm --pull=never --network none
        ${_abuse_container_user_args}
        --volume "${CMAKE_BINARY_DIR}/packages:/output"
        "abuse-dos-package-builder:${PROJECT_VERSION}")

add_custom_target(dos-container
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/packages"
    ${_abuse_dos_container_commands}
    WORKING_DIRECTORY "${_abuse_dos_source_dir}"
    COMMENT "Cross-building the DOSBox ZIP with DJGPP in a container"
    USES_TERMINAL VERBATIM)
