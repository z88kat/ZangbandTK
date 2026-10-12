# The Tomb window.
#
# T9: the death screen, as the original's Tomb window had it -- the tombstone
# in white on black, and what can still be done with the character beside it.
#
# The tombstone is the game's own: the screen death_screen() in ui-death.c
# draws on the main window, read back with angband_death tomb, so a
# retirement brings its own scene and whatever lib/screens says comes with it.
# The buttons are the game's death menu, by its names and keys, and all but
# three of them are run by the game as they are from that menu: the messages,
# the scores, the history and the rest come up on the main window, the window
# stepping aside until the game asks again.  The file dump is this front end's
# own save dialog, as the Character window's is, and New Game and Quit are
# confirmed here rather than on the main screen.
#
# Built from Tk's own widgets in the classic theme (decision 18); the black of
# the tombstone is the black of the panes.

set tomb(msg) ""

# The game is asking: show the window, or bring it back.
proc tomb_show {} {
    if {[catch {angband_death} here] || ![llength $here]} return
    lassign $here how actions
    if {![winfo exists .tomb]} { tomb_window $how $actions }
    tomb_draw
    if {[wm state .tomb] eq "withdrawn"} { wm deiconify .tomb }
    raise .tomb
    focus .tomb
}

# The screen as the game drew it, a row at a time in the panes' own font.
proc tomb_draw {} {
    set c .tomb.f.top.c
    $c delete all
    if {[catch {angband_death tomb} rows]} return

    set ch [font metrics termfont -linespace]
    set wide 0
    set pad 14
    set y $pad
    foreach row $rows {
        $c create text $pad $y -text $row -anchor nw -font termfont -fill white
        set wide [expr {max($wide, [font measure termfont $row])}]
        incr y $ch
    }
    $c configure -width [expr {$wide + 2 * $pad}] \
        -height [expr {[llength $rows] * $ch + 2 * $pad}]
}

# --- answering ------------------------------------------------------------------

proc tomb_act {key} {
    global choose

    switch -- $key {
        f {
            tomb_dump
            return
        }
        n {
            if {[tk_messageBox -parent .tomb -icon question -type yesno \
                    -title "New Game" -message "Start a new game?"] ne "yes"} return
            # The chooser offers the character just finished first; a new
            # one is what was asked for.
            set choose(fresh) 1
            destroy .tomb
        }
        q {
            if {[tk_messageBox -parent .tomb -icon question -type yesno \
                    -title "Quit" -message "Do you want to quit?"] ne "yes"} return
            destroy .tomb
        }
        default {
            # The game's own screen, on the main window, which has the keys.
            wm withdraw .tomb
            focus .
        }
    }
    if {[catch {angband_death answer $key}]} bell
}

# What the Character window's Dump to File does, from here.
proc tomb_dump {} {
    global tomb

    if {[catch {angband_dump suggest} path]} {
        set tomb(msg) "There is no character to dump."
        return
    }
    set file [tk_getSaveFile -parent .tomb -title "File Dump" \
        -initialdir [file dirname $path] -initialfile [file tail $path]]
    if {$file eq ""} return

    if {[catch {angband_dump write $file} err]} {
        set tomb(msg) "Could not write the dump: $err"
        return
    }
    set tomb(msg) "Character dump written to [file tail $file]."
}

# A key from the menu chooses as it does on the main screen.
proc tomb_key {char} {
    set b .tomb.f.top.acts.b$char
    if {$char ne "" && [winfo exists $b]} { $b invoke }
}

# --- the window ----------------------------------------------------------------

proc tomb_window {how actions} {
    global tomb

    set tomb(msg) ""
    toplevel .tomb
    wm title .tomb "Zangband: [dict get {dead Tomb retired Retired winner Winner} $how]"
    wm protocol .tomb WM_DELETE_WINDOW {tomb_act q}
    # Not `wm resizable .tomb 0 0`, which the window's size would suggest:
    # Tk 9 on macOS never painted a new window made so -- it stayed white
    # until it was withdrawn and shown again.

    set w .tomb.f
    ttk::frame $w
    pack $w -fill both -expand 1

    ttk::label $w.status -textvariable tomb(msg) -font {Helvetica 12} \
        -relief sunken -borderwidth 1 -anchor w -padding {4 1}
    pack $w.status -side bottom -fill x -padx 2 -pady 2

    ttk::frame $w.top
    pack $w.top -fill both -expand 1 -padx 10 -pady 10
    canvas $w.top.c -background black -highlightthickness 0 \
        -relief sunken -borderwidth 2
    pack $w.top.c -side left -anchor n

    # The menu, where the game puts it: to the right of the stone.  New Game
    # and Quit, which end the screen, stand apart from the rest.
    set a $w.top.acts
    ttk::frame $a
    pack $a -side left -fill y -padx {10 0}
    foreach act $actions {
        lassign $act key name
        if {$key eq "n"} {
            ttk::separator $a.sep -orient horizontal
            pack $a.sep -side top -fill x -pady 8
        }
        if {$key eq "f"} { append name "…" }
        ttk::button $a.b$key -text $name -width 15 \
            -underline [string first $key [string tolower $name]] \
            -command [list tomb_act $key]
        pack $a.b$key -side top -fill x -pady 2
    }

    bind .tomb <KeyPress> { tomb_key %A }
    bind .tomb <Escape> { tomb_act q }
}

bind . <<Angband_DEATHVIEW>> {+ tomb_show }
