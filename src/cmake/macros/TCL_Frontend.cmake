# The Tcl/Tk front end.
#
# Unlike the other front ends, this one does not look for a system library.
# It links the Tcl and Tk that scripts/build-tcltk builds from the sources
# vendored in tcltk/, and nothing else: see decision 12 in
# .claude/plans/phase3-tcl-tk-frontend.md.  A Homebrew Tcl or the Tk that ships
# with macOS would both appear to work and then diverge from what CI, the
# release and every other developer build is using.
#
# Two of the paths below are private headers -- tkInt.h and its friends -- which
# a plain `make install` does not install.  build-tcltk runs
# install-private-headers for exactly this reason, and the check here is what
# turns "that toolchain is incomplete" into a configure-time message rather than
# a compile error three minutes later.

macro(configure_tcl_frontend _NAME_TARGET)

    if(NOT TCLTK_PREFIX)
        set(TCLTK_PREFIX "${CMAKE_SOURCE_DIR}/tcltk/local")
        message(STATUS "TCLTK_PREFIX not given, trying ${TCLTK_PREFIX}")
    endif()

    find_library(TCL_LIBRARY
        NAMES tcl9.0 tcl90 tcl
        HINTS "${TCLTK_PREFIX}/lib"
        NO_DEFAULT_PATH
    )
    find_library(TK_LIBRARY
        NAMES tcl9tk9.0 tcl9tk90 tk9.0 tk
        HINTS "${TCLTK_PREFIX}/lib"
        NO_DEFAULT_PATH
    )
    find_path(TCL_INCLUDE_DIR
        NAMES tcl.h
        HINTS "${TCLTK_PREFIX}/include"
        NO_DEFAULT_PATH
    )

    if(NOT TCL_LIBRARY OR NOT TK_LIBRARY OR NOT TCL_INCLUDE_DIR)
        message(FATAL_ERROR
            "Support for Tcl/Tk front end - Failed.\n"
            "No Tcl/Tk 9 found under ${TCLTK_PREFIX}.\n"
            "Build it with scripts/build-tcltk, or point -DTCLTK_PREFIX at a "
            "prefix that has one.")
    endif()

    # The port needs Tk's private headers.  Say so here, by name, rather than
    # letting the compiler fail on a missing include later.
    if(NOT EXISTS "${TCL_INCLUDE_DIR}/tkInt.h")
        message(FATAL_ERROR
            "Support for Tcl/Tk front end - Failed.\n"
            "${TCL_INCLUDE_DIR} has no tkInt.h, so this toolchain was installed "
            "without its private headers.\n"
            "Re-run scripts/build-tcltk, which runs install-private-headers.")
    endif()

    target_include_directories(${_NAME_TARGET} PRIVATE "${TCL_INCLUDE_DIR}")
    target_link_libraries(${_NAME_TARGET} PRIVATE
        "${TCL_LIBRARY}"
        "${TK_LIBRARY}"
    )
    target_compile_definitions(${_NAME_TARGET} PRIVATE
        USE_TCL
        # Where Tcl's and Tk's own script libraries live.  A dev build runs from
        # the build directory, where Tcl_Init cannot find init.tcl by walking up
        # from the executable, so the front end falls back to this at startup.
        # The eventual .app bundle carries its own copy and never uses it.
        TCLTK_PREFIX_PATH="${TCLTK_PREFIX}"
    )

    message(STATUS "Support for Tcl/Tk front end - Ready (${TCLTK_PREFIX})")

endmacro()
