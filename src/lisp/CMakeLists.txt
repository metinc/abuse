add_library(lisp STATIC
    lisp.cpp lisp.h
    lisp_opt.cpp lisp_opt.h
    lisp_gc.cpp lisp_gc.h
    funcs.cpp funcs.h
    stack.h 
)

include_directories(
    ${abuse_SOURCE_DIR}/src
    ${abuse_SOURCE_DIR}/src/imlib
)


target_link_libraries(lisp SDL2_mixer::SDL2_mixer)

#INCLUDES = -I$(srcdir) -I$(srcdir)/../ -I$(srcdir)/../imlib $(SDL2_CFLAGS)

#CPPFLAGS = @CPPFLAGS@ -DNO_CHECK
