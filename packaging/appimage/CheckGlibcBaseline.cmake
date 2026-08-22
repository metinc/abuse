foreach(_required_variable IN ITEMS APPDIR READELF MAX_GLIBC)
    if(NOT DEFINED ${_required_variable} OR "${${_required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${_required_variable} must be set")
    endif()
endforeach()

if(NOT IS_DIRECTORY "${APPDIR}")
    message(FATAL_ERROR "AppDir does not exist: ${APPDIR}")
endif()

file(GLOB_RECURSE _appdir_files LIST_DIRECTORIES FALSE "${APPDIR}/*")
set(_maximum_found "0.0")
set(_offenders "")

foreach(_file IN LISTS _appdir_files)
    execute_process(
        COMMAND "${READELF}" --file-header "${_file}"
        RESULT_VARIABLE _header_result
        OUTPUT_QUIET
        ERROR_QUIET)
    if(NOT _header_result EQUAL 0)
        continue()
    endif()

    execute_process(
        COMMAND "${READELF}" --version-info "${_file}"
        RESULT_VARIABLE _version_result
        OUTPUT_VARIABLE _version_info
        ERROR_VARIABLE _version_error)
    if(NOT _version_result EQUAL 0)
        message(FATAL_ERROR
            "Could not inspect GLIBC requirements for ${_file}: ${_version_error}")
    endif()

    string(REGEX MATCHALL "GLIBC_[0-9]+\\.[0-9]+(\\.[0-9]+)?"
        _glibc_symbols "${_version_info}")
    list(REMOVE_DUPLICATES _glibc_symbols)
    set(_file_maximum "0.0")
    foreach(_symbol IN LISTS _glibc_symbols)
        string(REGEX REPLACE "^GLIBC_" "" _version "${_symbol}")
        if(_version VERSION_GREATER _file_maximum)
            set(_file_maximum "${_version}")
        endif()
        if(_version VERSION_GREATER _maximum_found)
            set(_maximum_found "${_version}")
        endif()
    endforeach()

    if(_file_maximum VERSION_GREATER MAX_GLIBC)
        file(RELATIVE_PATH _relative_file "${APPDIR}" "${_file}")
        list(APPEND _offenders
            "  ${_relative_file} requires GLIBC_${_file_maximum}")
    endif()
endforeach()

if(_offenders)
    list(JOIN _offenders "\n" _offender_text)
    message(FATAL_ERROR
        "AppImage GLIBC baseline exceeded (maximum allowed: GLIBC_${MAX_GLIBC}):\n"
        "${_offender_text}\n"
        "Build the executable and every bundled library in the supported "
        "container with the appimage-container target.")
endif()

message(STATUS
    "AppDir GLIBC requirement is GLIBC_${_maximum_found} (limit: GLIBC_${MAX_GLIBC})")
