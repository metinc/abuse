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

#INCLUDES = -I$(srcdir) -I$(srcdir)/../ -I$(srcdir)/../imlib $(SDL3_CFLAGS)

#CPPFLAGS = @CPPFLAGS@ -DNO_CHECK
