# The character sheet, as a window of its own.
#
# The first window that is not a pane (decision 16).  The main window is
# finished at five panes; everything else opens, and this is the pattern the
# rest of them follow:
#
#   - It is a Tk toplevel of native widgets, not a term.  It costs none of
#     ANGBAND_TERM_MAX's eight slots, and it is laid out by the packer rather
#     than by a character grid.
#   - It is *passive*.  The game owns the main loop, so this never asks for
#     anything and never blocks: it reads when an event says something changed.
#   - It reads through angband_player, which reads through the game.  Nothing
#     here recomputes anything -- 1.33 blows a round is the game's number, not
#     an opinion about weapons held in Tcl.
#   - Closing it does nothing to the game, and keys typed into it still play,
#     so it can be left open.

# What goes in it, and in what order.  A group is a heading and a list of
# {field label} pairs; the field names are angband_player's, which are the
# original's, which is the whole point of the naming rule.
set character(groups) {
    {"Character" 0 0 2 {
        name        "Name"
        race        "Race"
        class       "Class"
        title       "Title"
        level       "Level"
        exp         "Experience"
        exp_to_advance "Next level in"
        gold        "Gold"
        depth       "Depth"
        max_depth   "Deepest"
    }}
    {"Fighting" 0 1 1 {
        hitpoints   "Hit points"
        mana        "Mana"
        armor_class "Armour class"
        to_hit      "To hit"
        to_dam      "To damage"
        blows_per_round "Blows/round"
        shots_per_round "Shots/round"
    }}
    {"Being" 1 1 1 {
        speed       "Speed"
        infravision "Infravision"
        light       "Light"
        age         "Age"
        height      "Height"
        weight      "Weight"
        total_weight "Burden"
    }}
}

# Deliberately not here: `position` and `turn`.  angband_player has both and no
# Angband character sheet has ever shown either -- and they are the two fields
# the game changes without announcing, so a sheet carrying them would be
# visibly one step stale after every move.  Measured: the events fire during
# the move and the turn counter advances after them.

# Fields the game keeps as two numbers, and how to say them.
set character(format) {
    hitpoints   "%s / %s"
    mana        "%s / %s"
    position    "%s, %s"
}

# When to read again.  Binding the handful of events that can change what is
# shown, rather than redrawing on everything: EVENT_MAP alone fires many times
# a turn and none of them moves a stat.
set character(events) {
    STATS HP MANA AC EXPERIENCE PLAYERLEVEL PLAYERTITLE GOLD
    DUNGEONLEVEL PLAYERSPEED RACE_CLASS LIGHT STUDYSTATUS
    ENTER_GAME LEAVE_BIRTH ENTER_WORLD
}

proc character_value {values field} {
    global character

    if {![dict exists $values $field]} { return "—" }

    set v [dict get $values $field]
    if {[dict exists $character(format) $field]} {
        return [format [dict get $character(format) $field] {*}$v]
    }
    return $v
}

proc character_refresh {} {
    global character

    if {![winfo exists .character]} return

    # No character yet, or the game is between them: em-dashes rather than an
    # error dialog.  The window can be opened from the menu at any time.
    if {[catch {angband_player} values]} { set values {} }

    foreach field $character(fields) {
        set character(v,$field) [character_value $values $field]
    }

    set h ""
    if {[dict exists $values history]} { set h [dict get $values history] }
    .character.history.t configure -state normal
    .character.history.t delete 1.0 end
    .character.history.t insert end $h
    .character.history.t configure -state disabled
}

proc character_window {} {
    global character

    if {[winfo exists .character]} {
        wm deiconify .character
        raise .character
        return
    }

    toplevel .character
    wm title .character "Character"

    # Closing is closing.  The game is not told and does not care; this is why
    # the window can be opened and shut mid-fight.
    wm protocol .character WM_DELETE_WINDOW { destroy .character }

    # Keys typed here still play the game.  Without this the window would have
    # to be dismissed before the next step could be taken, which would make it
    # something to put away rather than something to leave open.
    bind .character <Key> { angband_key %N %s %A }

    set body [ttk::frame .character.body -padding 8]
    pack $body -fill both -expand 1

    set character(fields) {}
    set col 0
    foreach group $character(groups) {
        lassign $group heading grow gcol gspan pairs
        set f [ttk::labelframe $body.g$col -text $heading -padding {8 4}]
        grid $f -row $grow -column $gcol -rowspan $gspan \
            -sticky nsew -padx 4 -pady 4
        set row 0
        foreach {field label} $pairs {
            lappend character(fields) $field
            set character(v,$field) "—"
            ttk::label $f.l$field -text "$label:" -anchor e
            ttk::label $f.v$field -textvariable character(v,$field) -anchor w
            grid $f.l$field -row $row -column 0 -sticky e -padx {0 8}
            grid $f.v$field -row $row -column 1 -sticky w
            incr row
        }
        grid columnconfigure $f 1 -weight 1
        incr col
    }
    grid columnconfigure $body 0 -weight 1
    grid columnconfigure $body 1 -weight 1

    # The history is prose, so it gets a text widget and wraps.  Read-only:
    # the game owns it, and a sheet you can type into is a sheet that lies.
    set h [ttk::labelframe .character.history -text "History" -padding {8 4}]
    pack $h -fill both -expand 1 -padx 12 -pady {0 12}
    text $h.t -height 4 -wrap word -relief flat -borderwidth 0 \
        -highlightthickness 0 -background [ttk::style lookup TFrame -background]
    pack $h.t -fill both -expand 1
    $h.t configure -state disabled

    character_refresh
}

# The window stays up to date whether or not anyone is looking at it -- the
# bindings are on "." and cost nothing while it is closed, because
# character_refresh returns immediately when there is no window.
foreach event $character(events) {
    bind . <<Angband_$event>> {+ character_refresh }
}
