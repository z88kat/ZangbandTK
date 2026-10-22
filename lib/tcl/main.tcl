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
# The fonts are created here, not in C, and the cell size is measured from
# whatever this creates rather than assumed anywhere, so a different family or
# size changes the look and never the alignment.
#
# Three of them, from the Display page (display.tcl) by way of the front end's
# settings, which have to be read first: termfont for the map, panefont for the
# text panes beside it, and minimapfont, a smaller one, for the overhead map.
# A pane's cell size comes from its font, and the overhead map wants small
# cells: more of them fit, so more of the level fits, and the tiles drawn into
# them are correspondingly smaller.  That is what makes it a map of the level
# rather than a second view of the corner the player is standing in.
source [file join [file dirname [info script]] settings.tcl]
source [file join [file dirname [info script]] display.tcl]
display_fonts_create
# And the windows' own text, at the size kept last time, before any window
# asks for a font.
display_uisize_start

# The tile set and its size, for main-tcl.c to load once this has run; and
# the colours, before anything is drawn in them.
set angband(graphics) [settings_get display,tileset 7]
set angband(tilescale) [settings_get display,tilescale [display_tilescale_default]]
display_palette_start

set angband(cellw) [font measure termfont "W"]
set angband(cellh) [font metrics termfont -linespace]

# There is deliberately no `console hide` here.  Tk decides whether to build a
# console window inside Tk_Init, long before this script runs, and building it
# is where a Finder-launched application used to hang.  main-tcl.c prevents the
# decision instead, by naming this file as the startup script beforehand.

wm title . "ZangbandTK"

# The game's icon on every window, for a Linux desktop's dock and task bar.
# They show the window's own icon when they cannot match it to an installed
# desktop file, and an AppImage run from Downloads has none installed, so
# without this the dock showed a blank.  Only on X11: the Mac's Dock takes the
# icon from the application bundle.
if {[tk windowingsystem] eq "x11"} {
    set icon [file join [file dirname [file dirname [info script]]] \
        icons zangband.png]
    if {[file readable $icon]} {
        wm iconphoto . -default [image create photo -file $icon]
    }
    unset icon
}

# Closing the window, and Quit from the application menu, save the character
# and end the game, from wherever the game is -- angband_quit in main-tcl.c.
# Without ::tk::mac::Quit, Tk's own Quit ends the process outright and nothing
# is saved.  Only on a Mac: the ::tk::mac namespace exists nowhere else, and
# defining a procedure in it stops this file on Windows and X11.
wm protocol . WM_DELETE_WINDOW { angband_quit }
if {[tk windowingsystem] eq "aqua"} {
    proc ::tk::mac::Quit {} { angband_quit }
}

# Tk's classic look, in every window: decision 18 in the Phase 3 plan.
#
# The windows are meant to look like Tk on X11 in the 1990s: Motif relief, a
# grey ground, diamond radio buttons and bold Helvetica.  That is Tk's own
# `classic` theme, which ships with Tk and draws itself the same way on every
# platform.  Left alone, Tk on a Mac draws its controls as Aqua ones instead.
# It applies to every Ttk widget in the program, the sashes between the panes
# below included.
ttk::style theme use classic
ttk::style configure . -font [uifont 12 bold]

# No menu tears off.  Tk gives every menu a dashed tear-off line on Windows and
# X11 unless it is told not to, and the Mac, which ignores the option, never
# showed that most of the windows' menus did not say so.
option add *Menu.tearOff 0

# Every keystroke goes to the game.  Bound on "." rather than on the canvas so
# it works whatever has focus inside the window, which will matter more once
# there are panes.
#
# And only to the game: the break stops the bindings Tk puts on "all", which
# come after this one.  On X11 and Windows those walk the menu bar on F10 and
# on Alt and a key, so F10 went to the game and opened the menus too, and the
# menus then took the keys after it.  Windows does the same when Alt alone is
# let go, which is a key release, hence the second binding.  The Mac binds
# none of these, so nothing changes there; in this window the menus are the
# mouse's and the keys are the game's.
bind . <Key> { angband_key [key_sym %N %k] %s %A ; break }
bind . <KeyRelease> break

# The keysym every binding hands the game, given Tk's keysym and key code.
#
# On Windows Tk reports the keypad's digits and operators, with Num Lock on, as
# the characters they type -- its key table has no keypad -- so a keypad 1 was
# a 1, and pref.prf's keymaps, which walk on the keypad, never matched.  The
# key code there is Windows' own virtual-key code, which does tell them apart,
# so the keypad's keysym is put back from it.  Elsewhere Tk already reports
# the keypad as the keypad.
proc key_sym {keysym keycode} {
    if {[tk windowingsystem] ne "win32"} { return $keysym }
    if {$keycode >= 0x60 && $keycode <= 0x69} {
        # VK_NUMPAD0 to VK_NUMPAD9: KP_0 to KP_9.
        return [expr {0xFFB0 + $keycode - 0x60}]
    }
    # VK_MULTIPLY, VK_ADD, VK_SUBTRACT, VK_DECIMAL and VK_DIVIDE.
    set ops {0x6A 0xFFAA 0x6B 0xFFAB 0x6D 0xFFAD 0x6E 0xFFAE 0x6F 0xFFAF}
    foreach {vk kp} $ops {
        if {$keycode == $vk} { return [expr {$kp}] }
    }
    return $keysym
}

# Except F1, which opens the manual on playing, as F1 opens the manual where it
# covers each of the other windows; the game's own ? opens it at the contents.
# The function keys are game keys too, which a keymap can be put on, and a
# player who puts one on F1 has chosen what F1 does: the game has it then.
bind . <F1> { main_f1 %N %s %A ; break }

proc main_f1 {keysym state char} {
    if {[main_f1_bound $keysym $state $char]} {
        angband_key $keysym $state $char
    } else {
        help_window [help_first playing#playing-the-game playing]
    }
}

# Whether the player has a keymap of their own on this F1 -- with or without
# Shift or Control, which are other keys -- in the keyset in use.
proc main_f1_bound {keysym state char} {
    if {[catch {angband_keymap trigger $keysym $state $char} trigger]
            || $trigger eq "" || [catch {angband_keymap list} maps]} {
        return 0
    }
    # -exact: a trigger such as [F1] is a glob pattern otherwise, "F or 1".
    set row [lsearch -exact -inline -index 0 $maps $trigger]
    expr {$row ne "" && [lindex $row 2]}
}

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
    [list $messages.c messages  panefont] \
    [list $recall.c   recall    panefont] \
    [list $overhead.c minimap   minimapfont]]

# The menu bar.  The game's menus -- File, Inventory, Action, Other -- are
# commands.tcl's, built from the game's command table once there is one; these
# are the front end's own, Window and Help, which come after them.
menu .menubar
. configure -menu .menubar

# The tile sets, which Other > Preferences > Tiles lists.
#
# The list comes from the game -- whatever lib/tiles/list.txt declared and the
# files for it are present -- so a set added there appears without anyone
# editing this file.  Choosing one goes through reset_visuals and a redraw, so
# what you see after choosing is what you get: the preview is the real thing,
# which is the house rule (OBS-29) and cheaper than a mock-up besides.
set angband(tileset) [angband_tileset]
proc angband_tiles_menu {m} {
    foreach pair [angband_tilesets] {
        lassign $pair id name
        $m add radiobutton -label $name -variable angband(tileset) -value $id \
            -command [list angband_choose_tileset $id]
    }
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
.menubar.views add command -label "Pets" -command pet_window
.menubar.views add command -label "Spell Book" -command book_window
.menubar.views add command -label "Options" -command options_window
.menubar.views add command -label "Hall of Fame" -command fame_window
.menubar.views add separator
.menubar.views add command -label "Console" -command console_window
.menubar.views add command -label "Errors" -command errorsink_window

# Help, last, as on every Mac.  Not .menubar.help, for the reason above: Tk
# would hand it to macOS, which adds a search field this game has no use for.
menu .menubar.about -tearoff 0
.menubar add cascade -label "Help" -menu .menubar.about
.menubar.about add command -label "Tips…" -command tips_window
.menubar.about add separator
.menubar.about add command -label "About ZangbandTK…" -command about_window

# About, in the Help menu and in the application menu -- which on the Mac
# calls tkAboutDialog when there is one, and otherwise shows Tk's own.
proc about_window {} {
    if {[winfo exists .about]} {
        raise .about
        return
    }
    toplevel .about
    wm title .about "About ZangbandTK"
    set w .about.f
    ttk::frame $w -padding 16
    pack $w -fill both -expand 1
    ttk::label $w.name -text "ZangbandTK" -font [uifont Times 24 bold]
    ttk::label $w.version -text [angband_version]
    ttk::label $w.about -wraplength 380 -justify center -text "Zangband's\
        world on the Angband 4.2 engine, played through a Tk front end after\
        the original ZangbandTk."
    ttk::label $w.licence -wraplength 380 -justify center -text "Under the\
        Angband licence: free to copy and share for educational, research and\
        not-for-profit purposes.  Built on Angband 4.2 and on Zangband."
    ttk::button $w.ok -text OK -width 10 -default active \
        -command {destroy .about}
    pack $w.name $w.version -pady 2
    pack $w.about $w.licence -pady {10 0}
    pack $w.ok -pady {16 0}
    bind .about <Return> {destroy .about}
    bind .about <Escape> {destroy .about}
    focus $w.ok
}
proc tkAboutDialog {} { about_window }

# The debug pair first, before the windows, for the reason T3's exit
# criterion puts them first: the debug loop has to exist before the UI does.
# errorInfo.tcl installs the background error handler at source time, so an
# error raised while any of the files below are being sourced -- or at any
# point afterwards, in a binding or an `after` script -- lands somewhere a
# person can read instead of on the stderr Tk has already closed.
source [file join [file dirname [info script]] errorInfo.tcl]
source [file join [file dirname [info script]] debug.tcl]
source [file join [file dirname [info script]] accel.tcl]
source [file join [file dirname [info script]] portrait.tcl]

# Then the windows, which are Tk's own widgets in Tk's classic theme and
# nothing else (decision 18).
source [file join [file dirname [info script]] character.tcl]
source [file join [file dirname [info script]] flags.tcl]
source [file join [file dirname [info script]] knowledge.tcl]
source [file join [file dirname [info script]] pet.tcl]
source [file join [file dirname [info script]] items.tcl]
source [file join [file dirname [info script]] book.tcl]
source [file join [file dirname [info script]] spellmenu.tcl]
source [file join [file dirname [info script]] store.tcl]
source [file join [file dirname [info script]] building.tcl]
source [file join [file dirname [info script]] progress.tcl]
source [file join [file dirname [info script]] birth.tcl]
source [file join [file dirname [info script]] choose.tcl]
source [file join [file dirname [info script]] tomb.tcl]
source [file join [file dirname [info script]] tips.tcl]
source [file join [file dirname [info script]] help.tcl]
source [file join [file dirname [info script]] record.tcl]
source [file join [file dirname [info script]] fame.tcl]
source [file join [file dirname [info script]] options.tcl]
source [file join [file dirname [info script]] sound.tcl]
source [file join [file dirname [info script]] music.tcl]
source [file join [file dirname [info script]] keymap.tcl]
source [file join [file dirname [info script]] choice.tcl]
source [file join [file dirname [info script]] commands.tcl]

# Change the tile set, and remember it.  1 if it changed.  A set chosen here is
# the player's own choice, which the colour-blind palette then leaves alone
# (display.tcl); the palette marks its own choice again after calling this.
proc angband_choose_tileset {id} {
    global angband
    if {[catch {angband_tileset $id} err]} {
        tk_messageBox -icon error -title "ZangbandTK" \
            -message "Could not use that tile set." -detail $err
        # Put the tick back on the set that is actually in use.
        set angband(tileset) [angband_tileset]
        return 0
    }
    set angband(tileset) $id
    settings_set display,tileset $id
    settings_set display,pairedtiles 0
    # The windows that show a tile of their own -- the Character window's
    # portrait, the tombstone -- draw it again from the new set.
    event generate . <<TilesChanged>> -when tail
    return 1
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
