# The Items window: the paper doll and the pack.
#
# Two panes of one thing.  On the left, twelve equipment slots ranged around a
# pen-and-ink figure, each tied to it by a leader line; on the right, the pack,
# in the game's own order with the game's own selection letters.  Items move
# between them by dragging, and the keyboard commands keep working throughout.
# The spec is in .claude/plans/paper_doll, and it is drawn to the same
# Classical design system as the character sheet.
#
# Two things about it are worth knowing before reading the rest.
#
# The game stays authoritative.  Nothing here moves an item: a drop pushes the
# command the keyboard would have pushed and then waits to be told what
# happened, so the turn cost, the curse checks and the messages are the game's.
# That is why the message line reports only what a re-read confirms.
#
# The figure is vector line art, not a bitmap.  Its path data is the design's,
# transcribed from the handoff's SVG and flattened onto a canvas by
# items_flatten below, so a change to the drawing is a change to a string.

set items(dash) "—"
set items(message) ""
set items(burden) ""
set items(inscription) ""
set items(sel) {}
set items(expect) {}
set items(expectafter) ""
set items(drag) {}
set items(lit) {}

# When to read again.  Equipment and inventory for the obvious reason, stats
# because carrying capacity is strength, and the two birth events because the
# window can be open before there is anything in it.
set items(events) {
    INVENTORY EQUIPMENT STATS ENTER_GAME LEAVE_BIRTH ENTER_WORLD
}

# --- the slots ----------------------------------------------------------------
#
# The design's order, by slot type: head down the left, light down the right,
# so the leader lines meet the figure where the gear would be.  This is a
# preference and not a requirement -- a body with different slots, which
# shapechange can produce, keeps the game's own order for whatever is left
# over rather than losing a row.
set items(order,left)  {hat amulet weapon bow ring boots}
set items(order,right) {light body_armor cloak shield ring gloves}

# What to call a slot.  The game's own body-part names are "weapon", "shooting"
# and "back", which are the places rather than the things; these are the
# design's words for them.  Anything unlisted falls back to the body part,
# which is always right if not always graceful.
foreach {type text} {
    weapon      "Wielding"
    bow         "Shooting"
    amulet      "Neck"
    light       "Light"
    body_armor  "Body"
    cloak       "Cloak"
    shield      "Shield arm"
    hat         "Head"
    gloves      "Hands"
    boots       "Feet"
} { set items(label,$type) $text }

# What an *empty* slot shows.  Not the game's business: an empty slot has no
# object and so no symbol, but a doll with nine blank boxes reads as broken
# rather than as unequipped.  These are the symbols the game uses for the kinds
# of thing that go there.
foreach {type text} {
    weapon      "|"
    bow         "\}"
    ring        "="
    amulet      "\""
    light       "~"
    body_armor  "\["
    cloak       "("
    shield      ")"
    hat         "\]"
    gloves      "\]"
    boots       "\]"
} { set items(slotglyph,$type) $text }

# --- the figure ---------------------------------------------------------------
#
# The handoff's SVG, path by path, in document order: the cloak behind, the
# body over it, the hatching, then the contours.  Two line weights and two
# fills, as the design says and no more -- the hatching down both cloak edges
# is what makes it read as pen and ink rather than as an outline, and is not
# optional.
#
# {kind colour width path}
set items(figure,paths) {
    {fill   #eae7e7 0   {M46 82 q-20 40 -25 104 -4 54 -4 96 l28 3 q-3 -66 5 -116 7 -50 14 -80 z}}
    {fill   #eae7e7 0   {M94 82 q20 40 25 104 4 54 4 96 l-28 3 q3 -66 -5 -116 -7 -50 -14 -80 z}}
    {fill   #f8f4f4 0   {M70 15 q20 0 20 25 0 26 -20 28 -20 -2 -20 -28 0 -25 20 -25 z}}
    {fill   #f8f4f4 0   {M50 84 q20 -12 40 0 l9 30 -3 40 h-52 l-3 -40 z}}
    {fill   #f8f4f4 0   {M47 154 h46 l7 42 h-60 z}}
    {fill   #f8f4f4 0   {M56 196 h11 l1 92 h-13 z M73 196 h11 l1 92 h-13 z}}
    {stroke #7d7979 1   {M42 118 l-11 24 M40 146 l-12 24 M38 176 l-13 24 M37 208 l-13 22 M36 240 l-12 20 M36 268 l-12 16}}
    {stroke #7d7979 1   {M98 118 l11 24 M100 146 l12 24 M102 176 l13 24 M103 208 l13 22 M104 240 l12 20 M104 268 l12 16}}
    {stroke #2d2b2b 1.6 {M46 82 q-20 40 -25 104 -4 54 -4 96 l28 3}}
    {stroke #2d2b2b 1.6 {M94 82 q20 40 25 104 4 54 4 96 l-28 3}}
    {stroke #2d2b2b 1.6 {M50 38 q0 -27 20 -27 20 0 20 27}}
    {stroke #2d2b2b 1.6 {M50 38 q-4 18 2 26 M90 38 q4 18 -2 26}}
    {stroke #2d2b2b 1.6 {M50 42 q4 26 20 26 16 0 20 -26}}
    {stroke #2d2b2b 1.6 {M58 44 q4 -4 9 0 M73 44 q5 -4 9 0}}
    {stroke #2d2b2b 1.6 {M70 50 v8 M63 62 q7 5 14 0}}
    {stroke #2d2b2b 1.6 {M60 68 q10 7 20 0 l12 7 q11 5 13 21 l-4 44}}
    {stroke #2d2b2b 1.6 {M60 68 l-12 7 q-11 5 -13 21 l4 44}}
    {stroke #2d2b2b 1.6 {M39 140 q-4 14 4 19 9 4 12 -6 l2 -14}}
    {stroke #2d2b2b 1.6 {M101 140 q4 14 -4 19 -9 4 -12 -6 l-2 -14}}
    {stroke #7d7979 1   {M43 112 q27 10 54 0}}
    {stroke #7d7979 1   {M70 78 v72}}
    {stroke #2d2b2b 1.6 {M44 150 h52 v14 h-52 z}}
    {stroke #7d7979 1   {M63 150 h14 v14 h-14 z}}
    {stroke #2d2b2b 1.6 {M46 164 l-5 32 q29 9 58 0 l-5 -32}}
    {stroke #2d2b2b 1.6 {M55 196 l1 82 M69 198 v80 M84 196 l-1 82}}
    {stroke #7d7979 1   {M58 226 q12 5 24 0}}
    {stroke #2d2b2b 1.6 {M56 278 q7 4 13 0 M71 278 q6 4 13 0}}
    {stroke #2d2b2b 1.6 {M56 278 l-4 24 q10 6 21 2 l-3 -26 M84 278 l4 24 q-10 6 -21 2 l3 -26}}
    {stroke #7d7979 1   {M52 302 h22 M66 302 h22}}
}

# The design's viewBox and its rendered size.
set items(figure,w) 140
set items(figure,h) 330
set items(figure,scale) 0.843

# An SVG path to a list of polylines.
#
# Only the commands the figure uses: absolute M, relative l h v q, and z.  A
# curve becomes eight segments, which at this size is smooth and is a tenth of
# the canvas items that Tk's own -smooth would cost.
#
# Tk has no path item, so this is the whole of the SVG support there is going
# to be; a path with a command not listed here silently loses it, which is why
# the figure data above is transcribed rather than fetched.
proc items_flatten {d} {
    set subpaths {}
    set pts {}
    set x 0.0
    set y 0.0
    set startx 0.0
    set starty 0.0
    set cmd ""

    set toks [regexp -all -inline {[A-Za-z]|-?[0-9]*\.?[0-9]+} $d]
    set n [llength $toks]
    set i 0

    while {$i < $n} {
        set tok [lindex $toks $i]

        if {[regexp {^[A-Za-z]$} $tok]} {
            set cmd $tok
            incr i
            if {$cmd eq "z" || $cmd eq "Z"} {
                if {[llength $pts] >= 4} {
                    lappend pts $startx $starty
                    lappend subpaths $pts
                }
                set pts {}
                set x $startx
                set y $starty
            }
            continue
        }

        switch -exact -- $cmd {
            M {
                if {[llength $pts] >= 4} { lappend subpaths $pts }
                set x [lindex $toks $i]
                set y [lindex $toks [expr {$i + 1}]]
                incr i 2
                set pts [list $x $y]
                set startx $x
                set starty $y
                # A pair after an M is a lineto, in SVG and here.
                set cmd L
            }
            L {
                set x [lindex $toks $i]
                set y [lindex $toks [expr {$i + 1}]]
                incr i 2
                lappend pts $x $y
            }
            l {
                set x [expr {$x + [lindex $toks $i]}]
                set y [expr {$y + [lindex $toks [expr {$i + 1}]]}]
                incr i 2
                lappend pts $x $y
            }
            h { set x [expr {$x + [lindex $toks $i]}] ; incr i ; lappend pts $x $y }
            v { set y [expr {$y + [lindex $toks $i]}] ; incr i ; lappend pts $x $y }
            q {
                set cx [expr {$x + [lindex $toks $i]}]
                set cy [expr {$y + [lindex $toks [expr {$i + 1}]]}]
                set ex [expr {$x + [lindex $toks [expr {$i + 2}]]}]
                set ey [expr {$y + [lindex $toks [expr {$i + 3}]]}]
                incr i 4
                for {set s 1} {$s <= 8} {incr s} {
                    set t [expr {$s / 8.0}]
                    set u [expr {1.0 - $t}]
                    lappend pts \
                        [expr {$u * $u * $x + 2 * $u * $t * $cx + $t * $t * $ex}] \
                        [expr {$u * $u * $y + 2 * $u * $t * $cy + $t * $t * $ey}]
                }
                set x $ex
                set y $ey
            }
            default { incr i }
        }
    }

    if {[llength $pts] >= 4} { lappend subpaths $pts }
    return $subpaths
}

proc items_figure_draw {c} {
    global items

    set scale $items(figure,scale)
    $c delete all

    foreach spec $items(figure,paths) {
        lassign $spec kind colour width d
        foreach sub [items_flatten $d] {
            set coords {}
            foreach {px py} $sub {
                lappend coords [expr {$px * $scale}] [expr {$py * $scale}]
            }
            if {$kind eq "fill"} {
                if {[llength $coords] >= 6} {
                    $c create polygon {*}$coords -fill $colour -outline {}
                }
            } elseif {[llength $coords] >= 4} {
                $c create line {*}$coords -fill $colour -width $width \
                    -capstyle round -joinstyle round
            }
        }
    }
}

# --- reading the game ----------------------------------------------------------

# Tenths of a pound, which is how the game stores every weight, as the one
# decimal the design asks for.
proc items_weight {tenths} {
    if {![string is integer -strict $tenths]} { return "0.0" }
    return [format "%d.%d" [expr {$tenths / 10}] [expr {$tenths % 10}]]
}

proc items_gear {what args} {
    if {[catch {angband_gear $what {*}$args} result]} { return {} }
    return $result
}

# The slots in the order the design draws them: a template of types, filled
# from the body the character actually has, with anything unmatched appended.
# Returns two lists of rows, left and right.
proc items_arrange {rows} {
    global items

    set pool $rows
    set out {}

    foreach side {left right} {
        set column {}
        foreach type $items(order,$side) {
            set found -1
            set i 0
            foreach row $pool {
                if {[lindex $row 7] eq $type} { set found $i ; break }
                incr i
            }
            if {$found >= 0} {
                lappend column [lindex $pool $found]
                set pool [lreplace $pool $found $found]
            }
        }
        lappend out $column
    }

    # Whatever the template did not name, split evenly.  This is the
    # shapechange case and the "somebody added a slot" case; neither should
    # lose a row just because this file has not heard of it.
    lassign $out left right
    foreach row $pool {
        if {[llength $left] <= [llength $right]} {
            lappend left $row
        } else {
            lappend right $row
        }
    }

    return [list $left $right]
}

# A one-line summary of what is worn and carried, used only to tell whether a
# command did anything.
proc items_signature {} {
    set sig {}
    foreach where {equipment inventory} {
        foreach row [items_gear $where] {
            lappend sig [lindex $row 0] [lindex $row 2]
        }
    }
    return $sig
}

# --- the message line ----------------------------------------------------------
#
# The window says what happened, not what was asked for.  A command is queued
# and runs on the game's own schedule, and it can refuse -- a cursed ring stays
# on the finger -- so the message waits for a re-read to confirm the change and
# says so plainly when there is not one.
proc items_expect {verb name} {
    global items

    set items(expect) [list $verb $name [items_signature]]
    if {$items(expectafter) ne ""} { after cancel $items(expectafter) }
    set items(expectafter) [after 500 items_expect_timeout]
}

proc items_expect_settle {} {
    global items

    if {![llength $items(expect)]} return
    lassign $items(expect) verb name before
    if {[items_signature] eq $before} return

    switch -exact -- $verb {
        wield   { set items(message) "You are using $name." }
        takeoff { set items(message) "You were using $name." }
        drop    { set items(message) "You drop $name." }
        default { set items(message) "" }
    }
    items_expect_clear
}

proc items_expect_timeout {} {
    global items

    if {![llength $items(expect)]} return
    set items(message) "[lindex $items(expect) 1] stays where it is."
    items_expect_clear
}

proc items_expect_clear {} {
    global items

    set items(expect) {}
    if {$items(expectafter) ne ""} {
        after cancel $items(expectafter)
        set items(expectafter) ""
    }
}

# --- pieces --------------------------------------------------------------------

# A label, a hairline, a value: the worn-weight and carried-weight rules under
# each pane.  The same shape as the title strip, one size down, which is what
# makes the two read as the same document.
proc items_rule {parent name} {
    set f $parent.u$name
    frame $f -bg [classical::c bg]
    pack [classical::hairline $f.over] -fill x -side top

    set row $f.row
    frame $row -bg [classical::c bg]
    pack $row -fill x -pady [classical::pad 2 1]

    label $row.l -textvariable items(v,$name,label) \
        -font [classical::f footer] -bg [classical::c bg] \
        -fg [classical::c neutral-700]
    label $row.v -textvariable items(v,$name,value) \
        -font [classical::f footer] -bg [classical::c bg] \
        -fg [classical::c text]
    classical::hairline $row.rule
    pack $row.l -side left
    pack $row.v -side right
    pack $row.rule -side left -fill x -expand 1 -padx [classical::sp 3]

    return $f
}

# A scrolling area for the pack.
#
# The design gives the pane no scrollbar because a web page grows; a window
# cannot, and twenty-three items at forty pixels is taller than any sensible
# window.  So the list scrolls and nothing else does.
proc items_scroller {parent name} {
    set f $parent.s$name
    frame $f -bg [classical::c bg]

    canvas $f.c -bg [classical::c bg] -highlightthickness 0 -bd 0 \
        -yscrollcommand [list $f.sb set]
    ttk::scrollbar $f.sb -orient vertical -command [list $f.c yview]
    pack $f.sb -side right -fill y
    pack $f.c -side left -fill both -expand 1

    frame $f.c.inner -bg [classical::c bg]
    $f.c create window 0 0 -anchor nw -window $f.c.inner -tags inner

    bind $f.c.inner <Configure> [list items_scroller_fit $f]
    bind $f.c <Configure> [list items_scroller_fit $f]
    # Braced, not [list]: the %D has to survive until the event, and a list
    # built here would have expr fold it at bind time.
    bind $f.c <MouseWheel> {%W yview scroll [expr {-(%D)}] units}

    return $f
}

proc items_scroller_fit {f} {
    if {![winfo exists $f.c]} return
    $f.c itemconfigure inner -width [winfo width $f.c]
    $f.c configure -scrollregion [$f.c bbox all]
}

# One equipment slot: a leader line, a glyph box, a label and a name.
#
# `side` is which column it is in, and the row is built mirrored for the left
# one so that the leader lines all point inward at the figure.  The connector
# expands, which is what keeps it visible at every width -- it is the line that
# says this box belongs to that shoulder.
proc items_slotrow {parent name side} {
    set f $parent.r$name
    frame $f -bg [classical::c bg] -bd 0 -highlightthickness 0

    set conn $f.conn
    frame $conn -height 1 -bg [classical::c divider] -bd 0 -highlightthickness 0

    set box $f.box
    frame $box -width 46 -height 46 -bg [classical::c bg] \
        -bd 0 -highlightthickness 1 \
        -highlightbackground [classical::c neutral-400]
    pack propagate $box 0
    label $box.g -textvariable items(v,$name,glyph) \
        -font [classical::f glyph] -bg [classical::c bg] \
        -fg [classical::c accent-700]
    pack $box.g -expand 1

    set text $f.text
    frame $text -bg [classical::c bg]
    set anchor [expr {$side eq "left" ? "e" : "w"}]
    label $text.l -textvariable items(v,$name,label) -font [classical::f tiny] \
        -bg [classical::c bg] -fg [classical::c neutral-600] -anchor $anchor
    label $text.n -textvariable items(v,$name,name) -font [classical::f label] \
        -bg [classical::c bg] -fg [classical::c text] -anchor $anchor \
        -justify [expr {$side eq "left" ? "right" : "left"}] -wraplength 118
    pack $text.l $text.n -fill x

    # The 118 is the design's flex basis for this block, and it is fixed
    # rather than followed from the row's real width.  Both halves of that
    # matter.
    #
    # Fixed, because a Tk label with no wraplength asks for the width of its
    # text in one line, and twelve equipment names asking that is a pane that
    # wants two thousand pixels -- grid gives a column its requested width
    # before it shares out what is left, so the other pane goes off the side
    # of the window.
    #
    # Not followed, because a wraplength set from the widget's own width is a
    # ratchet: wider block, wider wraplength, wider request, wider block.  The
    # first version of this did exactly that and settled at nothing.

    if {$side eq "left"} {
        pack $conn -side right -fill x -expand 1 -padx [classical::pad 2 1]
        pack $box -side right
        pack $text -side right -fill x -expand 1 -padx [classical::pad 1 2]
    } else {
        pack $conn -side left -fill x -expand 1 -padx [classical::pad 1 2]
        pack $box -side left
        pack $text -side left -fill x -expand 1 -padx [classical::pad 2 1]
    }

    return $f
}

# --- painting ------------------------------------------------------------------

proc items_refresh {} {
    global items

    if {![winfo exists .items]} return

    items_refresh_slots
    items_refresh_pack
    items_refresh_burden
    items_expect_settle
}

# The slot rows are rebuilt only when the body changes, which is almost never
# -- but shapechange does change it, and a window that kept twelve rows for a
# body with four would be showing gear the character does not have.
proc items_refresh_slots {} {
    global items

    set rows [items_gear equipment]
    set shape {}
    foreach row $rows { lappend shape [lindex $row 7] }

    if {![info exists items(shape)] || $items(shape) ne $shape} {
        set items(shape) $shape
        items_build_slots $rows
    }

    foreach row $rows {
        lassign $row slot label name mention number weight glyph type part

        set key s$slot
        if {![winfo exists .items.card.body.eq.grid.r$key]} continue

        if {[info exists items(label,$type)]} {
            set items(v,$key,label) [classical::tracked \
                [string toupper $items(label,$type)]]
        } else {
            set items(v,$key,label) [classical::tracked [string toupper $part]]
        }

        if {$name eq ""} {
            set items(v,$key,name) $items(dash)
            set items(v,$key,glyph) [expr {[info exists items(slotglyph,$type)]
                ? $items(slotglyph,$type) : "."}]
            .items.card.body.eq.grid.r$key.box.g configure \
                -fg [classical::c neutral-400]
        } else {
            set items(v,$key,name) $name
            set items(v,$key,glyph) $glyph
            .items.card.body.eq.grid.r$key.box.g configure \
                -fg [classical::c accent-700]
        }
    }

    # The worn weight is the sum of the slots, which is not a number the game
    # keeps: total_weight counts the pack too.
    set worn 0
    foreach row $rows { incr worn [lindex $row 5] }
    set items(v,worn,label) "Worn weight"
    set items(v,worn,value) "[items_weight $worn] lb"
}

proc items_build_slots {rows} {
    global items

    set grid .items.card.body.eq.grid
    foreach child [winfo children $grid] {
        if {$child ne "$grid.figure"} { destroy $child }
    }
    # Only this grid's targets.  A blanket unset takes the pack pane's
    # registration with it, and the take-off half of the interaction then
    # silently does nothing.
    array unset items target,$grid.*
    set items(lit) {}

    lassign [items_arrange $rows] left right

    foreach {side column} [list left $left right $right] {
        set col [expr {$side eq "left" ? 0 : 2}]
        set r 0
        foreach row $column {
            set slot [lindex $row 0]
            set f [items_slotrow $grid s$slot $side]
            grid $f -row $r -column $col -sticky ew \
                -pady [classical::pad 1 1]
            set items(target,$f) [list slot $slot]
            items_draggable $f [list equipment $slot]
            incr r
        }
    }

    grid $grid.figure -row 0 -column 1 -rowspan 6 -padx [classical::sp 3]
    grid columnconfigure $grid 0 -weight 1 -minsize 148
    grid columnconfigure $grid 2 -weight 1 -minsize 148
    grid columnconfigure $grid 1 -weight 0
}

proc items_refresh_pack {} {
    global items

    set inner .items.card.body.pack.spack.c.inner
    foreach child [winfo children $inner] { destroy $child }
    array unset items cells,*

    set rows [items_gear inventory]

    if {![llength $rows]} {
        label $inner.empty -text "Your pack is empty." \
            -font [classical::f note] -bg [classical::c bg] \
            -fg [classical::c neutral-600] -anchor w
        grid $inner.empty -row 0 -column 0 -columnspan 4 -sticky w \
            -pady [classical::sp 2]
        set items(v,carried,label) "Nothing carried"
        set items(v,carried,value) "0.0 lb"
        return
    }

    set carried 0
    set r 0
    foreach row $rows {
        lassign $row index label name mention number weight glyph type part
        incr carried $weight

        set cells {}
        lappend cells [label $inner.k$index -text "$label)" \
            -font [classical::f footer] -bg [classical::c bg] \
            -fg [classical::c neutral-600] -anchor w]
        lappend cells [label $inner.g$index -text $glyph \
            -font [classical::f glyphsm] -bg [classical::c bg] \
            -fg [classical::c accent-700] -anchor w]
        lappend cells [label $inner.n$index -text $name \
            -font [classical::f item] -bg [classical::c bg] \
            -fg [classical::c text] -anchor w -justify left -wraplength 320]
        lappend cells [label $inner.w$index -text [items_weight $weight] \
            -font [classical::f amount] -bg [classical::c bg] \
            -fg [classical::c neutral-600] -anchor e]

        set c 0
        foreach cell $cells {
            grid $cell -row $r -column $c -sticky ew \
                -padx [expr {$c == 0 ? 0 : [classical::sp 3]}] -pady 5
            incr c
        }
        grid [classical::hairline $inner.d$index] -row [expr {$r + 1}] \
            -column 0 -columnspan 4 -sticky ew

        set items(cells,$index) $cells
        items_draggable_cells $cells [list inventory $index]
        incr r 2
    }

    grid columnconfigure $inner 2 -weight 1

    items_paint_selection

    set items(v,carried,label) [expr {[llength $rows] == 1
        ? "1 item" : "[llength $rows] items"}]
    set items(v,carried,value) "[items_weight $carried] lb"
}

proc items_refresh_burden {} {
    global items

    set b [items_gear burden]
    if {[llength $b] != 3} {
        set items(burden) ""
        return
    }
    lassign $b carried slow capacity
    set items(burden) [format "burden %s / %s / %s lb" \
        [items_weight $carried] [items_weight $slow] [items_weight $capacity]]
}

# --- dragging ------------------------------------------------------------------
#
# Tk has no drag and drop, so this is the whole of it: press, motion, release,
# and a borrowed toplevel for the thing being carried.  The rules are the
# design's -- both directions, plus the ring-to-ring case -- and the answer to
# "does this go here" is the game's, asked through angband_gear fits, so a slot
# the window thinks is compatible is one the wear command will accept.
#
# Two compromises against the spec, both Tk's fault and neither hidden:
# a refused target is drawn with a tinted background rather than a dashed
# outline, because a Tk frame border cannot be dashed; and the ghost follows
# the pointer at an offset, because winfo containing returns the topmost window
# and a ghost under the pointer would be the only thing it ever found.

proc items_draggable {f source} {
    items_draggable_cells [list $f {*}[items_descendants $f]] $source
}

proc items_descendants {w} {
    set out {}
    foreach child [winfo children $w] {
        lappend out $child {*}[items_descendants $child]
    }
    return $out
}

proc items_draggable_cells {cells source} {
    foreach w $cells {
        $w configure -cursor hand2
        bind $w <ButtonPress-1>   [list items_press $source %X %Y]
        bind $w <B1-Motion>       [list items_motion %X %Y]
        bind $w <ButtonRelease-1> [list items_release %X %Y]
    }
}

proc items_press {source X Y} {
    global items

    lassign $source view index

    # An empty slot has nothing to pick up, and the design says so: no drag
    # starts rather than a drag that can only fail.
    if {$view eq "equipment"} {
        set row [items_row equipment $index]
        if {[lindex $row 2] eq ""} { set items(drag) {} ; return }
    }

    set items(drag) [list $view $index $X $Y 0]
    items_select $view $index
}

proc items_motion {X Y} {
    global items

    if {![llength $items(drag)]} return
    lassign $items(drag) view index x0 y0 started

    if {!$started} {
        if {abs($X - $x0) < 4 && abs($Y - $y0) < 4} return
        set row [items_row $view $index]
        items_ghost_show [lindex $row 6] [lindex $row 2]
        set items(drag) [list $view $index $x0 $y0 1]
    }

    items_ghost_move $X $Y
    items_light [items_target $X $Y] $view $index
}

proc items_release {X Y} {
    global items

    if {![llength $items(drag)]} return
    lassign $items(drag) view index x0 y0 started

    items_ghost_hide
    items_light {} $view $index
    set items(drag) {}
    if {!$started} return

    set target [items_target $X $Y]
    if {![llength $target]} return

    set row [items_row $view $index]
    set name [lindex $row 2]
    if {$name eq ""} return

    switch -exact -- [lindex $target 0] {
        pack {
            if {$view ne "equipment"} return
            items_expect takeoff $name
            items_do [list takeoff equipment $index]
        }
        slot {
            set slot [lindex $target 1]
            if {$view eq "equipment"} {
                if {$slot == $index} return
                # There is no command for moving worn gear from one slot to
                # another: the game's own answer is to take it off and put it
                # on again, and inventing a two-turn action behind a single
                # drag would be the window deciding something the player did
                # not ask for.
                set items(message) \
                    "Take $name off first; it cannot move between slots."
                return
            }
            if {$slot ni [items_gear fits $view $index]} {
                set items(message) "You cannot wear $name there."
                return
            }
            items_expect wield $name
            items_do [list wield $view $index $slot]
        }
    }
}

# Every command goes through here, so a refusal from the bridge -- the game is
# mid-prompt, most often -- says so in the message line instead of raising a
# background error nobody sees.
proc items_do {argv} {
    global items

    if {[catch {angband_gear {*}$argv} err]} {
        items_expect_clear
        set items(message) $err
    }
}

proc items_row {view index} {
    foreach row [items_gear $view] {
        if {[lindex $row 0] == $index} { return $row }
    }
    return {}
}

# The drop target under the pointer.
#
# Geometry, not winfo containing.  The obvious implementation asks Tk what
# window is at the pointer and walks up to a registered ancestor, and it is
# wrong twice over: winfo containing answers for the whole application, so with
# the main window stacked above this one every drop lands on the map canvas;
# and the thing nearest the pointer during a drag is the ghost.  There are a
# dozen targets and they are all rectangles, so this asks them directly.
#
# The smallest match wins, so a target inside another target still gets the
# drop.
proc items_target {X Y} {
    global items

    set best {}
    set area 0

    foreach key [array names items target,*] {
        set w [string range $key 7 end]
        if {![winfo exists $w] || ![winfo ismapped $w]} continue

        set x [winfo rootx $w]
        set y [winfo rooty $w]
        set width [winfo width $w]
        set height [winfo height $w]
        if {$X < $x || $X >= $x + $width} continue
        if {$Y < $y || $Y >= $y + $height} continue

        set size [expr {$width * $height}]
        if {$best eq "" || $size < $area} {
            set best $items($key)
            set area $size
        }
    }

    return $best
}

# Light the hovered target, and only it.  A refused target still lights, which
# is the design's point: the player should see that the slot was understood.
proc items_light {target view index} {
    global items

    if {$items(lit) eq $target} return
    items_light_one $items(lit) none
    set items(lit) $target
    if {![llength $target]} return

    set ok 0
    switch -exact -- [lindex $target 0] {
        pack { set ok [expr {$view eq "equipment"}] }
        slot {
            set ok [expr {$view ne "equipment"
                && [lindex $target 1] in [items_gear fits $view $index]}]
        }
    }
    items_light_one $target [expr {$ok ? "yes" : "no"}]
}

proc items_light_one {target state} {
    if {![llength $target]} return

    switch -exact -- $state {
        yes  { set bg [classical::c accent-100] ; set edge [classical::c accent] }
        no   { set bg [classical::c neutral-200] ; set edge [classical::c neutral-400] }
        none {
            # Back to whatever it was before the pointer arrived, which is not
            # always the plain ground: a selected slot keeps its tint.
            set bg [items_base_bg $target]
            set edge [classical::c neutral-400]
        }
    }

    if {[lindex $target 0] eq "pack"} {
        set w .items.card.body.pack
        if {[winfo exists $w]} {
            $w configure -highlightbackground \
                [expr {$state eq "none" ? [classical::c bg] : $edge}]
        }
        return
    }

    set box .items.card.body.eq.grid.rs[lindex $target 1].box
    if {![winfo exists $box]} return
    $box configure -bg $bg -highlightbackground $edge
    $box.g configure -bg $bg
}

# The colour a target sits at when nothing is hovering over it.
proc items_base_bg {target} {
    global items

    if {[lindex $target 0] eq "slot" && [llength $items(sel)]
            && $items(sel) eq [list equipment [lindex $target 1]]} {
        return [classical::c accent-100]
    }
    return [classical::c bg]
}

proc items_ghost_show {glyph name} {
    set g .itemghost
    if {![winfo exists $g]} {
        toplevel $g -bg [classical::c accent-100] -bd 0 \
            -highlightthickness 1 -highlightbackground [classical::c accent]
        wm overrideredirect $g 1
        catch {wm attributes $g -topmost 1}
        label $g.g -font [classical::f glyphsm] \
            -bg [classical::c accent-100] -fg [classical::c accent-700]
        label $g.n -font [classical::f label] \
            -bg [classical::c accent-100] -fg [classical::c accent-900]
        pack $g.g -side left -padx [classical::pad 2 1] -pady 3
        pack $g.n -side left -padx [classical::pad 1 2] -pady 3
    }
    $g.g configure -text $glyph
    $g.n configure -text $name
    wm deiconify $g
    raise $g
}

proc items_ghost_move {X Y} {
    if {![winfo exists .itemghost]} return
    # Offset, so the pointer is never over the ghost: winfo containing returns
    # the topmost window, and the ghost would be it.
    wm geometry .itemghost "+[expr {$X + 14}]+[expr {$Y + 14}]"
}

proc items_ghost_hide {} {
    if {[winfo exists .itemghost]} { wm withdraw .itemghost }
}

# --- selection and inscription -------------------------------------------------
#
# The inscription bar acts on one item, so there has to be one: clicking a row
# picks it, and the bar then shows and sets that item's note.
proc items_select {view index} {
    global items

    set items(sel) [list $view $index]
    items_paint_selection
    set info [items_gear info $view $index]
    if {[dict exists $info inscription]} {
        set items(inscription) [dict get $info inscription]
    } else {
        set items(inscription) ""
    }
    if {[dict exists $info name]} {
        set items(v,sel) [dict get $info name]
    } else {
        set items(v,sel) ""
    }
}

# Which row the inscription bar is pointed at.  The design does not draw a
# selection, because a web page can put the inscription beside the item; a
# window has one bar for twelve slots and twenty-three rows, and a bar that
# acts on an item the player cannot see is a bar that will inscribe the wrong
# thing.
proc items_paint_selection {} {
    global items

    if {![winfo exists .items]} return

    foreach key [array names items cells,*] {
        set index [string range $key 6 end]
        set lit [expr {$items(sel) eq [list inventory $index]}]
        set bg [expr {$lit ? [classical::c accent-100] : [classical::c bg]}]
        foreach w $items($key) {
            if {[winfo exists $w]} { $w configure -bg $bg }
        }
    }

    foreach key [array names items target,*] {
        set target $items($key)
        if {[lindex $target 0] ne "slot"} continue
        set box .items.card.body.eq.grid.rs[lindex $target 1].box
        if {![winfo exists $box]} continue
        if {$items(lit) eq $target} continue
        set bg [items_base_bg $target]
        $box configure -bg $bg
        $box.g configure -bg $bg
    }
}

proc items_inscribe {} {
    global items

    if {![llength $items(sel)]} {
        set items(message) "Choose an item first."
        return
    }
    lassign $items(sel) view index
    items_do [list inscribe $view $index $items(inscription)]
    set items(message) "Inscribed $items(v,sel)."
}

# --- the window ----------------------------------------------------------------

proc items_window {} {
    global items

    if {[winfo exists .items]} {
        wm deiconify .items
        raise .items
        return
    }

    set card [classical::window .items "Items"]
    wm protocol .items WM_DELETE_WINDOW { destroy .items }
    wm geometry .items 1180x740
    wm minsize .items 560 520

    # Keys typed here still play the game, so the window can be left open and
    # the letter commands keep working -- which is the design's requirement
    # that dragging be an addition to the keyboard and never a replacement.
    bind .items <Key> { angband_key %N %s %A }
    bind .items <Escape> { destroy .items ; break }
    bind .items <Destroy> { if {"%W" eq ".items"} { items_forget } }

    classical::titlestrip $card ZANGBAND ITEMS
    classical::footer $card {
        esc "close"
        w   "wear or wield"
        t   "take off"
        d   "drop"
    } items(burden)

    # --- the inscription bar --------------------------------------------
    set insc $card.insc
    frame $insc -bg [classical::c bg]
    pack $insc -fill x -padx [classical::sp 6] -pady [classical::pad 3 3]

    label $insc.k -text [classical::tracked "INSCRIPTION"] \
        -font [classical::f kicker] -bg [classical::c bg] \
        -fg [classical::c neutral-600]
    pack $insc.k -side left -padx [classical::pad 1 3]

    set box [classical::searchbox $insc note items(inscription) "none"]
    pack $box -side left -fill x -expand 1
    $box.e configure -width 24
    bind $box.e <Return> { items_inscribe ; break }

    # The entry keeps the game's keys out of the field.  Tk runs the toplevel's
    # bindings after the widget's own, so without this every letter typed here
    # would be inserted *and* played.
    bindtags $box.e [list $box.e Entry all]

    label $insc.help \
        -text "Drag an item onto a slot to wield or wear it; drag it off to stow it." \
        -font [classical::f note] -bg [classical::c bg] \
        -fg [classical::c neutral-600]
    pack $insc.help -side left -padx [classical::pad 3 1]

    pack [classical::hairline $card.iunder] -fill x

    # --- the two panes ---------------------------------------------------
    set body $card.body
    frame $body -bg [classical::c bg]
    pack $body -fill both -expand 1

    set eq $body.eq
    frame $eq -bg [classical::c bg]
    frame $body.div -bg [classical::c divider] -width 1 -height 1 -bd 0 \
        -highlightthickness 0
    set pk $body.pack
    frame $pk -bg [classical::c bg] -bd 0 -highlightthickness 1 \
        -highlightbackground [classical::c bg]

    # The whole pane is the take-off target, as the design says: dropping a
    # worn item anywhere in the pack's half of the window stows it.
    set items(target,$pk) [list pack]

    # --- equipment -------------------------------------------------------
    classical::sectionhead $eq eq "Equipment" i
    pack $eq.heq -fill x -padx [classical::sp 6] -pady [classical::pad 6 1]

    set grid $eq.grid
    frame $grid -bg [classical::c bg]
    pack $grid -fill x -padx [classical::sp 4] -pady [classical::pad 4 1]

    canvas $grid.figure -bg [classical::c bg] -highlightthickness 0 -bd 0 \
        -width [expr {round($items(figure,w) * $items(figure,scale))}] \
        -height [expr {round($items(figure,h) * $items(figure,scale))}]
    items_figure_draw $grid.figure

    pack [items_rule $eq worn] -fill x -padx [classical::sp 6] \
        -pady [classical::pad 4 6]

    # --- the pack --------------------------------------------------------
    classical::sectionhead $pk pk "Inventory" ii
    pack $pk.hpk -fill x -padx [classical::sp 6] -pady [classical::pad 6 1]

    pack [items_scroller $pk pack] -fill both -expand 1 \
        -padx [classical::sp 6] -pady [classical::pad 3 1]

    label $pk.msg -textvariable items(message) -font [classical::f note] \
        -bg [classical::c bg] -fg [classical::c neutral-700] -anchor w \
        -justify left
    pack $pk.msg -fill x -padx [classical::sp 6] -pady [classical::pad 3 1]

    pack [items_rule $pk carried] -fill x -padx [classical::sp 6] \
        -pady [classical::pad 3 6]

    # Two columns when there is room for both, stacked when there is not --
    # the design's auto-fit at a 520px track, said in Tk.
    grid rowconfigure $body 0 -weight 1
    bind .items <Configure> { items_reflow_soon }
    items_reflow

    items_refresh

    # The geometry above is a request until the window is mapped, and the
    # column count is read from the real width.
    update idletasks
    items_reflow
}

# Dropping the window drops everything that pointed into it, so a later refresh
# cannot try to paint a widget that is gone.
proc items_forget {} {
    global items

    array unset items target,*
    array unset items cells,*
    unset -nocomplain items(shape)
    set items(lit) {}
    set items(drag) {}
    items_expect_clear
    items_ghost_hide
}

proc items_reflow_soon {} {
    global items

    if {[info exists items(reflowpending)]} return
    set items(reflowpending) 1
    after idle { unset -nocomplain items(reflowpending) ; items_reflow }
}

proc items_reflow {} {
    global items

    if {![winfo exists .items]} return
    set body .items.card.body

    set w [winfo width .items]
    if {$w <= 1} { set w [winfo reqwidth .items] }
    set wide [expr {$w >= 1080}]

    # The two pieces of running prose follow the window rather than their own
    # frames, which is what stops them ratcheting: the window's width is the
    # one number in the layout that the layout does not decide.
    if {[winfo exists .items.card.insc.help]} {
        .items.card.insc.help configure -wraplength \
            [expr {max(240, $w - 420)}]
    }
    if {[winfo exists .items.card.body.pack.msg]} {
        .items.card.body.pack.msg configure -wraplength \
            [expr {max(240, ($wide ? $w / 2 : $w) - 80)}]
    }

    if {[info exists items(wide)] && $items(wide) eq $wide} return
    set items(wide) $wide


    if {$wide} {
        # -columnspan 1 is not noise.  grid remembers every option it has ever
        # been given for a slave, so a pane that once spanned three columns
        # goes on spanning them however carefully the row and column are set
        # -- and the second pane is then laid out off the side of the window.
        grid $body.eq   -row 0 -column 0 -columnspan 1 -sticky nsew
        grid $body.div  -row 0 -column 1 -columnspan 1 -sticky ns
        grid $body.pack -row 0 -column 2 -columnspan 1 -sticky nsew
        grid rowconfigure $body 1 -weight 0
        grid rowconfigure $body 2 -weight 0
        grid columnconfigure $body 0 -weight 1 -uniform pane
        grid columnconfigure $body 1 -weight 0 -uniform ""
        grid columnconfigure $body 2 -weight 1 -uniform pane
    } else {
        grid $body.eq   -row 0 -column 0 -columnspan 3 -sticky nsew
        grid $body.div  -row 1 -column 0 -columnspan 3 -sticky ew
        grid $body.pack -row 2 -column 0 -columnspan 3 -sticky nsew
        grid rowconfigure $body 0 -weight 0
        grid rowconfigure $body 2 -weight 1
        grid columnconfigure $body 0 -weight 1 -uniform ""
        grid columnconfigure $body 1 -weight 0 -uniform ""
        grid columnconfigure $body 2 -weight 0 -uniform ""
    }
}

# Up to date whether or not anyone is looking: the bindings live on "." and
# cost nothing while the window is closed, because items_refresh returns at
# once when there is none.
foreach event $items(events) {
    bind . <<Angband_$event>> {+ items_refresh }
}
