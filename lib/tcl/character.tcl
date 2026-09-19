# The character sheet.
#
# The first window that is not a pane (decision 16), and the first drawn to the
# Classical design system in lib/tcl/classical.tcl -- an identity header, three
# hairline meters for the live resources, three ruled stat columns, the history
# as prose, and a keybinding strip.  The spec is in
# .claude/plans/design_handoff_character_sheet.
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

# The three stat columns, in reading order.  The numerals are the design's, and
# they run i, ii, iii, iv down the sheet.
set character(sections) {
    {fighting "FIGHTING" i {
        armor_class     "Armour class"
        to_hit          "To hit"
        to_dam          "To damage"
        blows_per_round "Blows / round"
        shots_per_round "Shots / round"
    }}
    {being "BEING" ii {
        speed        "Speed"
        infravision  "Infravision"
        light        "Light radius"
        age          "Age"
        size         "Height / weight"
        total_weight "Burden"
    }}
    {standing "STANDING" iii {
        exp        "Experience"
        depth      "Current depth"
        max_depth  "Deepest reached"
        title      "Title"
    }}
}

# The pages, in order.  Flags is not here: the resistance grid is built from
# ui-entry.h's iterator and a column per equipment slot, and neither the
# equipment reads nor that machinery exist yet.  An empty tab would be worse
# than a missing one; it arrives with equipment in T7.
set character(pages) {
    {info      INFO      i}
    {virtues   VIRTUES   ii}
    {mutations MUTATIONS iii}
    {notes     NOTES     iv}
}
set character(page) info

# The pages, in order.  Flags is not here: the resistance grid is built from
# ui-entry.h's iterator with a column per equipment slot, and neither the
# equipment reads nor that machinery exist yet.  An empty tab would be worse
# than a missing one; it arrives with equipment in T7.
set character(pages) {
    {info      INFO      i}
    {virtues   VIRTUES   ii}
    {mutations MUTATIONS iii}
    {notes     NOTES     iv}
}
set character(page) info

# When to read again.  The handful of events that can move a field, rather than
# all 66: EVENT_MAP alone fires many times a turn and none of them is a stat.
set character(events) {
    STATS HP MANA AC EXPERIENCE PLAYERLEVEL PLAYERTITLE GOLD
    DUNGEONLEVEL PLAYERSPEED RACE_CLASS LIGHT STUDYSTATUS
    ENTER_GAME LEAVE_BIRTH ENTER_WORLD
}

# --- the values --------------------------------------------------------------

# A depth in feet, or the word for it.  Angband counts depth in levels of fifty
# feet and calls level zero the town; the sheet says so rather than printing a
# nought and leaving the player to know that.
proc character_depth {n} {
    if {$n eq "" || $n eq 0} { return "Town" }
    return "[expr {$n * 50}] ft"
}

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

# The subhead under the name.  Derived, and only from what the game actually
# knows: race, class, and how deep this character has been.
proc character_subhead {v} {
    set race [dict get $v race]
    set class [dict get $v class]
    set deepest [dict get $v max_depth]

    if {$deepest == 0} {
        return "$race $class, not yet below ground"
    }
    return "$race $class of the deep places, [expr {$deepest * 50}] feet down"
}

proc character_values {} {
    global character

    if {[catch {angband_player} v]} { return {} }

    # Derived text first, while the numbers are still numbers: the display
    # substitutions below turn max_depth into "Town" or an em dash, and the
    # subhead wants to multiply it by fifty.
    dict set v subhead [character_subhead $v]

    # Everything the sheet shows that is not a field on its own.
    dict set v size "[dict get $v height] / [dict get $v weight]"
    dict set v to_hit [character_signed [dict get $v to_hit]]
    dict set v to_dam [character_signed [dict get $v to_dam]]
    dict set v depth [character_depth [dict get $v depth]]
    dict set v max_depth [expr {[dict get $v max_depth] == 0
        ? $character(dash) : [character_depth [dict get $v max_depth]]}]

    return $v
}

# The design's muted rule: a zero or absent value drops to neutral-500 so it
# reads as "none" rather than as data.
proc character_muted {value} {
    global character
    return [expr {$value eq $character(dash) || $value eq "" || $value eq "0"
                  || $value eq "0.0" || $value eq "+0"}]
}

proc character_refresh {} {
    global character

    if {![winfo exists .character]} return

    set v [character_values]
    set have [expr {[dict size $v] > 0}]

    foreach field $character(fields) {
        if {$have && [dict exists $v $field]} {
            set value [dict get $v $field]
        } else {
            set value $character(dash)
        }
        set character(v,$field) $value
        if {[info exists character(w,$field)]} {
            $character(w,$field) configure -fg [classical::c \
                [expr {[character_muted $value] ? "neutral-500" : "text"}]]
        }
    }

    # The header is not a stat row and reads from the same dict.
    set character(v,name)  [expr {$have ? [dict get $v name] : $character(dash)}]
    set character(v,kicker) [expr {$have ? [string toupper [dict get $v title]] : ""}]
    set character(v,subhead) [expr {$have ? [dict get $v subhead] : ""}]
    set character(v,level) [expr {$have ? [dict get $v level] : $character(dash)}]
    set character(v,gold)  [expr {$have ? [dict get $v gold] : $character(dash)}]
    .character.card.ident.names.k configure -text \
        [classical::tracked $character(v,kicker)]

    # The three meters.  A maximum of zero is an empty track, never a full one.
    foreach {name field} {hp hitpoints sp mana xp next_level} {
        lassign [expr {$have ? [dict get $v $field] : {0 0}}] now most
        set character(v,$name) "$now / $most"
        classical::meter_set .character.card.meters.m$name \
            [expr {$most > 0 ? double($now) / $most : 0}]
    }

    classical::prose_set .character.card.body.info.phistory \
        [expr {$have ? [dict get $v history] : ""}]

    character_refresh_virtues
    character_refresh_mutations
    character_refresh_notes

    set character(v,status) [expr {$have
        ? "turn [angband_player turn]" : "no character"}]
}

# A character made before the game recorded something has nothing to show for
# it, and saying so is better than an empty table that looks broken.
proc character_page_empty {page text} {
    classical::prose_set .character.card.body.$page.p$page $text
}

proc character_refresh_virtues {} {
    if {![winfo exists .character]} return

    if {[catch {angband_virtue list} rows]} { set rows {} }

    if {![llength $rows]} {
        character_page_empty virtues \
            "Nothing is recorded. A character chooses eight virtues at birth,\
             from their race, their class and the realms they study; one made\
             before the game kept them has none."
        return
    }

    # The game's own sentence for each.  virtue_describe on its own is a
    # fragment -- "neutral to" -- because it is written to be read as "You are
    # neutral to Valour", and a column of those reads as nonsense.  virtue_line
    # is the whole thing, and it is what the game prints elsewhere.
    set lines {}
    foreach row $rows {
        lassign $row idx name value desc line
        lappend lines [expr {$line ne "" ? $line
            : [format "%-16s %5d %s" $name $value $desc]}]
    }
    classical::prose_set_lines .character.card.body.virtues.pvirtues $lines
}

proc character_refresh_mutations {} {
    if {![winfo exists .character]} return

    if {[catch {angband_mutation list} rows]} { set rows {} }

    if {![llength $rows]} {
        character_page_empty mutations \
            "Unchanged. Nothing has rewritten this character yet."
        return
    }

    set lines {}
    foreach row $rows {
        lassign $row idx name desc power
        lappend lines $desc
    }
    classical::prose_set_lines .character.card.body.mutations.pmutations $lines
}

proc character_refresh_notes {} {
    if {![winfo exists .character]} return

    if {[catch {angband_history list} rows]} { set rows {} }

    if {![llength $rows]} {
        character_page_empty notes "Nothing has happened yet."
        return
    }

    # Turn, depth, level, event.  Columns, so the panel must not reflow it --
    # which is what prose_set_lines is for.
    set lines {}
    foreach row $rows {
        lassign $row turn depth clev text
        lappend lines [format "%8s  %-6s  %-4s  %s" $turn \
            [expr {$depth == 0 ? "town" : "[expr {$depth * 50}] ft"}] \
            "L$clev" $text]
    }
    classical::prose_set_lines .character.card.body.notes.pnotes $lines
}

proc character_switch {} {
    global character

    foreach page $character(pages) {
        lassign $page key label numeral
        if {$key eq $character(page)} {
            grid .character.card.body.$key -row 0 -column 0 -sticky nsew
        } else {
            grid remove .character.card.body.$key
        }
    }
    character_refresh
}

# --- the window ---------------------------------------------------------------

proc character_window {} {
    global character

    if {[winfo exists .character]} {
        wm deiconify .character
        raise .character
        return
    }

    set card [classical::window .character "Character"]
    wm protocol .character WM_DELETE_WINDOW { destroy .character }

    # Open wide enough for three columns.
    #
    # Left to its own devices the window asks for whatever the layout wants
    # before it has been mapped, which is one column, which is tall and narrow
    # -- and then reflows to three when the player widens it, having already
    # made a poor first impression.  Three columns and the history below them
    # is the shape the sheet was designed in, so it opens in it.
    wm geometry .character 1180x780
    wm minsize .character 620 480

    # Keys typed here still play the game.  Without this the window would have
    # to be dismissed before the next step could be taken, which would make it
    # something to put away rather than something to leave open.
    bind .character <Key> { angband_key %N %s %A }
    bind .character <Escape> { destroy .character ; break }

    classical::titlestrip $card ZANGBAND CHARACTER
    classical::footer $card {
        esc "close this window"
        C   "the game's own sheet"
    } character(v,status)

    # --- identity -------------------------------------------------------
    set ident $card.ident
    frame $ident -bg [classical::c bg]
    pack $ident -fill x -padx [classical::sp 6] -pady [list [classical::sp 6] [classical::sp 4]]

    # The sigil.  A text glyph in a bordered box, not an asset.
    frame $ident.sigil -bg [classical::c accent-100] -width 84 -height 84 \
        -highlightthickness 1 -highlightbackground [classical::c accent]
    pack $ident.sigil -side left
    pack propagate $ident.sigil 0
    label $ident.sigil.at -text "@" -font [classical::f sigil] \
        -bg [classical::c accent-100] -fg [classical::c accent-700]
    pack $ident.sigil.at -expand 1

    set names $ident.names
    frame $names -bg [classical::c bg]
    pack $names -side left -fill x -expand 1 -padx [list [classical::sp 6] 0]
    label $names.k -text "" -font [classical::f kicker] \
        -bg [classical::c bg] -fg [classical::c neutral-600] -anchor w
    label $names.n -textvariable character(v,name) -font [classical::f title] \
        -bg [classical::c bg] -fg [classical::c text] -anchor w
    label $names.s -textvariable character(v,subhead) \
        -font [classical::f subhead] \
        -bg [classical::c bg] -fg [classical::c neutral-700] -anchor w
    pack $names.k $names.n $names.s -anchor w

    # The display figures.  Level takes the accent; gold does not, because two
    # things in gold is neither of them emphasised.
    set fig $ident.figures
    frame $fig -bg [classical::c bg]
    pack $fig -side right -anchor n
    set col 0
    foreach {field label colour} {level LEVEL accent-700 gold GOLD text} {
        set g $fig.g$field
        frame $g -bg [classical::c bg]
        pack $g -side left -padx [list [classical::sp 6] 0] -anchor n
        label $g.l -text [classical::tracked $label] \
            -font [classical::f tiny] \
            -bg [classical::c bg] -fg [classical::c neutral-600] -anchor e
        label $g.v -textvariable character(v,$field) \
            -font [classical::f display] \
            -bg [classical::c bg] -fg [classical::c $colour] -anchor e
        pack $g.l $g.v -anchor e
        incr col
    }

    # --- meters ---------------------------------------------------------
    set meters $card.meters
    frame $meters -bg [classical::c bg]
    pack $meters -fill x -padx [classical::sp 6] -pady [list 0 [classical::sp 4]]
    foreach {name label var} {
        hp "HIT POINTS" hp
        sp "MANA" sp
        xp "NEXT LEVEL IN" xp
    } {
        classical::meter $meters $name $label character(v,$var)
    }
    classical::responsive $meters \
        [list $meters.mhp $meters.msp $meters.mxp] 220

    # --- the pages ------------------------------------------------------
    set tabs {}
    foreach page $character(pages) {
        lassign $page key label numeral
        lappend tabs $key $label
    }
    set strip [classical::tabstrip $card pages $tabs character(page) \
        character_switch]
    pack $strip -fill x -padx [classical::sp 6] \
        -pady [list [classical::sp 3] [classical::sp 3]]

    set body $card.body
    frame $body -bg [classical::c bg]
    pack $body -fill both -expand 1 -padx [classical::sp 6] \
        -pady [list 0 [classical::sp 6]]
    grid columnconfigure $body 0 -weight 1
    grid rowconfigure $body 0 -weight 1

    # Every page in the same cell; switching raises one and removes the rest.
    foreach page $character(pages) {
        lassign $page key label numeral
        frame $body.$key -bg [classical::c bg]
    }

    # The three that are a single panel apiece.
    foreach {key height} {virtues 14 mutations 14 notes 18} {
        pack [classical::prose $body.$key $key $height 1] \
            -fill both -expand 1
    }

    set info $body.info
    set cols $info.cols
    frame $cols -bg [classical::c bg]
    pack $cols -fill x

    set character(fields) {}
    set frames {}
    foreach section $character(sections) {
        lassign $section name title numeral rows
        set s $cols.s$name
        frame $s -bg [classical::c bg]
        lappend frames $s
        classical::sectionhead $s $name $title $numeral
        pack $s.h$name -fill x

        set n [expr {[llength $rows] / 2}]
        set i 0
        foreach {field label} $rows {
            incr i
            lappend character(fields) $field
            set character(v,$field) $character(dash)
            set character(w,$field) [classical::statrow $s $field $label \
                character(v,$field) [expr {$i == $n}]]
        }
    }
    classical::responsive $cols $frames 260

    # --- the background, which belongs with the rest of the identity ----
    classical::sectionhead $info history "HISTORY" iv
    pack $info.hhistory -fill x \
        -pady [list [classical::sp 4] [classical::sp 3]]
    pack [classical::prose $info history 3] -fill both -expand 1

    classical::tab_paint $strip character(page)
    character_switch

    # The geometry above is only a request until the window is mapped, and the
    # column count is computed from the real width, so settle one before the
    # other reads it.
    update idletasks
    classical::columns $meters [list $meters.mhp $meters.msp $meters.mxp] 220
    classical::columns $cols $frames 260
}

# The window stays up to date whether or not anyone is looking at it: the
# bindings live on "." and cost nothing while it is closed, because
# character_refresh returns immediately when there is no window.
foreach event $character(events) {
    bind . <<Angband_$event>> {+ character_refresh }
}
