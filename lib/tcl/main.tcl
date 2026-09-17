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

# The layout.
#
# One window with panels inside it, rather than the original's six independent
# toplevels -- decision 1 in the Phase 3 plan, and confirmed since: no show/hide
# toggles, no Window menu, no saved visibility.  The panels are always there.
#
# Arranged after the original: the map takes the bulk, the overhead view sits
# beside it on the right, and the three text panels run along the bottom.  What
# goes in each of the small ones is the game's choice, not ours -- textui_init()
# assigns messages, inventory, monster list, item list, recall and overhead to
# subwindows 1 to 6 in that order, and these are simply terms 1, 5, 2 and 6.
#
# Two of the original's panels are not here and cannot be yet: Progress (the
# HP/SP/Food bars) and the graphical Micro Map are native widgets rather than
# term views, so they arrive with T4 and T5.  The overhead pane stands in for
# the Micro Map meanwhile.

proc termpane {parent name cols rows} {
    # A term is a black canvas and nothing else.  The frame around it is what
    # the paned window resizes; the canvas fills it.
    #
    # The requested size matters more than it looks.  A Tk canvas asks for
    # 378x302 unless told otherwise, so without this every pane asks for the
    # same thing and the paned window shares the window out equally -- leaving
    # the map with about 62 columns, while the map term has a floor of 80 and
    # quietly draws the other 18 off the edge of its own canvas.  Asking in
    # cells is also the only size that means anything here.
    global angband
    set f [ttk::frame $parent.$name]
    canvas $f.c -background black -highlightthickness 0 -borderwidth 0 \
        -width  [expr {$cols * $angband(cellw)}] \
        -height [expr {$rows * $angband(cellh)}]
    pack $f.c -fill both -expand 1
    return $f
}

ttk::panedwindow .pw -orient vertical
ttk::panedwindow .pw.top -orient horizontal
ttk::panedwindow .pw.bottom -orient horizontal

# The map gets the game's floor of 80x24 and the rest ask for what suits them.
set map      [termpane .pw.top    map      80 24]
set overhead [termpane .pw.top    overhead 28 24]
set messages [termpane .pw.bottom messages 44 10]
set recall   [termpane .pw.bottom recall   44 10]
set choice   [termpane .pw.bottom choice   44 10]

.pw.top    add $map      -weight 4
.pw.top    add $overhead -weight 1
.pw.bottom add $messages -weight 2
.pw.bottom add $recall   -weight 2
.pw.bottom add $choice   -weight 2
.pw add .pw.top    -weight 4
.pw add .pw.bottom -weight 1
pack .pw -fill both -expand 1

# Let the requested sizes above decide how big the window starts: they already
# add up to a map at its floor with the other panes beside and below it.
wm minsize . [expr {80 * $angband(cellw)}] [expr {24 * $angband(cellh)}]

# Geometry has to be real before C measures these canvases: an unmapped widget
# reports 1x1, and every term would come out one cell wide.
update idletasks
update

# The order is the contract with main-tcl.c: element 0 is the map, and the rest
# become the game's subwindows 1, 2, 3... in the order given here.  Terms 1, 2
# and 5 are messages, inventory and recall; 6 is the overhead map, so the list
# is padded to put each pane on the subwindow whose content it wants.
set angband(terms) [list \
    $map.c \
    $messages.c \
    $choice.c \
    $recall.c \
    $overhead.c]

# Each pane follows its own size.  <Configure> fires for every pixel of a drag,
# so the work is deferred to idle and coalesced -- only the last size in a
# burst is acted on -- and rebuilding a couple of thousand canvas items per
# pixel would be unusable.
set angband(resizePending) 0

proc angband_resize_now {} {
    global angband
    set angband(resizePending) 0
    set i 0
    foreach c $angband(terms) {
        if {[winfo exists $c]} {
            set cols [expr {[winfo width  $c] / $angband(cellw)}]
            set rows [expr {[winfo height $c] / $angband(cellh)}]
            if {$cols > 0 && $rows > 0} {
                angband_resize $i $cols $rows
            }
        }
        incr i
    }
}

foreach c $angband(terms) {
    bind $c <Configure> {
        if {!$angband(resizePending)} {
            set angband(resizePending) 1
            after idle angband_resize_now
        }
    }
}
