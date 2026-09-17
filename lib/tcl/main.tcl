# The main window.
#
# This is the first script the Tcl/Tk front end loads, and for now it is the
# only one.  It exists mainly to establish the seam: everything about how the
# window looks and behaves belongs here, in a file that can be edited and
# reloaded, and the C side keeps only what has to be fast or has to talk to the
# game -- the per-cell drawing loop, the term hooks, the input queue.
#
# main-tcl.c sets these before sourcing, and expects a canvas called .term of
# the size they imply:
#
#   $angband(cols) $angband(rows)     the term's grid, in cells
#
# and reads these back afterwards:
#
#   $angband(cellw) $angband(cellh)   one cell, in pixels
#
# The font is created here, not in C, so that changing it is a one-line edit in
# a data file -- and the cell size is measured from whatever this creates
# rather than assumed anywhere, so a different family or size changes the look
# and never the alignment.

font create termfont -family Menlo -size 13
set angband(cellw) [font measure termfont "W"]
set angband(cellh) [font metrics termfont -linespace]

# There is deliberately no `console hide` here.  Tk decides whether to build a
# console window inside Tk_Init, long before this script runs, and building it
# is where a Finder-launched application used to hang.  main-tcl.c prevents the
# decision instead, by naming this file as the startup script beforehand.

wm title . "ZangbandTK"
wm protocol . WM_DELETE_WINDOW { angband_quit }

# Every keystroke goes to the game.  Bound on "." rather than on the canvas so
# it works whatever has focus inside the window, which will matter more once
# there are panes.
bind . <Key> { angband_key %N %s %A }

canvas .term \
    -width  [expr {$angband(cols) * $angband(cellw)}] \
    -height [expr {$angband(rows) * $angband(cellh)}] \
    -background black -highlightthickness 0 -borderwidth 0
pack .term -fill both -expand 1
