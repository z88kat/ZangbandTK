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
set overhead [termpane .pw.top    overhead 32 24]
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
# The two panes the front end itself reaches for: the map, which the pointer
# hovers over, and the minimap, which is dragged and carries the status bar.
set angband(map)     $map.c
set angband(minimap) $overhead.c

# The status bar, in the minimap's frame and under its canvas.
#
# -before matters.  termpane already packed the canvas with -expand 1, so it
# has claimed the whole frame; a label packed after it would be allocated what
# is left, which is nothing.  Putting it earlier in the packing order gives it
# its row first and the canvas expands into the rest.
ttk::label $overhead.status -textvariable angband(status) -anchor w \
    -padding {4 2} -font TkSmallCaptionFont
pack $overhead.status -side bottom -fill x -before $overhead.c

set angband(terms) [list \
    [list $map.c      map       termfont] \
    [list $messages.c messages  termfont] \
    [list $recall.c   recall    termfont] \
    [list $choice.c   inventory termfont] \
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

# The View menu: what the minimap pane shows.
#
# Left to itself the pane guesses -- the level while the player is in a town or
# below ground, the world out on the road -- and the guess is worth having
# because it is right most of the time.  It is only a guess though, so it is a
# default and not a rule, and this is where it gets overruled.
menu .menubar.view -tearoff 0
.menubar add cascade -label "View" -menu .menubar.view

set angband(minimapshow) [angband_minimap show]
foreach {value label} {auto "Minimap: follow where I am"
                       level "Minimap: this level"
                       world "Minimap: the known world"} {
    .menubar.view add radiobutton -label $label \
        -variable angband(minimapshow) -value $value \
        -command [list angband_minimap show $value]
}

.menubar.view add separator
.menubar.view add command -label "Centre the minimap on me" \
    -command { angband_minimap centre }

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

# ---------------------------------------------------------------------------
# The minimap: what it shows, and dragging it about
# ---------------------------------------------------------------------------
#
# The pane carries no subwindow flag, so the game does not draw it; main-tcl.c
# does, and it decides between the world map out of doors and the scaled level
# below ground.  The origin is the front end's, which is what makes a drag
# possible -- nothing else writes to the pane to undo it.
#
# The drag is in blocks, not pixels.  One cell of this pane is one block of the
# world, so the conversion is the pane's own cell size, which is known here and
# not in C.  Whole cells only: a drag of three pixels should move nothing, and
# rounding each event separately would make a slow drag move nothing at all, so
# the anchor stays put and the remainder is carried.

set angband(dragx) 0
set angband(dragy) 0

proc minimap_press {x y} {
    global angband
    set angband(dragx) $x
    set angband(dragy) $y
    $angband(minimap) configure -cursor fleur
}

proc minimap_release {} {
    global angband
    $angband(minimap) configure -cursor {}
}

proc minimap_drag {x y} {
    global angband
    set cw [font measure minimapfont "W"]
    set ch [font metrics minimapfont -linespace]

    set dx [expr {($x - $angband(dragx)) / $cw}]
    set dy [expr {($y - $angband(dragy)) / $ch}]

    if {$dx == 0 && $dy == 0} return

    # Dragging right pulls the world right, which means looking further left.
    angband_minimap pan [expr {-$dx}] [expr {-$dy}]

    # Move the anchor by what was used, not to where the pointer is: the
    # leftover pixels belong to the next event.
    incr angband(dragx) [expr {$dx * $cw}]
    incr angband(dragy) [expr {$dy * $ch}]
}

bind $angband(minimap) <ButtonPress-1>   { minimap_press %x %y }
bind $angband(minimap) <B1-Motion>       { minimap_drag  %x %y }
bind $angband(minimap) <ButtonRelease-1> { minimap_release }
bind $angband(minimap) <Double-Button-1> { angband_minimap centre }

# ---------------------------------------------------------------------------
# The status bar: what is under the pointer
# ---------------------------------------------------------------------------
#
# Hovering a tile on the map names it here.  It is the quickest way to learn a
# tile set -- a sprite you cannot read is two seconds of hovering rather than a
# look command and a prompt -- and it costs the game nothing, because
# angband_describe answers from what the character knows and not from what is
# there.
#
# A label rather than the bottom row of the minimap term: the pane is thirty
# cells wide at this font and a monster's name is longer than that, so the
# status bar wants the full width of its frame and a readable size, and it
# should not eat a row of map to get them.

proc map_hover {x y} {
    global angband
    set col [expr {$x / $angband(cellw)}]
    set row [expr {$y / $angband(cellh)}]

    if {$col == $angband(hovercol) && $row == $angband(hoverrow)} return

    set angband(hovercol) $col
    set angband(hoverrow) $row
    set angband(status) [angband_describe $col $row]
}

proc map_unhover {} {
    global angband
    set angband(hovercol) -1
    set angband(hoverrow) -1
    set angband(status) ""
}

set angband(status) ""
map_unhover

bind $angband(map) <Motion> { map_hover %x %y }
bind $angband(map) <Leave>  { map_unhover }
