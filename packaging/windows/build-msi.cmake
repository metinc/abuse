foreach(required_variable
        WIXL_EXECUTABLE WIXL_HEAT_EXECUTABLE STAGING_DIR PRODUCT_WXS OUTPUT)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

file(GLOB_RECURSE staged_files LIST_DIRECTORIES FALSE "${STAGING_DIR}/*")
if(NOT staged_files)
    message(FATAL_ERROR "No files found in ${STAGING_DIR}")
endif()

list(SORT staged_files)
string(REPLACE ";" "\n" staged_file_list "${staged_files}")
set(file_list "${CMAKE_CURRENT_BINARY_DIR}/windows-msi-files.txt")
set(component_wxs "${CMAKE_CURRENT_BINARY_DIR}/windows-msi-files.wxs")
file(WRITE "${file_list}" "${staged_file_list}\n")

execute_process(
    COMMAND "${WIXL_HEAT_EXECUTABLE}"
        --directory-ref INSTALLDIR
        --component-group AbuseFiles
        --var var.SourceDir
        --prefix "${STAGING_DIR}/"
        --win64
    INPUT_FILE "${file_list}"
    OUTPUT_FILE "${component_wxs}"
    ERROR_VARIABLE heat_error
    RESULT_VARIABLE heat_result
)
if(NOT heat_result EQUAL 0)
    message(FATAL_ERROR "wixl-heat failed: ${heat_error}")
endif()

execute_process(
    COMMAND "${WIXL_EXECUTABLE}"
        -D "SourceDir=${STAGING_DIR}"
        -D Win64=yes
        --arch x64
        -o "${OUTPUT}"
        "${PRODUCT_WXS}"
        "${component_wxs}"
    ERROR_VARIABLE wixl_error
    RESULT_VARIABLE wixl_result
)
if(NOT wixl_result EQUAL 0)
    message(FATAL_ERROR "wixl failed: ${wixl_error}")
endif()
