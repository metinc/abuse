add_library(net STATIC
    gserver.cpp gserver.h
    gclient.cpp gclient.h
    fileman.cpp fileman.h
    sock.cpp sock.h
    tcpip.cpp tcpip.h
    ghandler.h netface.h
)

include_directories(
    ${abuse_SOURCE_DIR}/src
    ${abuse_SOURCE_DIR}/src/imlib
)

target_link_libraries(net SDL3::SDL3 SDL3_net::SDL3_net)

add_definitions(-DNO_CHECK)
