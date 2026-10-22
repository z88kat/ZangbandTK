# The Flags page of the Character window.
#
# The grid of what the character has and where it comes from: a row per
# property, a column per equipment slot, then You and Timed, and a Result
# straight after the name.  The design is .claude/plans/design_handoff_flags_tk;
# the player's side of it is docs/flags.rst, which is also where the cards get
# their descriptions, so the manual and the page say the same thing.
#
# Everything the page knows comes from angband_flags.  The Result, the result
# without each slot, which curse gives a value and where the character's own
# value comes from are all worked out in C with the game's own combiners, so
# this file never decides a rule; it only draws and explains.
#
# Two canvases, not a grid of labels: sixteen columns by fifty-five rows is
# nearly nine hundred cells, which is slow to build as widgets and slower to
# recolour.  The heading is a canvas of its own so that it stays put while the
# rows scroll.  Nothing scrolls sideways.

set flags(dir) [file dirname [info script]]

array set flags {
    show       all
    markrow    ""
    markcol    ""
    sig        ""
    hover      ""
    tipafter   ""
    tipshown   0
    data       {}
    layout     {}
}

# The groups, in the game's order, with the word the page uses for each.  The
# game calls the third group hindrances; on the page they are Drawbacks, which
# reads as bad at a glance.
set flags(groups) {
    resistances res  Resistances
    abilities   abil Abilities
    hindrances  draw Drawbacks
    modifiers   mod  Modifiers
}

# Slot names, keyed by the body's own part names: the short one for a heading,
# the long one for the status bar and the cards, and the symbol an empty slot
# shows -- the same three as the Items window's doll.  A body this file has not
# heard of, as a shape change can make, falls back to the part's own name.
set flags(slots) {
    weapon       Weapon  Weapon             |
    shooting     Bow     Bow                \}
    {right hand} {Ring R} {Ring (right hand)} =
    {left hand}  {Ring L} {Ring (left hand)}  =
    neck         Amulet  Amulet             \"
    light        Light   Light              ~
    body         Body    Body               (
    back         Cloak   Cloak              (
    arm          Shield  Shield             )
    head         Helm    Helm               \]
    hands        Gloves  Gloves             \]
    feet         Boots   Boots              \]
}

# Timed effects are named from the game's own description of them, with two
# exceptions where that description is a phrase for a message rather than a
# name.
set flags(timednames) {
    BLOODLUST Bloodlust
    OPP_ELEC  {Lightning resistance}
}

# The size of things, from the design.  The slot width is a floor: the real
# width is measured from the short names in the font in use.
#
# These, and every other length on the page, are for the size the page was
# drawn at: the Mac's, where a line of its 12-point text is 19 pixels high --
# and Linux and Windows at their natural size, where it is too.  The canvases
# are drawn in pixels and only the fonts grow by themselves, so on a desktop
# scaled to twice the size, where Tk makes a point 2.7 pixels and the line 38,
# each row held two rows' worth of text and the page could not be read.
# flags_metrics measures the line and flags_px scales every length by it.
set flags(design) {
    row     18
    band    20
    head    44
    namemin 132
    result  92
    slotmin 44
}
set flags(k) 1.0
foreach {key n} $flags(design) { set flags($key) $n }

proc flags_metrics {} {
    global flags
    set line [font metrics [uifont 12] -linespace]
    set flags(k) [expr {max(1.0, $line / 19.0)}]
    foreach {key n} $flags(design) { set flags($key) [flags_px $n] }
}

# A length from the design, in this screen's pixels.
proc flags_px {n} {
    global flags
    return [expr {round($n * $flags(k))}]
}

# --- what a value means ---------------------------------------------------------

proc flags_kind {group} {
    global flags
    foreach {g kind label} $flags(groups) {
        if {$g eq $group} { return $kind }
    }
    return abil
}

proc flags_group_label {group} {
    global flags
    foreach {g kind label} $flags(groups) {
        if {$g eq $group} { return $label }
    }
    return [string totitle $group]
}

# A signed number for display: "+3", "−2", "0".
#
# Not expr: expr {$n > 0 ? "+$n" : $n} hands back the string "+3", which expr
# then reads straight back as the number 3, and the sign is gone.  This bit the
# character sheet a month before it bit this page.
proc flags_signed {n} {
    if {$n > 0} { return "+$n" }
    if {$n < 0} { return "−[expr {-$n}]" }
    return 0
}

# A number from angband_flags, or 0 when it sent a word instead.
proc flags_num {v} {
    return [expr {[string is integer -strict $v] ? $v : 0}]
}

# One cell's state: has, immune, timed, against, drawback, unknown, nothing,
# empty (nothing worn) or na (this column cannot give this row anything).
proc flags_cell_state {kind v {worn 1}} {
    if {$v eq "none"} { return [expr {$worn ? "na" : "empty"}] }
    if {$v eq "unknown"} { return unknown }
    if {$v eq "both"} { return nothing }
    if {$v == 0} { return nothing }
    switch -- $kind {
        res     { return [expr {$v >= 3 ? "immune" : $v > 0 ? "has" : "against"}] }
        draw    { return [expr {$v > 0 ? "drawback" : "has"}] }
        default { return [expr {$v > 0 ? "has" : "against"}] }
    }
}

# What a cell says: its mark, the ink and the tint.  Every state has a mark of
# its own as well as a colour, so the page reads the same in the colour-blind
# palette; the colours are palette roles, so a palette can change them.
proc flags_style {state} {
    switch -- $state {
        has      { return [list + [display_role flaghas]   [display_role flaghastint]] }
        immune   { return [list * [display_role flagimm]   [display_role flagimmtint]] }
        timed    { return [list ~ [display_role flagtimed] [display_role flagtimedtint]] }
        against  { return [list − [display_role flagbad] [display_role flagbadtint]] }
        drawback { return [list ! [display_role flagbad]   [display_role flagbadtint]] }
        unknown  { return [list ? [display_role flagunk]   [display_role flagunktint]] }
        nothing  { return [list · [display_role flagnone] white] }
        empty    { return [list "" "" [display_role flagempty]] }
        default  { return [list "" "" white] }
    }
}

# A row's Result as {state word rank}.  The value comes from the game's own
# combiner; the two rules added here are the design's: a timed effect lifts it
# by one level, and when that is what makes the difference the word is "For
# now".  The rank orders results from worst to best, which is how the heading
# card decides between "you'd lose" and "you'd gain".
proc flags_result {kind result} {
    lassign $result v a
    set timed [expr {[flags_num $a] != 0}]
    if {$kind eq "mod"} {
        if {$v eq "unknown"} { return {unknown Unknown 0} }
        set total [expr {[flags_num $v] + [flags_num $a]}]
        if {$total == 0} { return [list nothing · 0] }
        set word [flags_signed $total]
        if {$timed} { return [list timed $word $total] }
        return [list [expr {$total > 0 ? "has" : "against"}] $word $total]
    }
    if {$v eq "unknown"} { return {unknown Unknown 0} }
    set base [flags_num $v]
    if {$kind eq "res"} {
        set base [expr {$base >= 3 ? 3 : $base > 0 ? 1 : $base < 0 ? -1 : 0}]
        set level [expr {$timed && $base < 3 ? $base + 1 : $base}]
        if {$level >= 3} { return {immune Immune 3} }
        if {$level >= 1} {
            return [expr {$base < 1 ? {timed {For now} 1} : {has Resist 2}}]
        }
        if {$level < 0} { return {against Vulnerable -1} }
        return {nothing no 0}
    }
    if {$kind eq "draw"} {
        if {$base > 0} { return {drawback Affected -2} }
        if {$timed} { return {timed {For now} -1} }
        return {nothing no 0}
    }
    set base [expr {$base > 0 ? 1 : $base < 0 ? -1 : 0}]
    set level [expr {$timed ? $base + 1 : $base}]
    if {$level >= 1} {
        return [expr {$base < 1 ? {timed {For now} 1} : {has Yes 2}}]
    }
    if {$level < 0} { return {against Blocked -1} }
    return {nothing no 0}
}

# The word as it reads inside a sentence: "resist", "for now", "+3".
proc flags_word_lc {word} {
    if {[regexp {^[+−·]} $word]} { return $word }
    return [string tolower $word]
}

# --- reading the game -------------------------------------------------------------

# Everything the page shows, in one fetch: the columns and every group's rows.
# Returns "" when there is no character.
proc flags_fetch {} {
    if {[catch {angband_flags columns} cols]} { return "" }
    set groups {}
    foreach group [angband_flags categories] {
        if {[catch {angband_flags rows $group} rows]} { set rows {} }
        lappend groups $group $rows
    }
    return [list $cols $groups]
}

proc flags_slot_info {part} {
    global flags
    foreach {p short long symbol} $flags(slots) {
        if {$p eq $part} { return [list $short $long $symbol] }
    }
    set name [string totitle $part]
    return [list $name $name .]
}

proc flags_timed_name {name desc} {
    global flags
    if {[dict exists $flags(timednames) $name]} {
        return [dict get $flags(timednames) $name]
    }
    return [string toupper $desc 0 0]
}

# --- the page ----------------------------------------------------------------------

proc flags_build {f} {
    global flags

    flags_metrics
    set ground [ttk::style lookup . -background]
    set lf $f.lf
    ttk::labelframe $lf -text "What you have, and what is giving it to you" \
        -padding 6
    pack $lf -fill both -expand 1

    # Show: four filters, and the way to the manual.
    set sh $lf.show
    ttk::frame $sh
    pack $sh -side top -fill x -pady {0 4}
    ttk::label $sh.l -text "Show:" -font [uifont 12 bold]
    pack $sh.l -side left
    foreach {key label} {
        all All effect {In effect} against {Against you} unknown {Not yet learned}
    } {
        ttk::radiobutton $sh.$key -text $label -variable flags(show) \
            -value $key -command flags_redraw
        pack $sh.$key -side left -padx {12 0}
        bind $sh.$key <Enter> [list flags_say_show $key]
        bind $sh.$key <Leave> flags_say_idle
    }
    ttk::button $sh.help -text "Help..." -command flags_help
    pack $sh.help -side right

    # The note for a new character, shown only while most runes are unlearned.
    ttk::frame $lf.note
    label $lf.note.l -bitmap info -compound left -padx 6 -anchor w \
        -justify left -font [uifont 12] -background $ground

    pack $lf.note.l -side left -fill x

    # The legend, packed before the grid so it keeps its room.
    set lg $lf.legend
    ttk::frame $lg
    pack $lg -side bottom -fill x -pady {4 0}
    set i 0
    foreach {state text tip} {
        has      "has it"          "+: this gives it to you, for as long as you wear it."
        immune   "immune"          "*: this makes you immune. Resistances only."
        timed    "timed"           "~: you have it for now, from a potion, scroll or spell."
        against  "against you"     "−: this works against you: a vulnerability, or it takes the property away."
        drawback "drawback"        "!: this puts a drawback on you."
        unknown  "not identified"  "?: the item has a rune you haven't learned, and this could be it."
        nothing  "nothing"         "·: you're wearing something here, and it doesn't give this."
        empty    "empty slot"      "A grey column is a slot with nothing in it."
        rune     "rune not learned" "A grey, sloping name is a rune you haven't learned yet. Its values are still shown."
    } {
        set sw [canvas $lg.s$i -width [flags_px 18] -height [flags_px 14] \
            -highlightthickness 0 -borderwidth 0 -background $ground]
        if {$state eq "rune"} {
            $sw create text [flags_px 9] [flags_px 7] -text Ab \
                -font [uifont 11 italic] -fill [display_role flagrune]
            ttk::label $lg.t$i -text $text -font [uifont 12 italic] \
                -foreground [display_role flagrune]
        } else {
            lassign [flags_style $state] mark ink tint
            $sw create rectangle 1 1 [flags_px 17] [flags_px 13] -fill $tint \
                -outline #a3a3a3
            if {$mark ne ""} {
                $sw create text [flags_px 9] [flags_px 7] -text $mark \
                    -fill $ink -font [uifont Courier 12 bold]
            }
            ttk::label $lg.t$i -text $text -font [uifont 12]
        }
        pack $sw -side left -padx [list 0 [flags_px 3]]
        pack $lg.t$i -side left -padx [list 0 [flags_px 10]]
        foreach w [list $sw $lg.t$i] {
            bind $w <Enter> [list set character(msg) $tip]
            bind $w <Leave> flags_say_idle
        }
        incr i
    }

    # The grid: a heading canvas over a body canvas, and a scrollbar.
    set g $lf.grid
    ttk::frame $g -relief sunken -borderwidth 2
    pack $g -side top -fill both -expand 1
    canvas $g.head -height $flags(head) -background $ground \
        -highlightthickness 0 -borderwidth 0 -takefocus 0
    canvas $g.body -background white -highlightthickness 0 -borderwidth 0 \
        -takefocus 0 -yscrollcommand [list $g.sb set]
    ttk::scrollbar $g.sb -orient vertical -command [list flags_yview]
    grid $g.head -row 0 -column 0 -sticky ew
    grid $g.body -row 1 -column 0 -sticky nsew
    grid $g.sb   -row 1 -column 1 -sticky ns
    grid columnconfigure $g 0 -weight 1
    grid rowconfigure $g 1 -weight 1

    foreach w [list $g.head $g.body] {
        bind $w <Motion> [list flags_motion $w %x %y %X %Y]
        bind $w <Leave>  flags_leave
    }
    bind $g.body <1>        [list flags_click_body %x %y]
    bind $g.head <1>        [list flags_click_head %x]
    bind $g.head <Double-1> [list flags_inspect %x]
    bind $g.body <MouseWheel> { tk::MouseWheel %W y %D -40.0 ; flags_tip_hide }
    bind $g.body <Configure> flags_redraw_soon
}

proc flags_canvas {which} {
    return .character.f.page.flags.lf.grid.$which
}

proc flags_yview {args} {
    flags_tip_hide
    [flags_canvas body] yview {*}$args
}

# Called by character_refresh on every update.  Cheap when nothing changed or
# when the page isn't showing: the fetch is compared with the last one, and
# drawing happens only on the Flags page.
proc flags_refresh {have} {
    global flags character
    if {![winfo exists [flags_canvas body]]} return
    if {$character(page) ne "flags"} { set flags(sig) "" ; return }
    set data [expr {$have ? [flags_fetch] : ""}]
    if {$data eq $flags(sig)} return
    set flags(sig) $data
    set flags(data) $data
    flags_redraw
}

proc flags_redraw_soon {} {
    global flags
    if {[info exists flags(redrawafter)]} { after cancel $flags(redrawafter) }
    set flags(redrawafter) [after idle flags_redraw]
}

# --- drawing ------------------------------------------------------------------------

# Column tracks: {key x0 x1}, shared by the heading and the body.
#
# Measured, not fixed.  The design's 44-pixel slots are for its 1:1 pixels,
# and Tk scales points to the screen -- by a third on this Mac -- so fourteen
# equal slots wide enough for "Weapon" would not fit the window at any size
# worth reading.  So each slot is as wide as its own name, and the headings
# are set at the largest size, from 11 down to 9, at which the whole row fits
# the page.  The name column and the Result are what get read, so they keep
# their room, and the name takes whatever is spare.  Widen the window and the
# headings grow back.  Nothing scrolls sideways.
proc flags_layout {width cols} {
    global flags
    set shorts {}
    foreach c $cols { lappend shorts [lindex [flags_slot_info [lindex $c 1]] 0] }
    lappend shorts You Timed
    set n [llength $cols]

    set need 0
    foreach {group rows} [lindex $flags(data) 1] {
        foreach row $rows {
            set need [expr {max($need, [font measure [uifont 12] [lindex $row 0]])}]
        }
    }
    set namemin [expr {min([flags_px 220],
        max($flags(namemin), $need + [flags_px 14]))}]
    set resultw [expr {[flags_px 26] + [font measure [uifont 12 bold] Vulnerable]
        + [flags_px 6]}]

    foreach size {11 10 9} {
        set widths {}
        set total 0
        foreach short $shorts {
            set w [expr {max([flags_px 36],
                [font measure [uifont $size] $short] + [flags_px 6])}]
            lappend widths $w
            incr total $w
        }
        if {$namemin + $resultw + $total <= $width} break
    }
    set flags(headfont) [uifont $size]
    set namew [expr {max($namemin, $width - $resultw - $total)}]

    set tracks [list [list name 0 $namew]]
    set x $namew
    lappend tracks [list result $x [incr x $resultw]]
    set keys {}
    for {set i 0} {$i < $n} {incr i} { lappend keys s$i }
    lappend keys you timed
    foreach key $keys w $widths {
        lappend tracks [list $key $x [incr x $w]]
    }
    set flags(layout) $tracks
    return $tracks
}

proc flags_track {key} {
    global flags
    foreach t $flags(layout) {
        if {[lindex $t 0] eq $key} { return [lrange $t 1 2] }
    }
    return {0 0}
}

proc flags_redraw {} {
    global flags character
    set body [flags_canvas body]
    if {![winfo exists $body]} return
    unset -nocomplain flags(redrawafter)
    flags_tip_hide
    set head [flags_canvas head]
    # Measured again each time, so that a change to the text size is
    # followed as well.
    flags_metrics
    $head configure -height $flags(head)
    $head delete all
    $body delete all
    set flags(rows) {}
    set flags(rowinfo) {}
    array unset flags namebg,*
    array unset flags headbg,*

    set lf .character.f.page.flags.lf
    if {$flags(data) eq ""} {
        pack forget $lf.note
        $body create text [flags_px 12] [flags_px 12] -anchor nw \
            -text "No character yet." -font [uifont Courier 12] -fill #5a5a5a
        flags_counts {}
        return
    }
    lassign $flags(data) cols groups

    set width [winfo width $body]
    if {$width < 100} { set width [winfo reqwidth $body] }
    flags_layout $width $cols

    flags_draw_head $head $cols
    set y [flags_draw_body $body $cols $groups]
    $body configure -scrollregion [list 0 0 $width $y]
    flags_draw_marks
    flags_counts $groups
    flags_note $groups
}

proc flags_draw_head {c cols} {
    global flags
    set h $flags(head)
    set fb [uifont 12 bold]
    set f11 $flags(headfont)
    set mono [uifont Courier 12 bold]

    # The icon's box and the glyph's middle, which everything in it is placed
    # from.
    set top [flags_px 4]
    set bottom [flags_px 24]
    set mid [flags_px 14]

    lassign [flags_track name] x0 x1
    $c create text [expr {$x0 + [flags_px 8]}] [expr {$h - [flags_px 6]}] \
        -anchor sw -text Property -font $fb
    lassign [flags_track result] x0 x1
    $c create line $x0 0 $x0 $h -fill #d0d0d0
    $c create text [expr {$x0 + [flags_px 8]}] [expr {$h - [flags_px 6]}] \
        -anchor sw -text Result -font $fb

    set keys {}
    set i 0
    foreach col $cols { lappend keys s$i ; incr i }
    lappend keys you timed
    set i 0
    foreach key $keys {
        lassign [flags_track $key] x0 x1
        set cx [expr {($x0 + $x1) / 2}]
        set bg [$c create rectangle $x0 0 $x1 $h -fill [$c cget -background] \
            -outline ""]
        set flags(headbg,$key) $bg
        $c create line $x0 [expr {$h - 1}] $x0 0 [expr {$x1 - 1}] 0 -fill white
        $c create line [expr {$x1 - 1}] 0 [expr {$x1 - 1}] [expr {$h - 1}] \
            $x0 [expr {$h - 1}] -fill #828282
        set half [flags_px 10]
        $c create rectangle [expr {$cx - $half}] $top [expr {$cx + $half}] $bottom \
            -fill black -outline #5a5a5a
        switch -- $key {
            you {
                set short You
                $c create text $cx $mid -text @ -fill white -font $mono
            }
            timed {
                set short Timed
                # An hourglass, in two triangles.
                set w [flags_px 5]
                set y0 [flags_px 7]
                set y1 [flags_px 21]
                $c create polygon [expr {$cx - $w}] $y0 [expr {$cx + $w}] $y0 \
                    $cx $mid -fill #8fb0ff
                $c create polygon $cx $mid [expr {$cx - $w}] $y1 \
                    [expr {$cx + $w}] $y1 -fill #8fb0ff
            }
            default {
                set col [lindex $cols $i]
                lassign $col slot part glyph colour sname fname runes
                lassign [flags_slot_info $part] short long symbol
                if {$glyph ne ""} {
                    $c create text $cx $mid -text $glyph -fill $colour -font $mono
                    if {!$runes} {
                        # Runes not learned: say so on the heading, so the
                        # cause of a column of ? is visible before the rows.
                        $c create text [expr {$x1 - [flags_px 4]}] [flags_px 3] \
                            -anchor ne -text ? -fill [display_role flagunk] \
                            -font [uifont Courier 11 bold]
                    }
                } else {
                    $c create text $cx $mid -text $symbol -fill #3a3a3a -font $mono
                }
                incr i
            }
        }
        $c create text $cx [expr {$h - [flags_px 3]}] -anchor s -text $short \
            -font $f11
        if {$key eq "s0" || $key eq "you"} {
            $c create line $x0 0 $x0 $h -fill #a3a3a3 -width [flags_px 2]
        }
    }
}

# Which rows the Show filter keeps.
proc flags_row_shown {kind row result} {
    global flags
    lassign $row label known values
    switch -- $flags(show) {
        effect  { return [expr {[lindex $result 0] ni {nothing unknown}}] }
        against {
            foreach v $values {
                if {[flags_cell_state $kind $v] in {against drawback}} { return 1 }
            }
            return 0
        }
        unknown {
            if {!$known} { return 1 }
            return [expr {"unknown" in $values}]
        }
        default { return 1 }
    }
}

proc flags_draw_body {c cols groups} {
    global flags
    set n [llength $cols]
    set width [lindex [lindex $flags(layout) end] 2]
    set f12 [uifont 12]
    set fi [uifont 12 italic]
    set fb [uifont 12 bold]
    set mono [uifont Courier 12 bold]
    set y 0
    set any 0

    foreach {group rows} $groups {
        set kind [flags_kind $group]
        set shown {}
        set r 0
        foreach row $rows {
            set res [flags_result $kind [lindex $row 5]]
            if {[flags_row_shown $kind $row $res]} {
                lappend shown [list $r $row $res]
            }
            incr r
        }
        if {![llength $shown]} continue
        set any 1

        # The band: the group's name and its summary, raised, folding on click.
        set folded [expr {[info exists flags(fold,$group)] && $flags(fold,$group)}]
        set y1 [expr {$y + $flags(band)}]
        $c create rectangle 0 $y $width $y1 -fill #d9d9d9 -outline ""
        $c create line 0 [expr {$y1 - 1}] 0 $y [expr {$width - 1}] $y -fill white
        $c create line 0 [expr {$y1 - 1}] [expr {$width - 1}] [expr {$y1 - 1}] \
            -fill #828282
        $c create text [flags_px 8] [expr {$y + $flags(band) / 2}] -anchor w \
            -text "[expr {$folded ? "▶" : "▼"}] [flags_group_label $group]" \
            -font $fb
        lassign [flags_track result] rx0
        $c create text [expr {$rx0 + [flags_px 8]}] [expr {$y + $flags(band) / 2}] \
            -anchor w -text [flags_band_summary $group $kind $rows] -font $f12
        lappend flags(rows) [list $y $y1 band $group]
        set y $y1
        if {$folded} continue

        foreach item $shown {
            lassign $item r row res
            lassign $row label known values auxs renderer result without
            set id $group:$r
            set y1 [expr {$y + $flags(row)}]
            set ym [expr {$y + $flags(row) / 2}]
            lappend flags(rows) [list $y $y1 row $id]
            dict set flags(rowinfo) $id [list $group $kind $row $res]

            # Name.
            lassign [flags_track name] x0 x1
            set flags(namebg,$id) [$c create rectangle $x0 $y $x1 $y1 \
                -fill white -outline ""]
            $c create text [expr {$x0 + [flags_px 8]}] $ym -anchor w -text $label \
                -font [expr {$known ? $f12 : $fi}] \
                -fill [expr {$known ? "black" : [display_role flagrune]}]

            # Result.
            lassign [flags_track result] x0 x1
            lassign $res state word
            lassign [flags_style $state] mark ink tint
            $c create rectangle $x0 $y $x1 $y1 -fill $tint -outline ""
            $c create line $x0 $y $x0 $y1 -fill #d0d0d0
            if {$kind eq "mod"} {
                $c create text [expr {$x0 + [flags_px 10]}] $ym -anchor w -text $word \
                    -font $mono -fill [expr {$state eq "nothing" ? [display_role flagnone] : $ink}]
            } else {
                $c create text [expr {$x0 + [flags_px 10]}] $ym -anchor w -text $mark \
                    -font $mono -fill [expr {$ink eq "" ? "black" : $ink}]
                $c create text [expr {$x0 + [flags_px 26]}] $ym -anchor w -text $word \
                    -font [expr {$state eq "nothing" ? $f12 : $fb}] \
                    -fill [expr {$state eq "nothing" ? [display_role flagnone] : $ink}]
            }

            # Cells: the slots, then You, then Timed.
            for {set j 0} {$j <= $n + 1} {incr j} {
                if {$j < $n} {
                    set key s$j
                    set worn [expr {[lindex [lindex $cols $j] 2] ne ""}]
                    set state [flags_cell_state $kind [lindex $values $j] $worn]
                    set v [lindex $values $j]
                } elseif {$j == $n} {
                    set key you
                    set v [lindex $values $n]
                    set state [flags_cell_state $kind $v]
                } else {
                    set key timed
                    set v [lindex $auxs $n]
                    set state [expr {$v eq "none" ? "na"
                        : [flags_num $v] != 0 ? "timed" : "nothing"}]
                }
                lassign [flags_track $key] x0 x1
                lassign [flags_style $state] mark ink tint
                $c create rectangle $x0 $y $x1 $y1 -fill $tint -outline #ececec
                if {$kind eq "mod" && $state in {has against timed}} {
                    set mark [flags_signed [flags_num $v]]
                }
                if {$mark ne ""} {
                    $c create text [expr {($x0 + $x1) / 2}] $ym -text $mark \
                        -fill $ink -font $mono
                }
            }
            foreach key {s0 you} {
                lassign [flags_track $key] x0
                $c create line $x0 $y $x0 $y1 -fill #a3a3a3 -width [flags_px 2]
            }
            set y $y1
        }
    }

    if {!$any} {
        set text [dict get {
            effect  "Nothing is in effect yet."
            against "Nothing is working against you."
            unknown "You've learned every rune on this page."
            all     ""
        } $flags(show)]
        $c create text [flags_px 12] [expr {$y + [flags_px 12]}] -anchor nw \
            -text $text -font [uifont Courier 12] -fill #5a5a5a
        incr y [flags_px 36]
    }

    # What the page leaves out, which the old notes said.
    set note "Not on this page: your race's and class's own abilities,\
        which nothing can add or take away; mutations and shapes; and a\
        weapon's brands, which belong to its blows and are listed on the\
        weapon."
    set t [$c create text [flags_px 12] [expr {$y + [flags_px 10]}] -anchor nw \
        -text $note -font $f12 -fill #5a5a5a \
        -width [expr {max([flags_px 200], $width - [flags_px 24])}]]
    lassign [$c bbox $t] - - - by
    return [expr {$by + [flags_px 10]}]
}

# A band's summary, from its rows.
proc flags_band_summary {group kind rows} {
    set total [llength $rows]
    set good 0 ; set fornow 0 ; set unknown 0 ; set lowered 0
    foreach row $rows {
        lassign $row label known values auxs renderer result
        if {!$known} { incr unknown }
        lassign [flags_result $kind $result] state word rank
        switch -- $state {
            has - immune - drawback { incr good }
            timed { incr fornow }
            against { if {$kind eq "mod"} { incr lowered } }
        }
    }
    switch -- $kind {
        res  { set parts [list "$good of $total resisted"] }
        abil { set parts [list "$good of $total"] }
        draw { set parts [list [expr {$good ? "$good affecting you" : "nothing affecting you"}]] }
        mod  {
            set parts [list "$good raised"]
            if {$lowered} { lappend parts "$lowered lowered" }
        }
    }
    if {$fornow} { lappend parts "$fornow for now" }
    if {$unknown} { lappend parts "$unknown not known" }
    return [join $parts " · "]
}

# The counts beside the Show buttons, which stay put as the filter changes.
proc flags_counts {groups} {
    global flags
    set sh .character.f.page.flags.lf.show
    set all 0 ; set effect 0 ; set against 0 ; set unknown 0
    foreach {group rows} $groups {
        set kind [flags_kind $group]
        foreach row $rows {
            incr all
            set res [flags_result $kind [lindex $row 5]]
            set saved $flags(show)
            foreach key {effect against unknown} {
                set flags(show) $key
                if {[flags_row_shown $kind $row $res]} { incr $key }
            }
            set flags(show) $saved
        }
    }
    foreach {key label} {
        all All effect {In effect} against {Against you} unknown {Not yet learned}
    } {
        if {[winfo exists $sh.$key]} {
            $sh.$key configure -text [expr {[llength $groups]
                ? "$label ([set $key])" : $label}]
        }
    }
}

# The note for a new character, while more runes are unlearned than learned.
proc flags_note {groups} {
    set lf .character.f.page.flags.lf
    set all 0 ; set learned 0
    foreach {group rows} $groups {
        foreach row $rows {
            incr all
            if {[lindex $row 1]} { incr learned }
        }
    }
    if {$all && $learned < $all - $learned} {
        $lf.note.l configure -text "You've learned $learned of $all runes.\
            Grey rows are ones you haven't noticed yet; you'll learn each\
            the first time it shows itself."
        pack $lf.note -after $lf.show -side top -fill x -pady {0 4}
    } else {
        pack forget $lf.note
    }
}

# --- marking ------------------------------------------------------------------------

proc flags_draw_marks {} {
    global flags
    set body [flags_canvas body]
    set head [flags_canvas head]
    $body delete mark
    foreach key [array names flags namebg,*] {
        $body itemconfigure $flags($key) -fill white
    }
    foreach key [array names flags headbg,*] {
        $head itemconfigure $flags($key) -fill [$head cget -background]
    }
    set accent #b03060
    if {$flags(markrow) ne "" && [info exists flags(namebg,$flags(markrow))]} {
        $body itemconfigure $flags(namebg,$flags(markrow)) -fill #c3c3c3
        foreach r $flags(rows) {
            lassign $r y0 y1 what id
            if {$what eq "row" && $id eq $flags(markrow)} {
                set w [lindex [lindex $flags(layout) end] 2]
                $body create line 0 $y0 $w $y0 -fill $accent -tags mark
                $body create line 0 [expr {$y1 - 1}] $w [expr {$y1 - 1}] \
                    -fill $accent -tags mark
            }
        }
    }
    if {$flags(markcol) ne "" && [info exists flags(headbg,$flags(markcol))]} {
        $head itemconfigure $flags(headbg,$flags(markcol)) -fill #c3c3c3
        lassign [flags_track $flags(markcol)] x0 x1
        lassign [$body cget -scrollregion] - - - h
        if {$h eq ""} { set h [winfo height $body] }
        $body create line [expr {$x0 + 1}] 0 [expr {$x0 + 1}] $h \
            -fill $accent -width [flags_px 2] -tags mark
        $body create line [expr {$x1 - 1}] 0 [expr {$x1 - 1}] $h \
            -fill $accent -width [flags_px 2] -tags mark
    }
    flags_draw_hover
}

proc flags_click_body {x y} {
    global flags
    flags_tip_hide
    set t [flags_target body $x $y]
    switch -- [lindex $t 0] {
        band {
            set g [lindex $t 1]
            set flags(fold,$g) [expr {!([info exists flags(fold,$g)] && $flags(fold,$g))}]
            flags_redraw
        }
        name {
            set id [lindex $t 1]
            set flags(markrow) [expr {$flags(markrow) eq $id ? "" : $id}]
            flags_draw_marks
        }
    }
}

proc flags_click_head {x} {
    global flags
    flags_tip_hide
    set t [flags_target head $x 0]
    if {[lindex $t 0] ne "heading"} return
    set key [lindex $t 1]
    set flags(markcol) [expr {$flags(markcol) eq $key ? "" : $key}]
    flags_draw_marks
}

proc flags_fold_all {folded} {
    global flags
    foreach {g kind label} $flags(groups) { set flags(fold,$g) $folded }
    flags_redraw
}

# Double-click a heading: what the game says about that item, as the Items
# window's Inspect shows it.
proc flags_inspect {x} {
    global flags
    set t [flags_target head $x 0]
    if {[lindex $t 0] ne "heading"} return
    set key [lindex $t 1]
    if {![regexp {^s(\d+)$} $key - i]} return
    lassign [lindex [lindex $flags(data) 0] $i] slot part glyph colour sname fname
    if {$glyph eq ""} return
    if {[catch {angband_gear lore equipment $slot} lore]} return
    tk_messageBox -parent .character -icon info -title Inspect \
        -message [string toupper $fname 0 0] \
        -detail [regsub -all {[ \t]+\n} [string trim $lore] "\n"]
}

# --- pointing -----------------------------------------------------------------------

# What is under the pointer: {cell id key}, {name id}, {result id}, {band
# group}, {heading key} or {}.
proc flags_target {which x y} {
    global flags
    set c [flags_canvas $which]
    set cx [$c canvasx $x]
    set key ""
    foreach t $flags(layout) {
        lassign $t k x0 x1
        if {$cx >= $x0 && $cx < $x1} { set key $k ; break }
    }
    if {$which eq "head"} {
        if {$key in {"" name result}} { return {} }
        return [list heading $key]
    }
    set cy [$c canvasy $y]
    foreach r $flags(rows) {
        lassign $r y0 y1 what id
        if {$cy >= $y0 && $cy < $y1} {
            if {$what eq "band"} { return [list band $id] }
            switch -- $key {
                ""      { return {} }
                name    { return [list name $id] }
                result  { return [list result $id] }
                default { return [list cell $id $key] }
            }
        }
    }
    return {}
}

proc flags_motion {w x y X Y} {
    global flags character
    set which [expr {[string match *head $w] ? "head" : "body"}]
    set t [flags_target $which $x $y]
    if {$t eq $flags(hover)} return
    set flags(hover) $t
    flags_draw_hover
    if {![llength $t]} {
        flags_tip_hide
        flags_say_idle
        return
    }
    set character(msg) [flags_status $t]

    # The card: after a pause on a cell, name, result or heading; at once when
    # one is already up.  Bands get the status bar only.
    if {[lindex $t 0] eq "band"} { flags_tip_hide ; return }
    if {$flags(tipafter) ne ""} { after cancel $flags(tipafter) }
    if {$flags(tipshown)} {
        flags_tip_show $t $X $Y
    } else {
        set flags(tipafter) [after 450 [list flags_tip_show $t $X $Y]]
    }
}

proc flags_leave {} {
    global flags
    set flags(hover) {}
    flags_draw_hover
    flags_tip_hide
    flags_say_idle
}

# The pointed-at row's name and column's heading turn light grey, and the cell
# gets a thin ring, so you can see where you are.
proc flags_draw_hover {} {
    global flags
    set body [flags_canvas body]
    set head [flags_canvas head]
    if {![winfo exists $body]} return
    $body delete hover
    foreach key [array names flags namebg,*] {
        set id [string range $key 7 end]
        $body itemconfigure $flags($key) \
            -fill [expr {$id eq $flags(markrow) ? "#c3c3c3" : "white"}]
    }
    foreach key [array names flags headbg,*] {
        set k [string range $key 7 end]
        $head itemconfigure $flags($key) -fill [expr {$k eq $flags(markcol)
            ? "#c3c3c3" : [$head cget -background]}]
    }
    set t $flags(hover)
    set id "" ; set key ""
    switch -- [lindex $t 0] {
        cell    { lassign $t - id key }
        name - result { set id [lindex $t 1] }
        heading { set key [lindex $t 1] }
    }
    if {$id ne "" && $id ne $flags(markrow) && [info exists flags(namebg,$id)]} {
        $body itemconfigure $flags(namebg,$id) -fill #ececec
    }
    if {$key ne "" && $key ne $flags(markcol) && [info exists flags(headbg,$key)]} {
        $head itemconfigure $flags(headbg,$key) -fill #ececec
    }
    if {[lindex $t 0] eq "cell"} {
        lassign [flags_track $key] x0 x1
        foreach r $flags(rows) {
            lassign $r y0 y1 what rid
            if {$what eq "row" && $rid eq $id} {
                $body create rectangle [expr {$x0 + 1}] [expr {$y0 + 1}] \
                    [expr {$x1 - 1}] [expr {$y1 - 1}] -outline black -tags hover
            }
        }
    }
}

# --- explaining ---------------------------------------------------------------------

proc flags_say_idle {} {
    global character
    set character(msg) "Point at a row, a cell or a heading to see what it\
        means. Click a heading to mark its column."
}

proc flags_say_show {key} {
    global character
    set character(msg) [dict get {
        all     "Show all: every row on the page."
        effect  "Show in effect: only the rows that are doing something for or to you right now."
        against "Show against you: only the rows where something works against you: a vulnerability or a drawback."
        unknown "Show not yet learned: only the rows whose rune you haven't learned, or that could be hiding on an item."
    } $key]
}

# The row's description, quoted from the manual's list of properties: the help
# page is read for the paragraph under the row's own name in the section that
# lists them.  Empty when the manual has no entry, which the bridge test
# catches before a player does.
proc flags_description {label} {
    global flags
    if {![info exists flags(desc)]} {
        set flags(desc) {}
        set file [file join $flags(dir) help flags.txt]
        if {[info exists ::helpwin(dir)]} {
            set file [file join $::helpwin(dir) flags.txt]
        }
        if {![catch {open $file} ch]} {
            fconfigure $ch -encoding utf-8
            set in 0 ; set term "" ; set text ""
            foreach line [split [read $ch] \n] {
                set fields [split $line \t]
                set op [lindex $fields 0]
                if {$op eq "M" && [lindex $fields 1] eq "what-each-property-does"} {
                    set in 1
                }
                if {!$in || $op ne "T"} continue
                set tags [lindex $fields 1]
                set body [string map {\\n \n \\t \t \\\\ \\} [lindex $fields 2]]
                if {[string match "dt*" $tags]} {
                    if {$term ne "" && $text ne ""} {
                        dict set flags(desc) $term [flags_plain [string trim $text]]
                        set text ""
                    }
                    if {[string trim $body] ne ""} {
                        set term [flags_plain [string trim $body]]
                    }
                } elseif {[string match "p1*" $tags] && $term ne ""} {
                    append text $body
                }
            }
            if {$term ne "" && $text ne ""} {
                dict set flags(desc) $term [flags_plain [string trim $text]]
            }
            close $ch
        }
    }
    set label [flags_plain $label]
    if {[dict exists $flags(desc) $label]} {
        return [regsub -all {\s+} [dict get $flags(desc) $label] " "]
    }
    return ""
}

# Sphinx sets the manual with typographic quotes; the windows use plain ones.
proc flags_plain {text} {
    return [string map [list \u2019 ' \u2018 ' \u201c \" \u201d \"] $text]
}

# Where the character's own value comes from, as a phrase: "as a Dwarf".
proc flags_origin_phrase {origin} {
    if {[catch {angband_player} v]} { set v {} }
    set race [expr {[dict exists $v race] ? [dict get $v race] : "your race"}]
    set class [expr {[dict exists $v class] ? [dict get $v class] : "your class"}]
    set parts {}
    if {[dict get $origin race]} {
        lappend parts "as [flags_article $race]"
    } elseif {[dict get $origin racelevel]} {
        lappend parts "as [flags_article $race], since level [dict get $origin racelevel]"
    }
    if {[dict get $origin class]} {
        lappend parts "as [flags_article $class]"
    } elseif {[dict get $origin classlevel]} {
        lappend parts "as [flags_article $class], since level [dict get $origin classlevel]"
    }
    if {[dict get $origin shape]} { lappend parts "in your current shape" }
    if {![llength $parts]} { return "from your race, class or level" }
    return [join $parts " and "]
}

proc flags_article {noun} {
    return [expr {[string match -nocase {[aeiou]*} $noun] ? "an $noun" : "a $noun"}]
}

# The one-line phrase for what a cell does.
proc flags_cell_phrase {kind state label v} {
    set lc [string tolower $label]
    switch -- $state {
        has {
            switch -- $kind {
                res     { return "resists $lc" }
                mod     { return "[flags_signed $v] to $lc" }
                draw    { return "protects you from it" }
                default { return "gives you this" }
            }
        }
        immune   { return "makes you immune to $lc" }
        against  {
            switch -- $kind {
                res     { return "makes you vulnerable to $lc" }
                mod     { return "[flags_signed $v] to $lc" }
                default { return "takes it away" }
            }
        }
        drawback { return "puts this drawback on you" }
        nothing  { return "doesn't give this" }
        default  { return "" }
    }
}

# Everything about one row, for the status bar and the card: the sources, as
# {state slotname itemname curse key}, and the answer in words.
proc flags_row_story {id} {
    global flags
    lassign [dict get $flags(rowinfo) $id] group kind row res
    lassign $row label known values auxs renderer result without curses origin timed
    lassign $flags(data) cols
    set n [llength $cols]
    set sources {}
    for {set j 0} {$j < $n} {incr j} {
        lassign [lindex $cols $j] slot part glyph colour sname
        if {$glyph eq ""} continue
        set state [flags_cell_state $kind [lindex $values $j]]
        if {$state in {nothing na empty}} continue
        lassign [flags_slot_info $part] short long
        set name [expr {$state eq "unknown" ? "$sname: not identified" : $sname}]
        lappend sources [list $state $long $name [lindex $curses $j] s$j \
            [lindex $values $j]]
    }
    set state [flags_cell_state $kind [lindex $values $n]]
    if {$state ni {nothing na empty}} {
        lappend sources [list $state You [flags_origin_phrase $origin] "" you \
            [lindex $values $n]]
    }
    foreach te $timed {
        lassign $te tname tdesc turns
        lappend sources [list timed Timed \
            "[flags_timed_name $tname $tdesc], $turns turns left" "" timed \
            [lindex $auxs $n]]
    }
    return [list $label $kind $res $sources $known]
}

# The answer: what the row comes to, and what that means for taking things off.
proc flags_answer {id} {
    lassign [flags_row_story $id] label kind res sources known
    lassign $res state word
    set lc [string tolower $label]
    set good {} ; set bad {} ; set fornow {}
    foreach s $sources {
        lassign $s st slot name curse key
        switch -- $st {
            has - immune { lappend good $s }
            against - drawback { lappend bad $s }
            timed { lappend fornow $s }
        }
    }
    set lead ""
    switch -- $kind {
        res {
            set lead [dict get {
                immune "You're immune to %s."
                has "You resist %s."
                timed "You resist %s for now."
                against "You're vulnerable to %s."
                nothing "You don't resist %s."
                unknown "Not known."
            } $state]
            set lead [format $lead $lc]
        }
        abil {
            set lead [dict get {
                has "You have this."
                timed "You have this for now."
                against "Something you wear takes it away."
                nothing "You don't have this."
                unknown "Not known."
            } $state]
        }
        draw {
            set lead [dict get {
                drawback "This is affecting you."
                timed "This is affecting you for now."
                nothing "Nothing is putting this on you."
                unknown "Not known."
            } $state]
        }
        mod {
            set lead [expr {$state eq "nothing" ? "Nothing changes this."
                : $state eq "unknown" ? "Not known."
                : "Your total is $word."}]
        }
    }

    set more ""
    if {$state eq "unknown"} {
        set more "Something you wear has a rune you haven't learned, and it\
            could be this one."
    } elseif {$kind eq "res" && [llength $good] && [llength $bad]
            && $state in {timed nothing}} {
        # The cancelled case, which is the reason the Result column exists.
        set culprit [lindex $bad 0]
        set others [expr {[llength $good] == 1 ? "the other one"
            : "the other [flags_count_word [llength $good]]"}]
        set what "Your [lindex $culprit 2] makes you vulnerable, which cancels\
            the resistance from $others"
        if {$state eq "timed" && [llength $fornow]} {
            set tname [lindex [split [lindex [lindex $fornow 0] 2] ,] 0]
            set more "Resisted for now, by $tname. $what, so once it wears\
                off you won't resist $lc."
            set lead ""
        } else {
            set more "$what."
        }
    } elseif {$state in {has immune drawback} || ($kind eq "mod" && $state eq "has")} {
        set givers [expr {$state eq "drawback" ? $bad : $good}]
        if {[llength $givers] == 1} {
            set who [lindex $givers 0]
            set more [expr {[lindex $who 1] eq "You"
                ? "It comes from you, [lindex $who 2], so taking things off\
                   can't lose it."
                : "Your [lindex $who 2] is the only source."}]
        } elseif {[llength $givers] > 1 && $kind ne "mod"} {
            set more "[string totitle [flags_count_word [llength $givers]]]\
                things give it, so you could lose one and keep it."
        }
    } elseif {$state eq "timed" && [llength $fornow]} {
        set more "It comes from [lindex [split [lindex [lindex $fornow 0] 2] ,] 0],\
            and it wears off."
    }
    return [string trim "$lead $more"]
}

proc flags_count_word {n} {
    set words {zero one two three four five six seven eight nine ten eleven twelve}
    return [expr {$n < [llength $words] ? [lindex $words $n] : $n}]
}

# What taking slot `key` off would change: lines of {sign row from to}, where
# sign is - (lose), + (gain), = (no change) or ?.
proc flags_column_changes {key} {
    global flags
    if {![regexp {^s(\d+)$} $key - j]} { return {} }
    lassign $flags(data) cols groups
    set changes {}
    foreach {group rows} $groups {
        set kind [flags_kind $group]
        foreach row $rows {
            lassign $row label known values auxs renderer result without
            set cell [flags_cell_state $kind [lindex $values $j]]
            set now [flags_result $kind $result]
            set then [flags_result $kind [lindex $without $j]]
            set rn [lindex $now 2] ; set rt [lindex $then 2]
            if {$rn == $rt} {
                if {$cell ni {nothing na empty unknown}} {
                    lappend changes [list = $label]
                }
                continue
            }
            set from [flags_word_lc [lindex $now 1]]
            set to [flags_word_lc [lindex $then 1]]
            if {$kind eq "mod"} {
                set text "$label $from → $to"
            } else {
                set text "$label: $from → $to"
            }
            lappend changes [list [expr {$rt < $rn ? "-" : "+"}] $text $label \
                $kind $rn $rt]
        }
    }
    return $changes
}

# The status-bar line for whatever is under the pointer.
proc flags_status {t} {
    global flags
    lassign $flags(data) cols groups
    switch -- [lindex $t 0] {
        band {
            set g [lindex $t 1]
            set rows [dict get $groups $g]
            set folded [expr {[info exists flags(fold,$g)] && $flags(fold,$g)}]
            return "[flags_group_label $g]: [flags_band_summary $g [flags_kind $g] $rows].\
                Click to [expr {$folded ? "unfold" : "fold"}]."
        }
        name - result {
            lassign [flags_row_story [lindex $t 1]] label
            return "$label: [flags_answer [lindex $t 1]]"
        }
        heading {
            set key [lindex $t 1]
            if {$key eq "you"} {
                return "You: what your race, class and level give you. You can't take these off."
            }
            if {$key eq "timed"} {
                return "Timed: what potions, scrolls and spells are giving you right now."
            }
            regexp {^s(\d+)$} $key - j
            lassign [lindex $cols $j] slot part glyph colour sname fname
            lassign [flags_slot_info $part] short long
            if {$glyph eq ""} { return "$long: nothing worn." }
            set lose {} ; set lower {} ; set improve {} ; set lift {}
            foreach ch [flags_column_changes $key] {
                lassign $ch sign text label kind
                if {$sign eq "-"} {
                    if {$kind eq "mod"} { lappend lower $label } else { lappend lose $label }
                } elseif {$sign eq "+"} {
                    if {$kind eq "draw"} { lappend lift $label } else { lappend improve $label }
                }
            }
            set parts {}
            if {[llength $lose]} { lappend parts "loses [flags_list $lose]" }
            if {[llength $lower]} { lappend parts "lowers [flags_list $lower]" }
            if {[llength $improve]} { lappend parts "improves [flags_list $improve]" }
            if {[llength $lift]} { lappend parts "lifts [flags_list $lift]" }
            if {![llength $parts]} {
                return "$long: $fname. Taking it off changes nothing here."
            }
            return "$long: $fname. Taking it off [flags_list $parts]."
        }
        cell {
            lassign $t - id key
            lassign [dict get $flags(rowinfo) $id] group kind row
            lassign $row label known values auxs renderer result without curses origin timed
            set n [llength $cols]
            if {$key eq "timed"} {
                if {![llength $timed]} { return "$label · Timed: nothing for now." }
                set te [lindex $timed 0]
                return "$label · [flags_timed_name {*}[lrange $te 0 1]]: for\
                    now, [lindex $te 2] turns left. It wears off."
            }
            if {$key eq "you"} {
                set v [lindex $values $n]
                set state [flags_cell_state $kind $v]
                if {$state eq "na"} { return "$label · You: your race and class can't give this." }
                if {$state eq "nothing"} { return "$label · You: your race and class don't give this." }
                return "$label · You: [flags_cell_phrase $kind $state $label [flags_num $v]],\
                    [flags_origin_phrase $origin]."
            }
            regexp {^s(\d+)$} $key - j
            lassign [lindex $cols $j] slot part glyph colour sname
            lassign [flags_slot_info $part] short long
            if {$glyph eq ""} { return "$long: nothing worn." }
            set v [lindex $values $j]
            set state [flags_cell_state $kind $v]
            if {$state eq "unknown"} {
                return "$label · $sname: not known. It has a rune you haven't\
                    learned, and this could be it."
            }
            if {$state eq "na"} { return "$label · $sname: can't give this." }
            set curse [lindex $curses $j]
            return "$label · $sname: [flags_cell_phrase $kind $state $label [flags_num $v]][expr {$curse ne "" ? " (curse: $curse)" : ""}]."
        }
    }
    return ""
}

# "a, b and c".
proc flags_list {items} {
    if {[llength $items] <= 1} { return [join $items] }
    return "[join [lrange $items 0 end-1] {, }] and [lindex $items end]"
}

# --- the card -----------------------------------------------------------------------

proc flags_tip_hide {} {
    global flags
    if {$flags(tipafter) ne ""} {
        after cancel $flags(tipafter)
        set flags(tipafter) ""
    }
    set flags(tipshown) 0
    if {[winfo exists .flagtip]} { wm withdraw .flagtip }
}

proc flags_tip_show {t X Y} {
    global flags
    set flags(tipafter) ""
    if {![winfo exists .character]} return
    if {![winfo exists .flagtip]} {
        toplevel .flagtip -background black -borderwidth 0
        wm overrideredirect .flagtip 1
        wm withdraw .flagtip
        catch {wm attributes .flagtip -topmost 1}
    }
    destroy {*}[winfo children .flagtip]
    set paper #ffffe0
    set f [frame .flagtip.f -background $paper -padx 8 -pady 6]
    pack $f -padx 1 -pady 1
    set wrap [flags_px 420]
    set f12 [uifont 12]

    switch -- [lindex $t 0] {
        heading { flags_card_column $f [lindex $t 1] $paper $wrap }
        default { flags_card_row $f $t $paper $wrap }
    }

    update idletasks
    set w [winfo reqwidth .flagtip]
    set h [winfo reqheight .flagtip]
    set dx [flags_px 16]
    set dy [flags_px 20]
    set x [expr {$X + $dx}]
    set y [expr {$Y + $dy}]
    set right [expr {[winfo rootx .character] + [winfo width .character]}]
    set bottom [expr {[winfo rooty .character] + [winfo height .character]}]
    if {$x + $w > $right} { set x [expr {$X - $dx - $w}] }
    if {$y + $h > $bottom} { set y [expr {$Y - $dy - $h}] }
    wm geometry .flagtip +$x+$y
    wm deiconify .flagtip
    raise .flagtip
    set flags(tipshown) 1
}

proc flags_card_rule {f paper} {
    frame $f.r[llength [winfo children $f]] -height 1 -background #c8c4a0
}

# A cell's colour and mark, small, for a line of a card.
proc flags_card_swatch {w paper tint mark ink} {
    canvas $w -width [flags_px 22] -height [flags_px 16] -highlightthickness 0 \
        -borderwidth 0 -background $paper
    $w create rectangle [flags_px 2] 1 [flags_px 20] [flags_px 15] -fill $tint \
        -outline #a3a3a3
    $w create text [flags_px 11] [flags_px 8] -text $mark -fill $ink \
        -font [uifont Courier 11 bold]
    return $w
}

proc flags_card_row {f t paper wrap} {
    global flags
    set id [lindex $t 1]
    set key [expr {[lindex $t 0] eq "cell" ? [lindex $t 2] : ""}]
    lassign [flags_row_story $id] label kind res sources known
    lassign [dict get $flags(rowinfo) $id] group

    frame $f.title -background $paper
    label $f.title.l -text $label -font [uifont 12 bold] -background $paper
    label $f.title.g -text [flags_group_label $group] -font [uifont 12] \
        -foreground #5a5a5a -background $paper
    pack $f.title.l -side left
    pack $f.title.g -side right -padx {24 0}
    pack $f.title -fill x

    if {!$known} {
        label $f.note -text "You haven't learned this rune yet. You'll\
            recognise it on an item the first time you notice it." \
            -font [uifont 12 italic] -background $paper -wraplength $wrap \
            -justify left -anchor w
        pack $f.note -fill x -pady {2 0}
    }
    set desc [flags_description $label]
    if {$desc ne ""} {
        label $f.desc -text $desc -font [uifont 12] -background $paper \
            -wraplength $wrap -justify left -anchor w
        pack $f.desc -fill x -pady {4 0}
    }

    # The sources, one line each, the pointed-at one picked out.
    if {[llength $sources] || $key ne ""} {
        pack [flags_card_rule $f $paper] -fill x -pady 6
        set s [frame $f.src -background $paper]
        pack $s -fill x
        set r 0
        foreach src $sources {
            lassign $src state slot name curse skey v
            set here [expr {$skey eq $key}]
            set bg [expr {$here ? "#f0e08c" : $paper}]
            lassign [flags_style $state] mark ink tint
            if {$kind eq "mod" && [string is integer -strict $v] && $v != 0} {
                set mark [flags_signed $v]
            }
            # One band across the whole line when it is the pointed-at one:
            # the cells touch, and the gaps are each label's own padding.
            set font [expr {$here ? [uifont 12 bold] : [uifont 12]}]
            label $s.s$r -text $slot -font $font -background $bg \
                -foreground [expr {$here ? "black" : "#5a5a5a"}] -anchor w \
                -padx 6 -pady 1
            label $s.n$r -text [expr {$curse ne "" ? "$name (curse: $curse)" : $name}] \
                -font $font -background $bg -anchor w -justify left \
                -padx 4 -pady 1 -wraplength [expr {$wrap - [flags_px 40]}]
            # The swatch is as tall as its line, with the mark in the middle.
            set h [winfo reqheight $s.s$r]
            set m [expr {$h / 2}]
            set sw [canvas $s.w$r -width [flags_px 26] -height $h \
                -highlightthickness 0 -borderwidth 0 -background $bg]
            $sw create rectangle [flags_px 4] [expr {$m - [flags_px 7]}] \
                [flags_px 22] [expr {$m + [flags_px 7]}] \
                -fill $tint -outline #a3a3a3
            $sw create text [flags_px 13] $m -text $mark -fill $ink \
                -font [uifont Courier 11 bold]
            grid $sw $s.s$r $s.n$r -row $r -sticky nsew
            incr r
        }
        grid columnconfigure $s 2 -weight 1
    }

    set answer [flags_answer $id]
    if {$answer ne ""} {
        pack [flags_card_rule $f $paper] -fill x -pady 6
        label $f.ans -text $answer -font [uifont 12] -background $paper \
            -wraplength $wrap -justify left -anchor w
        pack $f.ans -fill x
    }
}

proc flags_card_column {f key paper wrap} {
    global flags
    lassign $flags(data) cols groups
    if {$key in {you timed}} {
        set title [expr {$key eq "you" ? "You" : "Timed"}]
        label $f.t -text $title -font [uifont 12 bold] -background $paper -anchor w
        pack $f.t -fill x
        label $f.d -text [expr {$key eq "you"
            ? "What your race, class and level give you. You can't take these off."
            : "What potions, scrolls and spells are giving you right now. They wear off on their own."}] \
            -font [uifont 12] -background $paper -wraplength $wrap -justify left -anchor w
        pack $f.d -fill x -pady {2 0}
        set lines {}
        foreach {group rows} $groups {
            set kind [flags_kind $group]
            foreach row $rows {
                lassign $row label known values auxs
                set n [expr {[llength $values] - 1}]
                if {$key eq "you"} {
                    set state [flags_cell_state $kind [lindex $values $n]]
                    if {$state in {has immune against drawback}} {
                        lappend lines [list $state $label [lindex $values $n] $kind]
                    }
                } elseif {[flags_num [lindex $auxs $n]] != 0} {
                    lappend lines [list timed $label [lindex $auxs $n] $kind]
                }
            }
        }
        if {[llength $lines]} {
            pack [flags_card_rule $f $paper] -fill x -pady 6
            set s [frame $f.l -background $paper]
            pack $s -fill x
            set r 0
            foreach line $lines {
                lassign $line state label v kind
                lassign [flags_style $state] mark ink tint
                if {$kind eq "mod"} { set mark [flags_signed $v] }
                set sw [flags_card_swatch $s.w$r $paper $tint $mark $ink]
                label $s.n$r -text $label -font [uifont 12] -background $paper -anchor w
                grid $sw $s.n$r -row $r -sticky w -pady 1
                grid configure $s.n$r -padx {6 0}
                incr r
            }
        }
        return
    }

    regexp {^s(\d+)$} $key - j
    lassign [lindex $cols $j] slot part glyph colour sname fname runes curses
    lassign [flags_slot_info $part] short long
    if {$glyph eq ""} {
        label $f.t -text "$long: nothing worn" -font [uifont 12 bold] \
            -background $paper -anchor w
        pack $f.t -fill x
        label $f.d -text "Wear something here and this column fills in." \
            -font [uifont 12] -background $paper -anchor w
        pack $f.d -fill x -pady {2 0}
        return
    }
    label $f.t -text "$long: $fname" -font [uifont 12 bold] -background $paper \
        -wraplength $wrap -justify left -anchor w
    pack $f.t -fill x
    set notes {}
    if {[llength $curses]} { lappend notes "Cursed: [flags_list $curses]." }
    if {!$runes} { lappend notes "It has runes you haven't learned." }
    if {[llength $notes]} {
        label $f.n -text [join $notes " "] -font [uifont 12 italic] \
            -background $paper -wraplength $wrap -justify left -anchor w
        pack $f.n -fill x -pady {2 0}
    }

    set changes [flags_column_changes $key]
    set lose {} ; set gain {} ; set same {}
    foreach ch $changes {
        switch -- [lindex $ch 0] {
            - { lappend lose [lindex $ch 1] }
            + { lappend gain [lindex $ch 1] }
            = { lappend same [lindex $ch 1] }
        }
    }
    if {![llength $lose] && ![llength $gain] && $runes} {
        pack [flags_card_rule $f $paper] -fill x -pady 6
        label $f.none -text "Taking it off would change nothing here: other\
            things you wear cover all of it." -font [uifont 12] \
            -background $paper -wraplength $wrap -justify left -anchor w
        pack $f.none -fill x
    } else {
        label $f.if -text "If you take it off:" -font [uifont 12] \
            -background $paper -anchor w
        pack $f.if -fill x -pady {4 0}
        pack [flags_card_rule $f $paper] -fill x -pady 6
        set s [frame $f.c -background $paper]
        pack $s -fill x
        set r 0
        foreach {state sign word items} [list \
                against − "You'd lose" $lose \
                has     + "You'd gain" $gain \
                nothing = "No change" [expr {[llength $same] ? [list [join $same {, }]] : {}}] \
                unknown ? "Unknown" [expr {$runes ? {} : {{whatever it has that you haven't learned yet}}}]] {
            set first 1
            foreach item $items {
                lassign [flags_style $state] mark ink tint
                set sw [flags_card_swatch $s.w$r $paper \
                    [expr {$state eq "nothing" ? "white" : $tint}] $sign \
                    [expr {$ink eq "" || $state eq "nothing" ? "black" : $ink}]]
                label $s.k$r -text [expr {$first ? $word : ""}] -font [uifont 12] \
                    -foreground #5a5a5a -background $paper -anchor w
                label $s.v$r -text $item -font [uifont 12] -background $paper \
                    -anchor w -justify left \
                    -wraplength [expr {$wrap - [flags_px 140]}]
                grid $sw $s.k$r $s.v$r -row $r -sticky w -pady 1
                grid configure $s.k$r -padx {6 10}
                set first 0
                incr r
            }
        }
        grid columnconfigure $s 2 -weight 1
    }
    pack [flags_card_rule $f $paper] -fill x -pady 6
    label $f.hint -text "Click to mark this column; double-click to inspect the item." \
        -font [uifont 12] -foreground #5a5a5a -background $paper -anchor w
    pack $f.hint -fill x
}

# --- help ---------------------------------------------------------------------------

# The manual's page for this, in the Help window.  Pointing at a row sends you
# to the list of properties at its group; otherwise to how to read the page.
proc flags_help {} {
    global flags
    set where flags#the-flags-page
    set t $flags(hover)
    if {[lindex $t 0] in {cell name result}} {
        set group [lindex [dict get $flags(rowinfo) [lindex $t 1]] 0]
        set where flags#[string tolower [flags_group_label $group]]
    }
    help_window [help_first $where flags]
}
