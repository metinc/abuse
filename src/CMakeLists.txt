#

add_subdirectory(lisp)
add_subdirectory(net)
add_subdirectory(imlib)

if (WIN32)
    # FIXME: Where should this logic reside? Whatever, first generate the
    # resource script...
    get_filename_component(abuse_ICON_PATH "${CMAKE_SOURCE_DIR}/doc/icon.ico" ABSOLUTE)
    configure_file(win32/resources.rc.in ${CMAKE_CURRENT_BINARY_DIR}/resources.rc ESCAPE_QUOTES NEWLINE_STYLE WIN32)
    # And then include it
    # FIXME: I think this means the build location can't have spaces
    set(abuse_RESOURCE_FILES "${CMAKE_CURRENT_BINARY_DIR}/resources.rc")
else()
    set(abuse_RESOURCE_FILES "")
endif()

add_executable(abuse
    common.h
    file_utils.cpp file_utils.h
    lol/matrix.cpp lol/matrix.h
    specache.cpp specache.h
    netcfg.cpp netcfg.h
    innet.cpp
    endgame.cpp
    profile.cpp profile.h
    cop.cpp cop.h
    compiled.cpp compiled.h
    ant.cpp ant.h
    sensor.cpp
    demo.cpp demo.h    
    nfclient.cpp nfclient.h
    clisp.cpp clisp.h
    transp.cpp transp.h
    collide.cpp
    property.cpp property.h
    cache.cpp cache.h
    particle.cpp particle.h
    game_object.cpp game_object.h
    simple_object.cpp simple_object.h
    ability.cpp ability.h
    items.cpp items.h
    dev.cpp dev.h
    chars.cpp chars.h
    level.cpp level.h
    smallfnt.cpp
    automap.cpp automap.h
    intsect.cpp intsect.h
    loader2.cpp loader2.h
    seq.cpp seq.h
    points.cpp points.h
    fnt6x13.cpp
    morpher.cpp morpher.h
    director.cpp director.h
    view.cpp view.h
    configuration.cpp configuration.h
    game.cpp game.h
    light.cpp light.h
    devsel.cpp devsel.h
    crc.cpp crc.h
    id.h isllist.h
    nfserver.h
    ui/audio_settings.cpp ui/audio_settings.h
    ui/audio_volume.h
    ui/chat.cpp ui/chat.h
    ui/console.cpp ui/console.h
    ui/gui.cpp ui/gui.h
    ui/help.cpp ui/help.h
    ui/loadgame.cpp ui/loadgame.h
    ui/menu.cpp ui/menu.h
    ui/statbar.cpp ui/sbar.h
    ui/video_settings.cpp ui/video_settings.h
    ${abuse_RESOURCE_FILES}
)

if(WIN32)
    set_target_properties(abuse PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

if(APPLE)
    set_target_properties(abuse PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_ICON_FILE "abuse"
        MACOSX_BUNDLE_BUNDLE_NAME "Abuse"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    )
endif()

target_link_libraries(abuse
    lisp
    imlib
    net
    SDL3::SDL3
    SDL3_mixer::SDL3_mixer
)

# This small native tool is required by the asset build and deliberately has
# no SDL or game-runtime dependencies.
if(NOT CMAKE_CROSSCOMPILING)
    add_executable(abuse-hmi2mid
        sdlport/hmi.cpp sdlport/hmi.h
        tool/hmi2mid.cpp)
endif()

if(ABUSE_BUILD_TOOLS)
    add_executable(abuse-tool
        file_utils.cpp file_utils.h
        tool/abuse-tool.cpp
        sdlport/hmi.cpp sdlport/hmi.h
        crc.cpp crc.h
        tool/AR_SPEC.cpp tool/AR_SPEC.h
        tool/AR_Help.cpp tool/AR_Help.h
        tool/third_party/stb_image_write.h)

    target_link_libraries(abuse-tool PRIVATE imlib)
endif()

include_directories(
    ${abuse_SOURCE_DIR}/src
    ${abuse_SOURCE_DIR}/src/lisp
    ${abuse_SOURCE_DIR}/src/imlib
    ${abuse_SOURCE_DIR}/src/net
)
