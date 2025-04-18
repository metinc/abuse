cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0074 NEW)
cmake_policy(SET CMP0177 NEW)

project(abuse
    VERSION 1.0.2
    DESCRIPTION "Run and gun game"
    LANGUAGES C CXX
)

# Options
option(ABUSE_ENABLE_NETWORK "Enable networking support" ON)

# Find appimagetool
find_program(APPIMAGETOOL appimagetool)

# Dependencies
find_package(SDL2 2.0.0 REQUIRED)
find_package(SDL2_mixer 2.0.0 REQUIRED)
set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED)
find_package(OpenCV REQUIRED)
find_package(GLEW REQUIRED)
include_directories(${GLEW_INCLUDE_DIRS})
link_libraries(${GLEW_LIBRARIES})

# System checks
include(CheckIncludeFiles)

check_include_files(unistd.h HAVE_UNISTD_H)
check_include_files("sys/time.h" HAVE_SYS_TIME_H)

option(HAVE_NETWORK "Enable networking support" TRUE)

if(TCPIP_DEBUG)
    add_compile_definitions(TCPIP_DEBUG)
endif()

if(NO_CHECK)
    add_compile_definitions(NO_CHECK)
endif()
        
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

# Asset directory setup
if(WIN32)
    set(ASSETDIR "." CACHE PATH "Asset directory")
elseif(APPLE)
    set(ASSETDIR "abuse.app/Contents/Resources/data" CACHE PATH "Asset directory")
else()
    if(APPIMAGETOOL)
        set(CMAKE_INSTALL_PREFIX "/usr")
    endif()
    set(ASSETDIR "share/abuse" CACHE PATH "Asset directory")
endif()

# Define ASSETDIR for all platforms
add_definitions(-DASSETDIR="${CMAKE_INSTALL_PREFIX}/${ASSETDIR}")

add_definitions(-DPACKAGE_NAME="abuse")
add_definitions(-DPACKAGE_VERSION="${PROJECT_VERSION}")
add_definitions(-DHAVE_CONFIG_H)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

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

    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libsdl2-2.0-0, libsdl2-mixer-2.0-0, libglew2.2")
    set(CPACK_DEBIAN_PACKAGE_SECTION "games")

    set(CPACK_RPM_PACKAGE_LICENSE "GPL-2.0")
    set(CPACK_RPM_PACKAGE_GROUP "Amusements/Games")
    set(CPACK_RPM_PACKAGE_REQUIRES "SDL2, SDL2_mixer")
endif()

include(CPack)

add_subdirectory(src)
add_subdirectory(data)

# Installation targets for Linux
if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)
    install(TARGETS abuse
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

    if(APPIMAGETOOL)
        # AppImage specific installation
        set(APPDIR "${CMAKE_BINARY_DIR}/AppDir")

        # Configure and install AppRun
        configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/AppRun.in"
            "${CMAKE_CURRENT_BINARY_DIR}/AppRun"
            @ONLY
        )
        install(FILES "${CMAKE_CURRENT_BINARY_DIR}/AppRun"
            PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_EXECUTE WORLD_READ
            DESTINATION "${APPDIR}")

        # Install main executable
        install(TARGETS abuse
            RUNTIME DESTINATION "${APPDIR}/usr/bin")

        # Install game data
        install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/data/"
            DESTINATION "${APPDIR}/usr/share/abuse")

        # Install desktop file and icon
        configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/abuse.desktop.in"
            "${CMAKE_CURRENT_BINARY_DIR}/abuse.desktop"
            @ONLY
        )

        # Make sure the desktop file is also in the AppDir root as required by AppImage spec
        install(FILES "${CMAKE_CURRENT_BINARY_DIR}/abuse.desktop"
            DESTINATION "${APPDIR}")
        install(FILES "${CMAKE_CURRENT_BINARY_DIR}/abuse.desktop"
            DESTINATION "${APPDIR}/usr/share/applications")

        # Install icon
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/doc/abuse.png"
            DESTINATION "${APPDIR}/usr/share/icons/hicolor/128x128/apps")
        # Copy icon to AppDir root with both names needed
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/doc/abuse.png"
            DESTINATION "${APPDIR}"
            RENAME ".DirIcon")
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/doc/abuse.png"
            DESTINATION "${APPDIR}"
            RENAME "abuse.png")

        # Shared library installation
        set(BASE_LIBS
            "libGLEW"
            "libSDL2-2.0"
            "libSDL2_mixer"
            "libSDL2_mixer-2.0"
            "libSDL3"
            "libfluidsynth"
            "libportaudio"
            "libinstpatch-1.0"
        )

        # Function to find all matching libraries for a base name
        function(find_and_install_libraries base_name dest_dir)
            file(GLOB MATCHING_LIBS
                "/usr/lib/${base_name}.so*"
                "/usr/lib64/${base_name}.so*"
                "/usr/lib/x86_64-linux-gnu/${base_name}.so*"
            )

            foreach(LIB_PATH IN LISTS MATCHING_LIBS)
                if(EXISTS "${LIB_PATH}")
                    install(FILES "${LIB_PATH}" DESTINATION "${dest_dir}")
                endif()
            endforeach()
        endfunction()

        # Find and install all required libraries
        foreach(BASE_LIB IN LISTS BASE_LIBS)
            find_and_install_libraries(${BASE_LIB} "${APPDIR}/usr/lib")
        endforeach()

        # Create AppImage
        add_custom_target(appimage
            COMMAND ${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR} --prefix ${APPDIR}/usr
            COMMAND ${CMAKE_COMMAND} -E env ARCH=x86_64 ${APPIMAGETOOL} "${APPDIR}" "Abuse-${PROJECT_VERSION}.AppImage"
            DEPENDS abuse
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Creating AppImage..."
        )
    endif()

    # Regular installation
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/data/"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/abuse"
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
        DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
endif()

# Windows installation
if(WIN32)
    find_file(SDL2_DLL
        NAMES SDL2.dll
        HINTS $ENV{SDL2DIR} "${CMAKE_CURRENT_SOURCE_DIR}/dlls" "C:/msys64/mingw64/bin"
        PATH_SUFFIXES bin
    )
    if(NOT SDL2_DLL)
        message(WARNING "SDL2.dll not found. Please set SDL2DIR or ensure SDL2.dll is in the specified paths.")
    endif()
    install(TARGETS abuse
        RUNTIME DESTINATION ".")

    if(SDL2_DLL)
        install(FILES ${SDL2_DLL}
            DESTINATION ".")
    endif()
endif()

# macOS installation
if(APPLE)
    install(TARGETS abuse
        BUNDLE DESTINATION .)

    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/data/"
        DESTINATION "${CMAKE_INSTALL_PREFIX}/abuse.app/Contents/Resources/data")
endif()
