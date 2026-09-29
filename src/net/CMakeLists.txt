add_library(net STATIC
    gserver.cpp gserver.h
    gclient.cpp gclient.h
    fileman.cpp fileman.h
    sock.cpp sock.h
    ghandler.h netface.h
)

include_directories(
    ${abuse_SOURCE_DIR}/src
    ${abuse_SOURCE_DIR}/src/imlib
)

target_link_libraries(net SDL3::SDL3)
if(ABUSE_NETWORK)
    target_sources(net PRIVATE tcpip.cpp tcpip.h webrtc.cpp webrtc.h)
    target_link_libraries(net SDL3_net::SDL3_net
        LibDataChannel::LibDataChannel nlohmann_json::nlohmann_json)
endif()

add_definitions(-DNO_CHECK)
