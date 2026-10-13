# The character sheet.
#
# The first window that is not a pane (decision 16).  Decision 18 builds it
# from Tk's own widgets in Tk's classic theme, to the handoff in
# .claude/plans/design_handoff_character_tk, and it shares its look with the
# knowledge browser because both are simply Tk: nothing here is drawn by hand.
#
# How it behaves, which is the pattern the other windows follow:
#
#   - A Tk toplevel of ordinary widgets, not a term.  It costs none of
#     ANGBAND_TERM_MAX's eight slots.
#   - Passive.  The game owns the main loop, so this never asks for anything
#     and never blocks: it reads when an event says something changed.
#   - Keys typed into it still play, so it can be left open.
#   - Closing it does nothing to the game.
#   - It opens before there is a character and shows em-dashes, because the
#     menu is always there.

set character(dash) "—"
set character(msg) ""
set character(turn) ""
set character(newname) ""

# The identity grid, two pairs to a row, in reading order.
set character(identity) {
    name  "Name"   level "Level"
    race  "Race"   gold  "Gold"
    class "Class"  place "Depth"
    title "Title"
}

# The three groups on the Info page.
set character(sections) {
    {fighting Fighting {
        armor_class     "Armour class"
        to_hit          "To hit"
        to_dam          "To damage"
        blows_per_round "Blows / round"
        shots_per_round "Shots / round"
    }}
    {being Being {
        speed        "Speed"
        infravision  "Infravision"
        light        "Light radius"
        age          "Age"
        size         "Height / weight"
        total_weight "Burden"
    }}
    {standing Standing {
        exp        "Experience"
        place      "Current depth"
        max_depth  "Deepest reached"
        title      "Title"
    }}
}

# The pages, with the letter each underlines.  Notes underlines its second,
# because N is Change Name's.  Flags is not here: the resistance grid is built
# from ui-entry.h's iterator with a column per equipment slot, and an empty
# page would be worse than a missing one.
set character(pages) {
    {info      Info      0}
    {history   History   0}
    {virtues   Virtues   0}
    {mutations Mutations 0}
    {notes     Notes     1}
}
set character(page) info

# The three bars: {name label field colour}.
set character(bars) {
    hp   "Hit points" hitpoints  #38a038
    sp   "Mana"       mana       #4060d0
    xp   "Experience" next_level #c09020
}

# When to read again.  The handful of events that can move a field, rather than
# all 66: EVENT_MAP alone fires many times a turn and none of them is a stat.
set character(events) {
    STATS HP MANA AC EXPERIENCE PLAYERLEVEL PLAYERTITLE GOLD
    DUNGEONLEVEL PLAYERSPEED RACE_CLASS LIGHT STUDYSTATUS
    ENTER_GAME LEAVE_BIRTH ENTER_WORLD
}

# --- the values --------------------------------------------------------------

# A bonus, with its sign.
#
# Not expr: "expr {$n >= 0 ? \"+$n\" : $n}" substitutes "+3" and then parses it
# straight back to the number 3, so the sign is lost between writing it and
# returning it.
proc character_signed {n} {
    if {![string is integer -strict $n]} { return $n }
    if {$n >= 0} { return "+$n" }
    return $n
}

# Everything the sheet shows, as the sheet shows it, or an empty dict when
# there is no character.
proc character_values {} {
    global character

    if {[catch {angband_player} v]} { return {} }

    dict set v size "[dict get $v height] / [dict get $v weight]"
    dict set v to_hit [character_signed [dict get $v to_hit]]
    dict set v to_dam [character_signed [dict get $v to_dam]]

    # 110 is normal speed, and the game counts from there.
    set speed [dict get $v speed]
    dict set v speed [expr {$speed == 110 ? "110 (Normal)"
        : "$speed ([character_signed [expr {$speed - 110}]])"}]

    # The game keeps infravision in tens of feet and weight in tenths of a
    # pound.
    dict set v infravision "[expr {[dict get $v infravision] * 10}] ft"
    set burden [dict get $v total_weight]
    dict set v total_weight [format "%d.%d lb" [expr {$burden / 10}] \
        [expr {$burden % 10}]]

    # Deepest in the sidebar's own form for a depth, 250' (L5), so it reads as
    # the same kind of thing as the current one beside it.
    set deepest [dict get $v max_depth]
    dict set v max_depth [expr {$deepest == 0 ? $character(dash)
        : "[expr {$deepest * 50}]' (L$deepest)"}]

    return $v
}

proc character_refresh {} {
    global character

    if {![winfo exists .character]} return
    set w .character.f

    set v [character_values]
    set have [expr {[dict size $v] > 0}]

    # Not expr, which would read "+3" back as the number 3 and lose the sign
    # character_signed put there.
    foreach field [concat $character(fields) [dict keys $character(identity)]] {
        if {$have && [dict exists $v $field]} {
            set character(v,$field) [dict get $v $field]
        } else {
            set character(v,$field) $character(dash)
        }
    }

    # The bars.  A maximum of zero is an empty bar, never a full one.
    foreach {name label field colour} $character(bars) {
        lassign [expr {$have ? [dict get $v $field] : {0 0}}] now most
        set character(v,$name) [expr {$have ? "$now / $most" : $character(dash)}]
        $w.top.cond.p$name configure -maximum [expr {$most > 0 ? $most : 1}] \
            -value [expr {$most > 0 ? min($now, $most) : 0}]
    }
    character_hp_colour $v

    character_text $w.page.history.f.t \
        [expr {$have ? [regsub -all {\s+} [string trim [dict get $v history]] " "] : ""}]
    character_refresh_virtues $have
    character_refresh_mutations $have
    character_refresh_notes $have

    set character(turn) [expr {$have ? "Turn [angband_player turn]" : ""}]
    character_portrait
}

# The character's portrait (portrait.tcl): its race and class, a man or a
# woman, on the class's backdrop.  The original's was the character's icon;
# without a portrait to draw it is still the map tile, or the "@" while the
# game is drawing letters.
proc character_portrait_image {} {
    # Not named character_portrait: an image is a command as well, and would
    # take the place of the procedure below.
    if {"character_tile" ni [image names]} {
        image create photo character_tile
    }
    return character_tile
}

proc character_portrait {} {
    set g .character.f.top.id.glyph
    if {![winfo exists $g]} return
    set tile [portrait_draw [character_portrait_image] 64]
    $g itemconfigure tile -state [expr {$tile ? "normal" : "hidden"}]
    $g itemconfigure letter -state [expr {$tile ? "hidden" : "normal"}]
}

# The game's own rule for the colour of hit points, player_hp_attr(): full,
# above the warning level the player set, or below it.  The colours are the
# bar's, because the game's yellow and red are for a black screen.
proc character_hp_colour {v} {
    global character

    set colour #38a038
    if {[dict size $v]} {
        lassign [dict get $v hitpoints] now most
        if {[catch {angband_option hitpoint_warn} warn]} { set warn 0 }
        if {$now >= $most} {
            set colour #38a038
        } elseif {$now > $most * $warn / 10} {
            set colour #c8a000
        } else {
            set colour #c03030
        }
    }

    # Only when it changes: configuring a style redraws every widget that
    # uses the theme, and this runs on every hit point lost.
    if {[info exists character(hpcolour)] && $character(hpcolour) eq $colour} return
    set character(hpcolour) $colour
    ttk::style configure HP.Horizontal.TProgressbar -background $colour
}

proc character_text {t text} {
    $t configure -state normal
    $t delete 1.0 end
    $t insert end $text
    $t configure -state disabled
    $t yview moveto 0
}

proc character_refresh_virtues {have} {
    global character

    set f .character.f.page.virtues.f
    if {[catch {angband_virtue list} rows]} { set rows {} }

    # The grid is rebuilt only when the eight themselves change, which is at
    # birth and on loading; every other refresh just rewrites the standings.
    set names {}
    foreach row $rows { lappend names [lindex $row 1] }
    if {[info exists character(virtues)] && $character(virtues) eq [list $have $names]} {
        set i 0
        set untested 1
        foreach row $rows {
            lassign $row idx name value desc line
            if {$value != 0} { set untested 0 }
            $f.v$i configure -text [character_standing $desc]
            incr i
        }
        if {[winfo exists $f.note] && !$untested} { destroy $f.note }
        return
    }
    set character(virtues) [list $have $names]
    destroy {*}[winfo children $f]

    if {![llength $rows]} {
        ttk::label $f.none -font {Helvetica 12} -wraplength 520 -justify left \
            -text [expr {$have
                ? "Nothing is recorded. A character chooses eight virtues at\
                   birth, from their race, their class and the realms they\
                   study; one made before the game kept them has none."
                : ""}]
        grid $f.none -row 0 -column 0 -sticky w
        return
    }

    set i 0
    set untested 1
    foreach row $rows {
        lassign $row idx name value desc line
        if {$value != 0} { set untested 0 }
        set r [expr {$i / 2}]
        set c [expr {($i % 2) * 2}]
        ttk::label $f.l$i -text "$name:" -font {Helvetica 12}
        ttk::label $f.v$i -text [character_standing $desc] \
            -relief sunken -borderwidth 1 \
            -anchor w -padding {4 1}
        grid $f.l$i -row $r -column $c -sticky e -pady 3 \
            -padx [expr {$c ? {18 6} : {0 6}}]
        grid $f.v$i -row $r -column [expr {$c + 1}] -sticky ew -pady 3
        incr i
    }
    grid columnconfigure $f {1 3} -weight 1 -uniform value

    if {$untested} {
        ttk::label $f.note -font {Helvetica 12} -text \
            "Your virtues are untested. What you kill, spend and spare will move them."
        grid $f.note -row [expr {($i + 1) / 2}] -column 0 -columnspan 4 \
            -sticky w -pady {12 0}
    }
}

# A virtue's standing in the game's own words.  They are written to be read as
# "You are neutral to Valour", so the trailing preposition is dropped and the
# grid can say "Valour: Neutral" and "Valour: A champion" from the same
# thirteen phrases.
proc character_standing {desc} {
    set standing [regsub { \S+$} $desc ""]
    return "[string toupper [string index $standing 0]][string range $standing 1 end]"
}

proc character_refresh_mutations {have} {
    set t .character.f.page.mutations.f.t
    if {[catch {angband_mutation list} rows]} { set rows {} }
    if {![llength $rows]} {
        character_text $t [expr {$have ? "You have no mutations." : ""}]
        return
    }
    set lines {}
    foreach row $rows {
        lassign $row idx name desc power
        lappend lines $desc
    }
    character_text $t [join $lines "\n"]
}

# The game's own record of what has happened to this character: turn, depth,
# level, event.  In columns, so the text does not wrap.
#
# This is the original's Notes page.  ZangbandTK showed Zangband's notes file
# there -- what auto_notes recorded, and what the player typed with ':' -- and
# 4.2 keeps both in the history instead, ':' included.  It was never a place to
# type notes into, and nothing here saves what is typed.
proc character_refresh_notes {have} {
    set t .character.f.page.notes.f.t
    if {[catch {angband_history list} rows]} { set rows {} }
    if {![llength $rows]} {
        character_text $t [expr {$have ? "Nothing has happened yet." : ""}]
        return
    }
    set lines {}
    foreach row $rows {
        lassign $row turn depth clev text
        lappend lines [format "%8s  %-6s  %-4s  %s" $turn \
            [expr {$depth == 0 ? "town" : "[expr {$depth * 50}] ft"}] \
            "L$clev" $text]
    }
    character_text $t [join $lines "\n"]
}

proc character_switch {} {
    global character

    if {![winfo exists .character]} return
    foreach page $character(pages) {
        set key [lindex $page 0]
        if {$key eq $character(page)} {
            grid .character.f.page.$key -row 0 -column 0 -sticky nsew
        } else {
            grid remove .character.f.page.$key
        }
    }
    character_refresh
}

# --- the commands -------------------------------------------------------------

proc character_rename {} {
    global character

    if {[catch {angband_player name} name]} {
        set character(msg) "There is no character to rename."
        return
    }

    set d .character.rename
    if {[winfo exists $d]} { raise $d ; return }

    toplevel $d
    wm title $d "Change Name"
    wm transient $d .character
    wm resizable $d 0 0

    ttk::frame $d.f -padding 12
    pack $d.f -fill both -expand 1
    ttk::label $d.f.l -text "Enter a new name for your character:"
    set character(newname) $name
    # The game's limit, PLAYER_NAME_LEN less the terminator.
    ttk::entry $d.f.e -width 20 -font {Courier 12} \
        -textvariable character(newname) \
        -validate key -validatecommand {expr {[string length %P] <= 31}}
    ttk::separator $d.f.sep -orient horizontal
    ttk::frame $d.f.b
    ttk::button $d.f.b.ok -text OK -width 8 -default active \
        -command character_rename_ok
    ttk::button $d.f.b.cancel -text Cancel -width 8 -command [list destroy $d]
    pack $d.f.b.cancel $d.f.b.ok -side right -padx {10 0}

    pack $d.f.l -anchor w
    pack $d.f.e -fill x -pady {6 10}
    pack $d.f.sep -fill x
    pack $d.f.b -fill x -pady {10 0}

    bind $d <Return> { character_rename_ok ; break }
    bind $d <Escape> { destroy .character.rename ; break }

    ::tk::PlaceWindow $d widget .character
    $d.f.e selection range 0 end
    $d.f.e icursor end
    focus $d.f.e

    # A grab on a window that is not on screen yet is refused, so wait for it.
    tkwait visibility $d
    if {[winfo exists $d]} { grab set $d }
}

proc character_rename_ok {} {
    global character

    set name [string trim $character(newname)]
    destroy .character.rename

    # A blank name is no change, and so is the same one.
    if {$name eq "" || $name eq $character(v,name)} return

    if {[catch {angband_rename $name} err]} {
        set character(msg) $err
        return
    }
    set character(msg) "You are now known as $name."
    character_refresh
}

proc character_dump {} {
    global character

    if {[catch {angband_dump suggest} path]} {
        set character(msg) "There is no character to dump."
        return
    }
    set file [tk_getSaveFile -parent .character -title "Dump to File" \
        -initialdir [file dirname $path] -initialfile [file tail $path]]
    if {$file eq ""} return

    if {[catch {angband_dump write $file} err]} {
        set character(msg) "Could not write the dump: $err"
        return
    }
    set character(msg) "Character dump written to [file tail $file]."
}

# F1: the manual where it covers what the page shows -- the attributes a
# character has, its virtues, its mutations, the notes it keeps -- and, in the
# status line, how to use the window, which the manual does not cover.
proc character_help {} {
    global character
    set where {
        info      birth#character-characteristics
        history   birth#character-characteristics
        virtues   virtues
        mutations mutations
        notes     command#message-commands
    }
    help_window [help_first [expr {[dict exists $where $character(page)]
        ? [dict get $where $character(page)] : ""}] birth]
    set character(msg) "Choose a page to read. Keys the window does not use\
        itself go to the game, so it can stay open while you play."
}

# --- the window ----------------------------------------------------------------

proc character_menus {} {
    global character

    set mb .character.mb
    menu $mb
    .character configure -menu $mb

    menu $mb.file
    $mb add cascade -label File -underline 0 -menu $mb.file
    $mb.file add command -label "Change Name..." -underline 7 \
        -command character_rename
    $mb.file add command -label "Dump to File..." -underline 0 \
        -accelerator Ctrl+S -command character_dump
    $mb.file add separator
    $mb.file add command -label Close -underline 0 -accelerator Esc \
        -command {destroy .character}

    menu $mb.view
    $mb add cascade -label View -underline 0 -menu $mb.view
    foreach page $character(pages) {
        lassign $page key label under
        $mb.view add radiobutton -label $label -underline $under \
            -variable character(page) -value $key -command character_switch
    }

    # Not $mb.help: on macOS Tk treats a menu with that name as the platform's
    # own Help menu and fills it with things this window does not have.
    menu $mb.about
    $mb add cascade -label Help -underline 0 -menu $mb.about
    $mb.about add command -label "On Character..." -underline 0 \
        -accelerator F1 -command character_help
}

# A row of label and value, the label on the left and the value sunken.
proc character_row {parent row col field label {anchor w} {width ""}} {
    global character
    set character(v,$field) $character(dash)
    ttk::label $parent.l$field$col -text "$label:" -font {Helvetica 12}
    ttk::label $parent.v$field$col -textvariable character(v,$field) \
        -relief sunken -borderwidth 1 -anchor $anchor -padding {4 1}
    if {$width ne ""} { $parent.v$field$col configure -width $width }
    return [list $parent.l$field$col $parent.v$field$col]
}

proc character_window {} {
    global character

    if {[winfo exists .character]} {
        wm deiconify .character
        raise .character
        return
    }

    unset -nocomplain character(virtues) character(hpcolour)
    toplevel .character
    wm title .character "Zangband: Character"
    wm protocol .character WM_DELETE_WINDOW { destroy .character }
    wm minsize .character 900 640

    character_menus

    # One Ttk frame under everything, because that is what carries the theme's
    # grey: the toplevel itself is drawn by the platform.
    set w .character.f
    ttk::frame $w
    pack $w -fill both -expand 1

    set ground [ttk::style lookup . -background]
    set field  [ttk::style lookup TEntry -fieldbackground {} white]

    # --- identity and condition
    ttk::frame $w.top
    pack $w.top -fill x -padx 10 -pady {10 4}

    set id $w.top.id
    ttk::labelframe $id -text Character -padding 8
    canvas $id.glyph -width 64 -height 64 -background black \
        -relief sunken -borderwidth 2 -highlightthickness 0
    $id.glyph create text 34 34 -text "@" -fill white -anchor center \
        -font {Courier 32 bold} -tags letter
    $id.glyph create image 34 34 -image [character_portrait_image] \
        -anchor center -tags tile -state hidden
    ttk::frame $id.fields
    grid $id.glyph -row 0 -column 0 -sticky nw -padx {0 10}
    grid $id.fields -row 0 -column 1 -sticky new
    grid columnconfigure $id 1 -weight 1

    set i 0
    foreach {fieldname label} $character(identity) {
        set r [expr {$i / 2}]
        set c [expr {($i % 2) * 2}]
        lassign [character_row $id.fields $r $c $fieldname $label] l v
        grid $l -row $r -column $c -sticky e -pady 2 \
            -padx [expr {$c ? {12 6} : {0 6}}]
        grid $v -row $r -column [expr {$c + 1}] -sticky ew -pady 2
        incr i
    }
    grid columnconfigure $id.fields {1 3} -weight 1 -uniform value

    set cond $w.top.cond
    ttk::labelframe $cond -text Condition -padding 8
    set r 0
    foreach {name label fieldname colour} $character(bars) {
        set style [expr {$name eq "hp" ? "HP" : [string toupper $name]}].Horizontal.TProgressbar
        ttk::style configure $style -background $colour
        set character(v,$name) $character(dash)
        ttk::label $cond.l$name -text "$label:" -font {Helvetica 12}
        ttk::progressbar $cond.p$name -style $style -length 160
        ttk::label $cond.v$name -textvariable character(v,$name) \
            -relief sunken -borderwidth 1 -anchor e -width -10 -padding {4 1}
        grid $cond.l$name -row $r -column 0 -sticky e -padx {0 6} -pady 4
        grid $cond.p$name -row $r -column 1 -sticky ew -pady 4
        grid $cond.v$name -row $r -column 2 -sticky e -padx {6 0} -pady 4
        incr r
    }
    grid columnconfigure $cond 1 -weight 1

    grid $id   -row 0 -column 0 -sticky nsew -padx {0 12}
    grid $cond -row 0 -column 1 -sticky nsew
    grid columnconfigure $w.top 0 -weight 11
    grid columnconfigure $w.top 1 -weight 9

    # --- the page row
    ttk::frame $w.pages
    pack $w.pages -fill x -padx 10 -pady {8 4}
    ttk::label $w.pages.l -text "Page:"
    pack $w.pages.l -side left
    foreach page $character(pages) {
        lassign $page key label under
        ttk::radiobutton $w.pages.$key -text $label -underline $under \
            -variable character(page) -value $key -command character_switch
        pack $w.pages.$key -side left -padx {16 0}
    }

    # --- status, buttons and the rule over them, packed before the pages so
    # that they keep their room when the window is made smaller
    ttk::frame $w.status
    pack $w.status -side bottom -fill x -padx 2 -pady 2
    ttk::label $w.status.msg -textvariable character(msg) -font {Helvetica 12} \
        -relief sunken -borderwidth 1 -anchor w -padding {4 1}
    ttk::label $w.status.turn -textvariable character(turn) \
        -font {Helvetica 12} -relief sunken -borderwidth 1 -anchor center \
        -width -14 -padding {4 1}
    pack $w.status.turn -side right
    pack $w.status.msg -side left -fill x -expand 1 -padx {0 2}

    ttk::frame $w.btns
    pack $w.btns -side bottom -fill x -padx 10 -pady 10
    ttk::button $w.btns.rename -text "Change Name..." -underline 7 \
        -command character_rename
    ttk::button $w.btns.dump -text "Dump to File..." -underline 0 \
        -command character_dump
    ttk::button $w.btns.close -text Close -underline 0 -width 10 \
        -default active -command {destroy .character}
    pack $w.btns.rename -side left
    pack $w.btns.dump -side left -padx {10 0}
    pack $w.btns.close -side right

    ttk::separator $w.sep -orient horizontal
    pack $w.sep -side bottom -fill x

    # --- the pages, every one in the same cell
    set pg $w.page
    ttk::frame $pg
    pack $pg -fill both -expand 1 -padx 10 -pady {4 10}
    grid columnconfigure $pg 0 -weight 1
    grid rowconfigure $pg 0 -weight 1 -minsize 250
    foreach page $character(pages) {
        ttk::frame $pg.[lindex $page 0]
    }

    # Info: three groups side by side.
    set character(fields) {}
    set col 0
    foreach section $character(sections) {
        lassign $section name title rows
        set s $pg.info.$name
        ttk::labelframe $s -text $title -padding 8
        set r 0
        foreach {fieldname label} $rows {
            lappend character(fields) $fieldname
            lassign [character_row $s $r 0 $fieldname $label e -12] l v
            grid $l -row $r -column 0 -sticky w -padx {0 6} -pady 2
            grid $v -row $r -column 1 -sticky e -pady 2
            incr r
        }
        grid columnconfigure $s 0 -weight 1
        grid $s -row 0 -column $col -sticky nsew \
            -padx [expr {$col < 2 ? {0 10} : 0}]
        grid columnconfigure $pg.info $col -weight 1 -uniform group
        incr col
    }
    set character(fields) [lsort -unique $character(fields)]

    # History, Mutations and Notes: a text each, read-only.  History is held
    # to about the width a paragraph reads well at.
    foreach {key title wrap width} {
        history   Background word 68
        mutations Mutations  word 68
        notes     Notes      none 80
    } {
        set f $pg.$key.f
        ttk::labelframe $f -text $title -padding 6
        pack $f -fill both -expand 1
        text $f.t -height 8 -width $width -wrap $wrap -font {Courier 12} \
            -background $field -foreground black \
            -relief sunken -borderwidth 2 -highlightthickness 1 \
            -highlightcolor black -highlightbackground $ground \
            -padx 4 -pady 4 -state disabled \
            -yscrollcommand [list $f.sb set]
        ttk::scrollbar $f.sb -orient vertical -command [list $f.t yview]
        pack $f.sb -side right -fill y
        pack $f.t -side left -fill both -expand [expr {$key ne "history"}]
    }

    # Virtues: a grid, filled in by character_refresh_virtues.
    ttk::labelframe $pg.virtues.f -text Virtues -padding 8
    pack $pg.virtues.f -fill both -expand 1

    # Keys typed here still play the game.  Without this the window would have
    # to be dismissed before the next step could be taken, which would make it
    # something to put away rather than something to leave open.
    bind .character <Key> { angband_key %N %s %A }
    bind .character <Escape> { destroy .character ; break }
    bind .character <Control-s> { character_dump ; break }
    bind .character <F1> { character_help ; break }
    accel_bind .character n { character_rename ; break }
    accel_bind .character d { character_dump ; break }
    accel_bind .character c { destroy .character ; break }
    foreach page $character(pages) {
        lassign $page key label under
        set letter [string tolower [string index $label $under]]
        accel_bind .character $letter \
            [list apply {{key} {
                set ::character(page) $key
                character_switch
            }} $key]
    }

    character_switch
}

# The window stays up to date whether or not anyone is looking at it: the
# bindings live on "." and cost nothing while it is closed, because
# character_refresh returns immediately when there is no window.
foreach event $character(events) {
    bind . <<Angband_$event>> {+ character_refresh }
}
bind . <<TilesChanged>> {+ character_portrait }
