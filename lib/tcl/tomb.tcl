# The Tomb window.
#
# T9: the death screen, as the original's Tomb window had it -- the tombstone
# in white on black, and what can still be done with the character beside it.
#
# The tombstone is the game's own: the screen death_screen() in ui-death.c
# draws on the main window, read back with angband_death tomb, so a
# retirement brings its own scene and whatever lib/screens says comes with it.
# The buttons are the game's death menu, by its names and keys, and most of
# them are run by the game as they are from that menu: the messages, the
# history and the rest come up on the main window, the window stepping aside
# until the game asks again.  The file dump is this front end's own save
# dialog, as the Character window's is; View scores is the Hall of Fame; and
# New Game and Quit are confirmed here rather than on the main screen, and
# offer a Character Record on the way out (record.tcl).
#
# Built from Tk's own widgets in the classic theme (decision 18); the black of
# the tombstone is the black of the panes.

set tomb(msg) ""

# The game is asking: show the window, or bring it back.
proc tomb_show {} {
    if {[catch {angband_death} here] || ![llength $here]} return
    lassign $here how actions
    if {$how eq "crown"} {
        tomb_kingly
        return
    }
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

    tomb_portrait $rows $pad $ch
}

# The character's own tile on the stone, which the text screen cannot have and
# the original's Tomb window did (OBS-46).  In the gap the game's screens leave
# between the top of the stone and the name: rows 4 to 6, in the middle of the
# columns death_screen() centres its words in, 8 to 39.  Only where that gap
# is empty, so a screen drawn differently loses the tile rather than its words.
proc tomb_portrait {{rows ""} {pad 14} {ch 0}} {
    set c .tomb.f.top.c
    if {![winfo exists $c]} return
    $c delete portrait
    if {$rows eq "" && [catch {angband_death tomb} rows]} return
    if {!$ch} { set ch [font metrics termfont -linespace] }
    foreach r {4 5 6} {
        if {[string trim [string range [lindex $rows $r] 16 31]] ne ""} return
    }
    # The image is not called tomb_portrait: an image is a command too.
    if {"tomb_tile" ni [image names]} { image create photo tomb_tile }
    set size [expr {int($ch * 2.6)}]
    if {[catch {angband_tile player tomb_tile $size} ok] || !$ok} return
    set cw [expr {[font measure termfont [string repeat 0 40]] / 40.0}]
    $c create image [expr {$pad + 23.5 * $cw}] [expr {$pad + 5.5 * $ch}] \
        -image tomb_tile -anchor center -tags portrait
}

# The winner's crown, before the tombstone, as the original's Kingly window
# had it: the game's own screen (crown.txt and its words), read back like the
# stone, the crown in gold and the words in white, and Continue.
proc tomb_kingly {} {
    if {![winfo exists .kingly]} {
        toplevel .kingly
        wm title .kingly "Zangband: Winner!"
        wm protocol .kingly WM_DELETE_WINDOW tomb_kingly_done
        set w .kingly.f
        ttk::frame $w -padding 10
        pack $w -fill both -expand 1
        canvas $w.c -background black -highlightthickness 0 \
            -relief sunken -borderwidth 2
        pack $w.c
        ttk::button $w.go -text Continue -width 11 -default active \
            -command tomb_kingly_done
        pack $w.go -pady {10 0}
        bind .kingly <Return> tomb_kingly_done
        bind .kingly <Escape> tomb_kingly_done
    }

    set c .kingly.f.c
    $c delete all
    if {[catch {angband_death tomb} rows]} { set rows {} }

    # The game centres the crown on the main screen, however wide that is;
    # here it is the window's own, so the margin it shares goes, and so do
    # the blank rows above it.
    while {[llength $rows] && [string trim [lindex $rows 0]] eq ""} {
        set rows [lrange $rows 1 end]
    }
    set margin 9999
    foreach row $rows {
        if {[string trim $row] eq ""} continue
        regexp {^ *} $row lead
        set margin [expr {min($margin, [string length $lead])}]
    }
    if {$margin == 9999} { set margin 0 }
    set rows [lmap row $rows { string range $row $margin end }]
    set ch [font metrics termfont -linespace]
    set pad 14
    set wide 0
    set y $pad
    foreach row $rows {
        set fill [expr {[regexp {[A-Za-z]{3}} $row] ? "white" : "#ffd040"}]
        $c create text $pad $y -text $row -anchor nw -font termfont -fill $fill
        set wide [expr {max($wide, [font measure termfont $row])}]
        incr y $ch
    }
    $c configure -width [expr {$wide + 2 * $pad}] \
        -height [expr {[llength $rows] * $ch + 2 * $pad}]
    raise .kingly
    focus .kingly.f.go
}

proc tomb_kingly_done {} {
    destroy .kingly
    catch {angband_death answer continue}
}

# --- answering ------------------------------------------------------------------

proc tomb_act {key} {
    global choose

    switch -- $key {
        f {
            tomb_dump
            return
        }
        v {
            # The Hall of Fame, this character among the dead.
            fame_window
            return
        }
        n {
            if {[tk_messageBox -parent .tomb -icon question -type yesno \
                    -title "New Game" -message "Start a new game?"] ne "yes"} return
            # What to keep of this one, while there is still a character to
            # keep it of -- the original asked here too.
            record_offer
            # The chooser offers the character just finished first; a new
            # one is what was asked for.
            set choose(fresh) 1
            destroy .tomb
        }
        q {
            if {[tk_messageBox -parent .tomb -icon question -type yesno \
                    -title "Quit" -message "Do you want to quit?"] ne "yes"} return
            record_offer
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
bind . <<TilesChanged>> {+ tomb_portrait }
