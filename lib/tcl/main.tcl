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

# A second, smaller font for the minimap.  A pane's cell size comes from its
# font, and the minimap wants small cells: more of them fit, so more of the
# level fits, and the tiles drawn into them are correspondingly smaller.  That
# is what makes it a map of the level rather than a second view of the corner
# the player is standing in.
font create minimapfont -family Menlo -size 7

set angband(cellw) [font measure termfont "W"]
set angband(cellh) [font metrics termfont -linespace]

# There is deliberately no `console hide` here.  Tk decides whether to build a
# console window inside Tk_Init, long before this script runs, and building it
# is where a Finder-launched application used to hang.  main-tcl.c prevents the
# decision instead, by naming this file as the startup script beforehand.

wm title . "ZangbandTK"

# Closing the window, and Quit from the application menu, save the character
# and end the game, from wherever the game is -- angband_quit in main-tcl.c.
# Without ::tk::mac::Quit, Tk's own Quit ends the process outright and nothing
# is saved.
wm protocol . WM_DELETE_WINDOW { angband_quit }
proc ::tk::mac::Quit {} { angband_quit }

# Tk's classic look, in every window: decision 18 in the Phase 3 plan.
#
# The windows are meant to look like Tk on X11 in the 1990s: Motif relief, a
# grey ground, diamond radio buttons and bold Helvetica.  That is Tk's own
# `classic` theme, which ships with Tk and draws itself the same way on every
# platform.  Left alone, Tk on a Mac draws its controls as Aqua ones instead.
# It applies to every Ttk widget in the program, the sashes between the panes
# below included.
ttk::style theme use classic
ttk::style configure . -font {Helvetica 12 bold}

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
# The original's Progress window -- the HP, SP and food bars, and here
# experience too -- sits under the overhead view, as it sat under the original's
# Micro Map: a strip of Tk's own widgets rather than a term, filled in by
# lib/tcl/progress.tcl.  The graphical Micro Map is still to come (T5), and the
# overhead pane stands in for it meanwhile.

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
ttk::panedwindow .pw.top.side -orient vertical
ttk::panedwindow .pw.bottom -orient horizontal

# The map gets the game's floor of 80x24 and the rest ask for what suits them.
# The overhead view and the Progress strip share the column beside the map,
# and between them come to the map's height.
set map      [termpane .pw.top      map      80 24]
set overhead [termpane .pw.top.side overhead 32 19]
set messages [termpane .pw.bottom messages 44 10]
set recall   [termpane .pw.bottom recall   44 10]

# The Choice pane is not a term: it is lib/tcl/choice.tcl's, a list that can
# be clicked to answer the game's item prompts.  Its frame is made here, at a
# term's size, so that it takes its place in the layout with the others; the
# list goes into it when choice.tcl is sourced below.
set choice [ttk::frame .pw.bottom.choice \
    -width [expr {44 * $angband(cellw)}] -height [expr {10 * $angband(cellh)}]]

# The Progress strip likewise: a frame here, its bars from progress.tcl.
ttk::style configure Progress.TFrame -background black
set strip [ttk::frame .pw.top.side.progress -style Progress.TFrame \
    -width [expr {32 * $angband(cellw)}] -height [expr {5 * $angband(cellh)}]]

.pw.top.side add $overhead -weight 1
.pw.top.side add $strip -weight 0
.pw.top    add $map      -weight 4
.pw.top    add .pw.top.side -weight 1
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

# The contract with main-tcl.c: one {canvas role} pair per term, the map first.
#
# The role is what the pane is *for*, and the C side turns it into the game's
# window flags.  Saying it here rather than relying on position is the whole
# point: textui_init() assigns content to subwindows by index -- 1 messages,
# 2 inventory, 3 monster list, 4 item list, 5 recall, 6 overhead -- so a layout
# with five panes silently got the first five of that list, and the overhead
# map, which is index 6, simply never appeared.  Naming the role means the
# arrangement and the content stop being the same decision.
# Each entry is {canvas role font}.  The font decides the pane's cell size, so
# it is how a pane ends up denser or coarser than its neighbours.
set angband(terms) [list \
    [list $map.c      map       termfont] \
    [list $messages.c messages  termfont] \
    [list $recall.c   recall    termfont] \
    [list $overhead.c minimap   minimapfont]]

# The menu bar, which for now carries exactly one menu: the tile sets.
#
# This is not the menu bar T7 builds.  That one is generated from the game's
# own command table so that it stays in step with the keyboard; this is front
# end configuration, which the game has no commands for, and the two will sit
# side by side rather than one replacing the other.
#
# The list comes from the game -- whatever lib/tiles/list.txt declared and the
# files for it are present -- so a set added there appears here without anyone
# editing this file.  Choosing one goes through reset_visuals and a redraw, so
# what you see after choosing is what you get: the preview is the real thing,
# which is the house rule (OBS-29) and cheaper than a mock-up besides.
menu .menubar
. configure -menu .menubar

menu .menubar.tiles -tearoff 0
.menubar add cascade -label "Tiles" -menu .menubar.tiles

set angband(tileset) [angband_tileset]
foreach pair [angband_tilesets] {
    lassign $pair id name
    .menubar.tiles add radiobutton -label $name \
        -variable angband(tileset) -value $id \
        -command [list angband_choose_tileset $id]
}

# The Window menu: the things that are not panes.
#
# Decision 16 finished the main window at its panes -- five, and the Progress
# strip under the overhead view -- so everything else opens as a window of its
# own and this is where they are opened from.  No
# accelerators: every key belongs to the game.
# The path must not be .menubar.window.  On macOS Tk treats three menu names
# as the platform's own -- .menubar.apple, .menubar.window and .menubar.help --
# and fills them in: a menu at .menubar.window arrives with Minimize, Zoom,
# Fill, Center, Full Screen Tile, the tab-bar commands and Bring All to Front
# already in it, none of which this game has any use for.  Same label, ordinary
# path, no junk.
menu .menubar.views -tearoff 0
.menubar add cascade -label "Window" -menu .menubar.views
.menubar.views add command -label "Character" -command character_window
.menubar.views add command -label "Knowledge" -command knowledge_window
.menubar.views add command -label "Items" -command items_window
.menubar.views add command -label "Spell Book" -command book_window
.menubar.views add command -label "Options" -command options_window
.menubar.views add separator
.menubar.views add command -label "Console" -command console_window
.menubar.views add command -label "Errors" -command errorsink_window

# The debug pair first, before the windows, for the reason T3's exit
# criterion puts them first: the debug loop has to exist before the UI does.
# errorInfo.tcl installs the background error handler at source time, so an
# error raised while any of the files below are being sourced -- or at any
# point afterwards, in a binding or an `after` script -- lands somewhere a
# person can read instead of on the stderr Tk has already closed.
source [file join [file dirname [info script]] errorInfo.tcl]
source [file join [file dirname [info script]] debug.tcl]

# Then the windows, which are Tk's own widgets in Tk's classic theme and
# nothing else (decision 18).
source [file join [file dirname [info script]] character.tcl]
source [file join [file dirname [info script]] knowledge.tcl]
source [file join [file dirname [info script]] items.tcl]
source [file join [file dirname [info script]] book.tcl]
source [file join [file dirname [info script]] store.tcl]
source [file join [file dirname [info script]] building.tcl]
source [file join [file dirname [info script]] progress.tcl]
source [file join [file dirname [info script]] birth.tcl]
source [file join [file dirname [info script]] choose.tcl]
source [file join [file dirname [info script]] tomb.tcl]
source [file join [file dirname [info script]] options.tcl]
source [file join [file dirname [info script]] choice.tcl]
source [file join [file dirname [info script]] commands.tcl]

proc angband_choose_tileset {id} {
    global angband
    if {[catch {angband_tileset $id} err]} {
        tk_messageBox -icon error -title "ZangbandTK" \
            -message "Could not use that tile set." -detail $err
        # Put the tick back on the set that is actually in use.
        set angband(tileset) [angband_tileset]
    }
}

# Each pane follows its own size.  <Configure> fires for every pixel of a drag,
# so the work is deferred to idle and coalesced -- only the last size in a
# burst is acted on -- and rebuilding a couple of thousand canvas items per
# pixel would be unusable.
set angband(resizePending) 0

proc angband_resize_now {} {
    global angband
    set angband(resizePending) 0
    set i 0
    foreach pair $angband(terms) {
        set c [lindex $pair 0]
        set f [lindex $pair 2]
        if {[winfo exists $c]} {
            set cw [font measure $f "W"]
            set ch [font metrics $f -linespace]
            set cols [expr {[winfo width  $c] / $cw}]
            set rows [expr {[winfo height $c] / $ch}]
            if {$cols > 0 && $rows > 0} {
                angband_resize $i $cols $rows
            }
        }
        incr i
    }
}

foreach pair $angband(terms) {
    bind [lindex $pair 0] <Configure> {
        if {!$angband(resizePending)} {
            set angband(resizePending) 1
            after idle angband_resize_now
        }
    }
}
