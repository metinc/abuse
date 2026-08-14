add_library(imlib STATIC
    filter.cpp filter.h
    image.cpp image.h
    transimage.cpp transimage.h
    linked.cpp linked.h
    input.cpp input.h
    palette.cpp palette.h
    include.cpp include.h
    fonts.cpp fonts.h
    specs.cpp specs.h
    supmorph.cpp supmorph.h
    pcxread.cpp pcxread.h
    jrand.cpp jrand.h
    keys.cpp keys.h
    status.cpp status.h
    pmenu.cpp pmenu.h
    scroller.cpp scroller.h
    filesel.cpp filesel.h
    tools.cpp tools.h
    guistat.cpp guistat.h
    sprite.cpp sprite.h
    jwindow.cpp jwindow.h
    video.h
    event.h
    timing.h jdir.h visobj.h
    ../sdlport/video.cpp
    ../sdlport/event.cpp
    ../sdlport/sound.cpp ../sdlport/sound.h
    ../sdlport/timing.cpp
    ../sdlport/jdir.cpp
    ../sdlport/joystick.cpp ../sdlport/joy.h
    ../sdlport/setup.cpp ../sdlport/setup.h
    ../sdlport/hmi.cpp ../sdlport/hmi.h
    ../sdlport/errorui.cpp ../sdlport/errorui.h
)

include_directories(
    ${abuse_SOURCE_DIR}/src
    ${abuse_SOURCE_DIR}/src/imlib
)
add_definitions(-DNO_CHECK)
target_link_libraries(imlib SDL3::SDL3 SDL3_mixer::SDL3_mixer toml11::toml11)
#INCLUDES = -I$(srcdir) -I$(srcdir)/.. $(SDL3_CFLAGS)

#CPPFLAGS = @CPPFLAGS@ -DNO_CHECK
