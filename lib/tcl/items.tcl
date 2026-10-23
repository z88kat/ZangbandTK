# The Items window: the paper doll and the pack.
#
# Two views of one thing.  On the left, the equipment slots ranged around a
# figure, each tied to it by a dotted leader; on the right, the pack, in the
# game's own order with the game's own letters.  Items move between them by
# dragging, by the buttons and by the letter keys, and the keyboard commands
# keep working throughout.
#
# Decision 18 builds it from Tk's own widgets in Tk's classic theme, to the
# handoff in .claude/plans/design_handoff_items_tk.  The doll is one canvas,
# because a canvas is Tk's widget for drawing; the pack is a text widget used
# as a list, because each row needs a coloured symbol on a black cell and a
# listbox has one colour per row.
#
# The game stays authoritative.  Nothing here moves an item: every action
# pushes the command the keyboard would have pushed and then waits to be told
# what happened, so the turn cost, the curse checks and the messages are the
# game's.  That is why the message line reports only what a re-read confirms.

set items(dir) [file dirname [info script]]
set items(message) ""
set items(inscription) ""
set items(sel) {}
set items(selname) ""
set items(expect) {}
set items(expectafter) ""
set items(drag) {}
set items(lit) {}
set items(packrows) {}
set items(recalled) {}
foreach v {sel worn packcount packweight burden capacity} { set items(v,$v) "" }

# When to read again.  Equipment and inventory for the obvious reason, stats
# because carrying capacity is strength, and the two birth events because the
# window can be open before there is anything in it.
set items(events) {
    INVENTORY EQUIPMENT STATS ENTER_GAME LEAVE_BIRTH ENTER_WORLD
}

# --- the slots ----------------------------------------------------------------
#
# The design's order, by slot type: head down the left, light down the right,
# so the leaders meet the figure where the gear would be.  This is a preference
# and not a requirement -- a body with different slots, which shapechange can
# produce, keeps the game's own order for whatever is left over rather than
# losing a row.
set items(order,left)  {hat amulet weapon bow ring boots}
set items(order,right) {light body_armor cloak shield ring gloves}

# What to call a slot, in the design's words.  Anything unlisted falls back to
# the game's body part, which is always right if not always graceful.
foreach {type text} {
    hat Helm  amulet Amulet  weapon Weapon  bow Bow  ring Ring  boots Boots
    light Light  body_armor Body  cloak Cloak  shield Shield  gloves Gloves
} { set items(label,$type) $text }

# What an *empty* slot shows.  An empty slot has no object and so no symbol,
# but a doll of blank wells reads as broken rather than as unequipped, so it
# shows the symbol the game uses for the kind of thing that goes there.
foreach {type text} {
    hat "\]"  amulet "\""  weapon "|"  bow "\}"  ring "="  boots "\]"
    light "~"  body_armor "("  cloak "("  shield ")"  gloves "\]"
} { set items(slotglyph,$type) $text }

# The doll panel's measurements, in pixels: the design's.  The labels are a
# fixed width so that the wells line up in two straight columns.
array set items {
    doll,bd 2  doll,padx 14  doll,pady 10
    doll,label 52  doll,gap 6  doll,well 40  doll,row 44  doll,leader 24
    doll,spritew 120  doll,spriteh 240
}

# --- reading the game ----------------------------------------------------------

# Tenths of a pound, which is how the game stores every weight, to one decimal.
proc items_weight {tenths} {
    if {![string is integer -strict $tenths]} { return "0.0" }
    return [format "%d.%d" [expr {$tenths / 10}] [expr {$tenths % 10}]]
}

proc items_gear {what args} {
    if {[catch {angband_gear $what {*}$args} result]} { return {} }
    return $result
}

proc items_row {view index} {
    foreach row [items_gear $view] {
        if {[lindex $row 0] == $index} { return $row }
    }
    return {}
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

# Every command goes through here, so a refusal from the bridge -- the game is
# mid-prompt, most often -- says so in the message line instead of raising a
# background error nobody sees.
proc items_do {argv} {
    global items

    if {[catch {angband_gear {*}$argv} err]} {
        items_expect_clear
        set items(message) $err
        return 0
    }
    return 1
}

# --- the doll ------------------------------------------------------------------

proc items_doll_canvas {} { return .items.f.body.eq.doll }

# The slots are rebuilt only when the body changes, which is almost never --
# but shapechange does change it, and a doll that kept twelve slots for a body
# with four would be showing gear the character does not have.
proc items_build_slots {rows} {
    global items

    set c [items_doll_canvas]
    $c delete slot
    array unset items well,*

    lassign [items_arrange $rows] left right
    set n [expr {max(6, [llength $left], [llength $right])}]

    # The design's measurements are for text of 13.  The wells and rows grow
    # with the window text, and the label column is as wide as the widest
    # label in the font it is drawn in -- a fixed one cut "Weapon" off at the
    # canvas edge once the text was larger.
    set o $items(doll,bd)
    set label $items(doll,label)
    foreach row $rows {
        lassign $row - - - - - - - type part
        set text [expr {[info exists items(label,$type)] ? $items(label,$type)
            : [string totitle $part]}]
        set label [expr {max($label, [font measure [uifont 12] $text] + 2)}]
    }
    set gap $items(doll,gap)
    set well [ui_scaled $items(doll,well)]
    set rowh [ui_scaled $items(doll,row)]
    set leader $items(doll,leader)
    # Never shorter than the figure, which is pixel art and does not scale.
    set height [expr {max($n * $rowh - ($rowh - $well), $items(doll,spriteh))}]

    # The sprite, centred between the two columns of wells.
    set sx1 [expr {$o + $items(doll,padx) + $label + $gap + $well + $leader}]
    set sx2 [expr {$sx1 + $items(doll,spritew)}]
    set width [expr {2 * $items(doll,padx) + 2 * ($label + $gap + $well + $leader)
        + $items(doll,spritew)}]
    $c configure -width $width -height [expr {$height + 2 * $items(doll,pady)}]
    $c coords sprite [expr {($sx1 + $sx2) / 2}] \
        [expr {$o + $items(doll,pady) + $height / 2}]

    foreach {side column} [list left $left right $right] {
        set r 0
        foreach row $column {
            lassign $row slot
            set y1 [expr {$o + $items(doll,pady) + $r * $rowh}]
            set y2 [expr {$y1 + $well}]
            set cy [expr {$y1 + $well / 2}]

            if {$side eq "left"} {
                set x1 [expr {$o + $items(doll,padx) + $label + $gap}]
                set x2 [expr {$x1 + $well}]
                set lx [expr {$x1 - $gap}]
                set anchor e
                set lead [list $x2 $cy $sx1 $cy]
            } else {
                set x1 [expr {$sx2 + $leader}]
                set x2 [expr {$x1 + $well}]
                set lx [expr {$x2 + $gap}]
                set anchor w
                set lead [list $sx2 $cy $x1 $cy]
            }

            $c create line {*}$lead -fill #606060 -dash . -tags slot
            $c create rectangle $x1 $y1 $x2 $y2 -fill black -outline #606060 \
                -tags [list slot s$slot]
            $c create text [expr {($x1 + $x2) / 2}] $cy -anchor center \
                -font [uifont Courier 18 bold] -tags [list slot s$slot g$slot]
            $c create text $lx $cy -anchor $anchor -font [uifont 12] \
                -fill #c0c0c0 -tags [list slot s$slot l$slot]
            set items(well,$slot) [list $x1 $y1 $x2 $y2]

            $c bind s$slot <ButtonPress-1> \
                [list items_press $c [list equipment $slot] %X %Y]
            $c bind s$slot <Enter> [list items_recall equipment $slot]
            $c bind s$slot <Double-1> [list items_takeoff_slot $slot]
            incr r
        }
    }
    $c raise selring
    $c raise hover
}

# The doll is laid out for the window text's size, so laid out again for a new
# one.
bind . <<WindowTextChanged>> {+
    if {[winfo exists .items]} {
        unset -nocomplain items(shape)
        items_refresh_slots
    }
}

proc items_refresh_slots {} {
    global items

    set rows [items_gear equipment]
    set shape {}
    foreach row $rows { lappend shape [lindex $row 7] }
    if {![info exists items(shape)] || $items(shape) ne $shape} {
        set items(shape) $shape
        items_build_slots $rows
    }

    set c [items_doll_canvas]
    set worn 0
    foreach row $rows {
        lassign $row slot label name mention number weight glyph type part colour
        incr worn $weight

        $c itemconfigure l$slot -text [expr {[info exists items(label,$type)]
            ? $items(label,$type) : [string totitle $part]}]
        if {$name eq ""} {
            $c itemconfigure g$slot -fill #3a3a3a -text [expr {
                [info exists items(slotglyph,$type)] ? $items(slotglyph,$type) : "."}]
        } else {
            $c itemconfigure g$slot -text $glyph \
                -fill [expr {$colour ne "" ? $colour : "white"}]
        }
    }

    # The worn weight is the sum of the slots, which is not a number the game
    # keeps: total_weight counts the pack too.
    set items(v,worn) [expr {[llength $rows] ? "[items_weight $worn] lb" : ""}]
}

# --- the pack ------------------------------------------------------------------

proc items_pack_text {} { return .items.f.body.inv.list.t }

proc items_refresh_pack {} {
    global items

    set t [items_pack_text]
    $t configure -state normal
    $t delete 1.0 end

    # No character is not the same as an empty pack, and says nothing.
    set have [expr {![catch {angband_gear inventory} rows]}]
    if {!$have} { set rows {} }
    set items(packrows) $rows
    set carried 0

    if {$have && ![llength $rows]} {
        $t insert end "Your pack is empty." empty
    }
    foreach row $rows {
        lassign $row index label name mention number weight glyph type part colour
        incr carried $weight
        if {$colour eq ""} { set colour white }
        $t tag configure fg$colour -foreground $colour
        $t tag raise fg$colour
        $t insert end "$label)\t" {} " $glyph " [list glyph fg$colour] \
            "\t$name\t[items_weight $weight] lb\n" {}
    }
    $t tag raise glyph
    $t configure -state disabled

    set n [llength $rows]
    lassign [items_gear slots] used size
    set items(v,packcount) [expr {$used eq "" ? ""
        : "$n [expr {$n == 1 ? {item} : {items}}], $used of $size slots"}]
    set items(v,packweight) [expr {$have ? "[items_weight $carried] lb" : ""}]
}

# The weight column is a right-aligned tab at the far edge, which moves when
# the list is resized.
# Measured in the window text's own font, so measured again when it changes.
bind . <<WindowTextChanged>> {+ items_pack_tabs }

proc items_pack_tabs {} {
    set t [items_pack_text]
    if {![winfo exists $t]} return
    set cw [font measure [uifont Courier 12] 0]
    set inset [expr {2 * ([$t cget -padx] + [$t cget -borderwidth]
        + [$t cget -highlightthickness]) + 4}]
    set right [expr {max(12 * $cw, [winfo width $t] - $inset)}]
    $t configure -tabs [list [expr {3 * $cw}] [expr {7 * $cw}] $right right]
}

proc items_refresh_burden {} {
    global items

    set b [items_gear burden]
    set bar .items.f.tools.burden
    if {[llength $b] != 3} {
        set items(v,burden) ""
        set items(v,capacity) ""
        $bar configure -value 0
        return
    }

    # Green until speed starts to suffer, gold past it -- which is the line the
    # game draws, at half of what can be carried at all.
    lassign $b carried slow capacity
    set items(v,burden) "[items_weight $carried] / [items_weight $slow] lb"
    set items(v,capacity) "Capacity [items_weight $capacity] lb"
    $bar configure -maximum [expr {max(1, $capacity)}] \
        -value [expr {min($carried, $capacity)}]
    set colour [expr {$carried > $slow ? "#c09020" : "#38a038"}]
    if {![info exists items(burdencolour)] || $items(burdencolour) ne $colour} {
        set items(burdencolour) $colour
        ttk::style configure Burden.Horizontal.TProgressbar -background $colour
    }
}

# --- painting ------------------------------------------------------------------

# The figure: a woman (doll.png) or a man (doll-male.png), the two sprites the
# handoff drew, as the character's portrait is one or the other -- portrait_sex,
# from what birth rolled, so the doll and the Character window always agree.
# Checked on every refresh, which is cheap, so a character loaded or made after
# the window opened gets theirs.  The woman, the handoff's default, until there
# is a character.
proc items_doll_sprite {} {
    global items

    set file doll.png
    if {![catch {angband_player age}] && [portrait_sex] eq "M"} {
        set file doll-male.png
    }
    if {[info exists items(dollfile)] && $items(dollfile) eq $file} return
    if {"items_doll" ni [image names]} { image create photo items_doll }
    if {[catch {
        items_doll blank
        items_doll read [file join $items(dir) $file]
    }]} {
        return
    }
    set items(dollfile) $file
}

proc items_refresh {} {
    global items

    if {![winfo exists .items]} return

    items_doll_sprite
    items_refresh_slots
    items_refresh_pack
    items_refresh_burden
    # Positions move when the gear does, so the next one pointed at is asked
    # about afresh.
    set items(recalled) {}

    # A selection is a position, and positions move when the gear does: if the
    # thing at that position is not the thing that was chosen, nothing is.
    if {[llength $items(sel)]} {
        if {[lindex [items_row {*}$items(sel)] 2] ne $items(selname)} {
            items_select_none
        }
    }
    items_paint_selection
    items_actions
    items_expect_settle
}

# Which item is chosen: the yellow ring on a slot, or the raised band on a row.
proc items_paint_selection {} {
    global items

    if {![winfo exists .items]} return
    set c [items_doll_canvas]
    set t [items_pack_text]

    lassign $items(sel) view index
    if {$view eq "equipment" && [info exists items(well,$index)]} {
        lassign $items(well,$index) x1 y1 x2 y2
        $c coords selring [expr {$x1 - 2}] [expr {$y1 - 2}] \
            [expr {$x2 + 2}] [expr {$y2 + 2}]
        $c itemconfigure selring -state normal
    } else {
        $c itemconfigure selring -state hidden
    }

    $t tag remove chosen 1.0 end
    if {$view eq "inventory"} {
        set i 0
        foreach row $items(packrows) {
            if {[lindex $row 0] == $index} {
                set line [expr {$i + 1}]
                $t tag add chosen $line.0 "$line.0 lineend + 1 char"
                $t see $line.0
                break
            }
            incr i
        }
    }
}

proc items_select {view index} {
    global items

    set row [items_row $view $index]
    set items(sel) [list $view $index]
    set items(selname) [lindex $row 2]
    set items(v,sel) [lindex $row 2]

    set info [items_gear info $view $index]
    set items(inscription) [expr {[dict exists $info inscription]
        ? [dict get $info inscription] : ""}]
    items_paint_selection
    items_actions
    items_recall $view $index
}

# What is pointed at or chosen is described in the Recall pane, as the
# original's Inventory window did -- the game's recall of it, by tracking it the
# way the game's own item menu does.  Asked once per item rather than on every
# movement of the pointer across it.
proc items_recall {view index} {
    global items

    if {[list $view $index] eq $items(recalled)} return
    set items(recalled) [list $view $index]
    catch {angband_gear recall $view $index}
}

proc items_select_none {} {
    global items
    set items(sel) {}
    set items(selname) ""
    set items(v,sel) ""
    set items(inscription) ""
}

# Up and down the pack with the keyboard.
proc items_step {how} {
    global items

    set n [llength $items(packrows)]
    if {!$n} return
    set at -1
    lassign $items(sel) view index
    if {$view eq "inventory"} {
        set i 0
        foreach row $items(packrows) {
            if {[lindex $row 0] == $index} { set at $i ; break }
            incr i
        }
    }
    switch -- $how {
        up    { set at [expr {$at <= 0 ? 0 : $at - 1}] }
        down  { set at [expr {min($n - 1, $at + 1)}] }
        first { set at 0 }
        last  { set at [expr {$n - 1}] }
    }
    items_select inventory [lindex $items(packrows) $at 0]
}

# --- the commands --------------------------------------------------------------

# What the selection allows: {wear takeoff drop inspect inscribe}, each 0 or 1.
proc items_can {} {
    global items

    lassign $items(sel) view index
    if {$view eq "" || $items(selname) eq ""} { return {0 0 0 0 0} }
    if {$view eq "equipment"} { return {0 1 1 1 1} }
    set fits [expr {[llength [items_gear fits inventory $index]] > 0}]
    return [list $fits 0 1 1 1]
}

# Buttons and menu entries that do not apply to the selection are disabled.
proc items_actions {} {
    if {![winfo exists .items]} return
    set w .items.f
    set m .items.mb.items
    foreach action {wear takeoff drop inspect inscribe} on [items_can] {
        if {[winfo exists $w.btns.$action]} {
            $w.btns.$action state [expr {$on ? "!disabled" : "disabled"}]
        }
        set state [expr {$on ? "normal" : "disabled"}]
        switch $action {
            inscribe {
                $w.tools.inscribe state [expr {$on ? "!disabled" : "disabled"}]
                $m entryconfigure "Inscribe" -state $state
                $m entryconfigure "Uninscribe" -state $state
            }
            default { $m entryconfigure [items_menu_label $action] -state $state }
        }
    }
    lassign [items_can] - - - chosen
    $m entryconfigure "Ignore" -state [expr {$chosen ? "normal" : "disabled"}]
}

# The game's ignore menu for what is chosen, as its own entries: this one,
# every one like it, or every one of its ego or quality, and burning a book.
proc items_ignore_menu {} {
    global items

    set m .items.mb.items.ignore
    $m delete 0 end
    lassign $items(sel) view index
    if {$view eq "" || $items(selname) eq ""
            || [catch {angband_ignore item $view $index} choices]} {
        $m add command -label "Choose an item first" -state disabled
        return
    }
    foreach c $choices {
        lassign $c value label
        $m add command -label $label -command [list items_ignore $value $label]
    }
}

proc items_ignore {value label} {
    global items

    lassign $items(sel) view index
    if {[catch {angband_ignore item $view $index $value} err]} {
        set items(message) [string toupper $err 0 0]
        bell
        return
    }
    # Burning asks and answers for itself.
    switch -glob -- $label {
        "This item only"     { set items(message) "Ignoring $items(selname)." }
        "Unignore this item" { set items(message) "Unignoring $items(selname)." }
        "Unignore *"         { set items(message) "Unignoring [string range $label 9 end]." }
        "Burn *"             {}
        default              { set items(message) "Ignoring [string tolower $label 0 0]." }
    }
}

proc items_menu_label {action} {
    return [dict get {
        wear "Wear/Wield" takeoff "Take Off" drop "Drop" inspect "Inspect"
    } $action]
}

proc items_wear {} {
    global items
    lassign [items_can] can
    if {!$can} return
    lassign $items(sel) view index
    items_expect wield $items(selname)
    items_do [list wield $view $index]
}

proc items_takeoff {} {
    global items
    lassign [items_can] - can
    if {!$can} return
    lassign $items(sel) view index
    items_expect takeoff $items(selname)
    items_do [list takeoff $view $index]
}

proc items_takeoff_slot {slot} {
    items_select equipment $slot
    items_takeoff
}

# Drop is the game's drop, which puts down one of a stack unless told more,
# and the message says so rather than claiming the whole stack went.
proc items_drop {} {
    global items
    lassign [items_can] - - can
    if {!$can} return
    lassign $items(sel) view index
    set number [lindex [items_row $view $index] 4]
    items_expect drop [expr {$number > 1
        ? "one of your $items(selname)" : $items(selname)}]
    items_do [list drop $view $index]
}

# What the game says about this very object: object_info, which reads the
# runes the character has learned.
proc items_inspect {} {
    global items
    lassign [items_can] - - - can
    if {!$can} return
    lassign $items(sel) view index
    set row [items_row $view $index]
    set lore [items_gear lore $view $index]
    set items(message) "You examine $items(selname). It weighs\
        [items_weight [lindex $row 5]] lb."
    tk_messageBox -parent .items -icon info -title "Inspect" \
        -message [string toupper $items(selname) 0 0] \
        -detail [regsub -all {[ \t]+\n} [string trim $lore] "\n"]
}

proc items_inscribe {{text ""} {erase 0}} {
    global items
    lassign [items_can] - - - - can
    if {!$can} {
        set items(message) "Choose an item first."
        return
    }
    lassign $items(sel) view index
    set note [expr {$erase ? "" : $items(inscription)}]
    if {[items_do [list inscribe $view $index $note]]} {
        set items(message) [expr {$note eq ""
            ? "Uninscribed $items(selname)." : "Inscribed $items(selname)."}]
    }
}

# The letter keys act on the selection when it allows them, and otherwise go to
# the game as they always did, so with nothing chosen 'w' still asks the game's
# own question.
proc items_letter {action n s a} {
    set i [lsearch {wear takeoff drop inspect} $action]
    if {[lindex [items_can] $i]} {
        items_$action
    } else {
        angband_key $n $s $a
    }
}

# F1: the manual on the inventory commands, which are what the window does,
# and how to use the window in the status line.
proc items_help {} {
    global items
    help_window [help_first command#inventory-commands command]
    set items(message) "Drag an item onto a slot to wear it and off again to\
        stow it, or choose one and use the buttons."
}

# --- dragging ------------------------------------------------------------------
#
# Tk has no drag and drop, so this is the whole of it: press, motion, release,
# and a borrowed toplevel for the thing being carried.  The answer to "does
# this go here" is the game's, asked through angband_gear fits, so a slot the
# window thinks is compatible is one the wear command will accept.

proc items_press {w source X Y} {
    global items

    lassign $source view index
    items_select $view $index
    focus $w

    # An empty slot has nothing to pick up: no drag starts rather than a drag
    # that can only fail.
    if {$items(selname) eq ""} { set items(drag) {} ; return }
    set items(drag) [list $view $index $X $Y 0 $w]
}

proc items_motion {X Y} {
    global items

    if {![llength $items(drag)]} return
    lassign $items(drag) view index x0 y0 started w

    if {!$started} {
        if {abs($X - $x0) < 4 && abs($Y - $y0) < 4} return
        set row [items_row $view $index]
        items_ghost_show [lindex $row 6] [lindex $row 9]
        lset items(drag) 4 1
    }

    items_ghost_move $X $Y
    items_light [items_target $X $Y] $view $index $w
}

proc items_release {X Y} {
    global items

    if {![llength $items(drag)]} return
    lassign $items(drag) view index x0 y0 started w

    items_ghost_hide
    items_light {} $view $index $w
    set items(drag) {}
    if {!$started} return

    set target [items_target $X $Y]
    if {![llength $target]} return
    set name $items(selname)

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

# The drop target under the pointer.
#
# Geometry, not winfo containing, which answers for the whole application: with
# the main window stacked above this one every drop would land on the map, and
# the thing nearest the pointer during a drag is the ghost.  The wells are
# rectangles on one canvas and the pack is one widget, so ask them directly.
proc items_target {X Y} {
    global items

    set c [items_doll_canvas]
    set x [expr {$X - [winfo rootx $c]}]
    set y [expr {$Y - [winfo rooty $c]}]
    if {$x >= 0 && $y >= 0 && $x < [winfo width $c] && $y < [winfo height $c]} {
        foreach key [array names items well,*] {
            lassign $items($key) x1 y1 x2 y2
            if {$x >= $x1 - 4 && $x <= $x2 + 4 && $y >= $y1 - 4 && $y <= $y2 + 4} {
                return [list slot [string range $key 5 end]]
            }
        }
        return {}
    }

    set p .items.f.body.inv
    set x [expr {$X - [winfo rootx $p]}]
    set y [expr {$Y - [winfo rooty $p]}]
    if {$x >= 0 && $y >= 0 && $x < [winfo width $p] && $y < [winfo height $p]} {
        return [list pack]
    }
    return {}
}

# Light the hovered target, and only it.  A refused target still lights, in
# red, which is the design's point: the player should see that the slot was
# understood and is saying no.
proc items_light {target view index w} {
    global items

    if {$items(lit) eq $target} return
    set items(lit) $target

    set c [items_doll_canvas]
    set t [items_pack_text]
    $c itemconfigure hover -state hidden
    $t configure -highlightbackground [ttk::style lookup . -background]
    items_cursor $w ""
    if {![llength $target]} return

    switch -exact -- [lindex $target 0] {
        pack {
            if {$view eq "equipment"} {
                $t configure -highlightbackground black
            } else {
                items_cursor $w X_cursor
            }
        }
        slot {
            set slot [lindex $target 1]
            set ok [expr {$view ne "equipment"
                && $slot in [items_gear fits $view $index]}]
            lassign $items(well,$slot) x1 y1 x2 y2
            $c coords hover [expr {$x1 - 2}] [expr {$y1 - 2}] \
                [expr {$x2 + 2}] [expr {$y2 + 2}]
            $c itemconfigure hover -state normal \
                -outline [expr {$ok ? "#ffff00" : "#ff0000"}]
            if {!$ok} { items_cursor $w X_cursor }
        }
    }
}

# The drag's source keeps its own cursor when nothing is wrong: the pack list
# has an arrow, where a text widget would otherwise have an I-beam.
proc items_cursor {w cursor} {
    if {$cursor eq "" && [winfo class $w] eq "Text"} { set cursor arrow }
    $w configure -cursor $cursor
}

# The thing being carried, drawn inside the window: a plain label placed over
# everything else, a little below and to the right of the pointer so that it is
# never what the pointer is over.  Not the usual borderless toplevel: on macOS
# one of those is never drawn while the game's event loop is the one running,
# and the ghost came out as an empty white shape.  Inside the window is also
# the only place a drop can land.
proc items_ghost_show {glyph colour} {
    set g .items.f.ghost
    if {![winfo exists $g]} {
        label $g -font [uifont Courier 18 bold] -background black -padx 8 -pady 2 \
            -relief raised -borderwidth 1
    }
    $g configure -text $glyph \
        -foreground [expr {$colour ne "" ? $colour : "white"}]
}

proc items_ghost_move {X Y} {
    set g .items.f.ghost
    if {![winfo exists $g]} return
    place $g -x [expr {$X - [winfo rootx .items.f] + 14}] \
        -y [expr {$Y - [winfo rooty .items.f] + 14}]
    raise $g
}

proc items_ghost_hide {} {
    if {[winfo exists .items.f.ghost]} { place forget .items.f.ghost }
}

# --- the pack's mouse and keys -------------------------------------------------

# Which pack row a point in the list is, or "".
proc items_pack_at {x y} {
    global items
    set line [lindex [split [[items_pack_text] index @$x,$y] .] 0]
    set row [lindex $items(packrows) [expr {$line - 1}]]
    return [lindex $row 0]
}

proc items_pack_hover {x y} {
    set index [items_pack_at $x $y]
    if {$index ne ""} { items_recall inventory $index }
}

proc items_pack_press {x y X Y} {
    set index [items_pack_at $x $y]
    if {$index eq ""} return
    items_press [items_pack_text] [list inventory $index] $X $Y
}

# --- the window ----------------------------------------------------------------

proc items_menus {} {
    set mb .items.mb
    menu $mb
    .items configure -menu $mb

    menu $mb.file
    $mb add cascade -label File -underline 0 -menu $mb.file
    $mb.file add command -label Close -underline 0 -accelerator Esc \
        -command {destroy .items}

    menu $mb.items
    $mb add cascade -label Items -underline 0 -menu $mb.items
    $mb.items add command -label "Wear/Wield" -underline 0 -accelerator W \
        -command items_wear
    $mb.items add command -label "Take Off" -underline 0 -accelerator T \
        -command items_takeoff
    $mb.items add command -label "Drop" -underline 0 -accelerator D \
        -command items_drop
    $mb.items add command -label "Inspect" -underline 0 -accelerator I \
        -command items_inspect
    $mb.items add separator
    $mb.items add command -label "Inscribe" -underline 2 -command items_inscribe
    $mb.items add command -label "Uninscribe" -underline 0 \
        -command {items_inscribe "" 1}
    $mb.items add separator
    menu $mb.items.ignore -tearoff 0 -postcommand items_ignore_menu
    $mb.items add cascade -label "Ignore" -underline 1 -menu $mb.items.ignore

    # Not $mb.help: on macOS Tk treats a menu with that name as the platform's
    # own Help menu and fills it with things this window does not have.
    menu $mb.about
    $mb add cascade -label Help -underline 0 -menu $mb.about
    $mb.about add command -label "On Items..." -underline 0 -accelerator F1 \
        -command items_help
}

# A sunken read-out, as the other windows have.
proc items_readout {w var args} {
    ttk::label $w -textvariable $var -relief sunken -borderwidth 1 \
        -padding {4 1} {*}$args
    return $w
}

proc items_window {} {
    global items

    if {[winfo exists .items]} {
        wm deiconify .items
        raise .items
        return
    }

    toplevel .items
    wm title .items "Zangband: Items"
    wm protocol .items WM_DELETE_WINDOW { destroy .items }
    wm minsize .items 960 600

    items_menus

    # One Ttk frame under everything, because that is what carries the theme's
    # grey: the toplevel itself is drawn by the platform.
    set w .items.f
    ttk::frame $w
    pack $w -fill both -expand 1

    set ground [ttk::style lookup . -background]
    set field  [ttk::style lookup TEntry -fieldbackground {} white]
    set chosen [ttk::style lookup . -selectbackground]

    # --- status, buttons and the rule over them, packed first so that they
    # keep their room when the window is made smaller
    ttk::frame $w.status
    pack $w.status -side bottom -fill x -padx 2 -pady 2
    items_readout $w.status.msg items(message) -font [uifont 12] -anchor w
    items_readout $w.status.cap items(v,capacity) -font [uifont 12] \
        -anchor center -width -18
    pack $w.status.cap -side right
    pack $w.status.msg -side left -fill x -expand 1 -padx {0 2}

    ttk::frame $w.btns
    pack $w.btns -side bottom -fill x -padx 10 -pady 10
    foreach {action text} {
        wear "Wear/Wield" takeoff "Take Off" drop "Drop" inspect "Inspect"
    } {
        ttk::button $w.btns.$action -text $text -underline 0 -width 11 \
            -command items_$action
        pack $w.btns.$action -side left -padx {0 10}
    }
    ttk::button $w.btns.close -text Close -underline 0 -width 10 \
        -default active -command {destroy .items}
    pack $w.btns.close -side right

    ttk::separator $w.sep -orient horizontal
    pack $w.sep -side bottom -fill x

    # --- inscription and burden
    set tl $w.tools
    ttk::frame $tl
    pack $tl -side bottom -fill x -padx 10 -pady {4 8}
    ttk::label $tl.il -text "Inscription:" -underline 2
    ttk::entry $tl.insc -textvariable items(inscription) -font [uifont Courier 12] \
        -width 22
    ttk::button $tl.inscribe -text Inscribe -command items_inscribe
    ttk::label $tl.bl -text "Burden:"
    ttk::progressbar $tl.burden -style Burden.Horizontal.TProgressbar \
        -length 160
    items_readout $tl.bv items(v,burden) -anchor e -width -16
    pack $tl.il -side left
    pack $tl.insc -side left -padx {4 6}
    pack $tl.inscribe -side left
    pack $tl.bv -side right
    pack $tl.burden -side right -padx {4 6}
    pack $tl.bl -side right

    # The entry keeps what is typed into it.  Without its own bindtags the
    # toplevel's bindings would run after the entry's, and every letter typed
    # here would be inserted and played -- or worn, dropped or inspected.
    bindtags $tl.insc [list $tl.insc TEntry all]
    bind $tl.insc <Return> { items_inscribe ; break }
    bind $tl.insc <Escape> { destroy .items ; break }

    # --- the two panes
    set body $w.body
    ttk::frame $body
    pack $body -fill both -expand 1 -padx 10 -pady {10 4}

    set eq $body.eq
    ttk::labelframe $eq -text Equipment -padding 8
    canvas $eq.doll -background black -relief sunken \
        -borderwidth $items(doll,bd) -highlightthickness 0
    items_doll_sprite
    $eq.doll create image 0 0 -image items_doll -anchor center -tags sprite
    $eq.doll create rectangle 0 0 0 0 -outline #ffff00 -dash . -state hidden \
        -tags selring
    $eq.doll create rectangle 0 0 0 0 -width 2 -state hidden -tags hover
    bind $eq.doll <B1-Motion> { items_motion %X %Y }
    bind $eq.doll <ButtonRelease-1> { items_release %X %Y }

    ttk::frame $eq.read
    ttk::label $eq.read.sl -text "Selected:" -font [uifont 12]
    items_readout $eq.read.sv items(v,sel) -anchor w
    ttk::label $eq.read.wl -text "Worn:" -font [uifont 12]
    items_readout $eq.read.wv items(v,worn) -anchor e -width -8
    grid $eq.read.sl -row 0 -column 0 -sticky e -padx {0 6} -pady 2
    grid $eq.read.sv -row 0 -column 1 -sticky ew -pady 2
    grid $eq.read.wl -row 1 -column 0 -sticky e -padx {0 6} -pady 2
    grid $eq.read.wv -row 1 -column 1 -sticky w -pady 2
    grid columnconfigure $eq.read 1 -weight 1

    pack $eq.doll -anchor n
    pack $eq.read -fill x -pady {8 0}

    set inv $body.inv
    ttk::labelframe $inv -text Inventory -padding 8
    ttk::frame $inv.hdr
    ttk::label $inv.hdr.i -text "Item" -font [uifont 12]
    ttk::label $inv.hdr.w -text "Weight" -font [uifont 12]
    pack $inv.hdr.i -side left -padx {4 0}
    pack $inv.hdr.w -side right -padx {0 24}

    ttk::frame $inv.list
    set t $inv.list.t
    text $t -width 44 -height 14 -wrap none -font [uifont Courier 12] \
        -background $field -foreground black -cursor arrow \
        -relief sunken -borderwidth 2 -highlightthickness 2 \
        -highlightcolor black -highlightbackground $ground \
        -padx 4 -pady 4 -spacing1 3 -spacing3 3 -insertwidth 0 \
        -yscrollcommand [list $inv.list.sb set]
    ttk::scrollbar $inv.list.sb -orient vertical -command [list $t yview]
    pack $inv.list.sb -side right -fill y
    pack $t -side left -fill both -expand 1

    # Not "sel": that is the text widget's own selection tag.
    $t tag configure glyph -background black -font [uifont Courier 12 bold]
    $t tag configure chosen -background $chosen -relief raised -borderwidth 1
    $t tag configure empty -foreground #828282
    $t tag raise glyph

    bind $t <Configure> { items_pack_tabs }
    bind $t <ButtonPress-1> { items_pack_press %x %y %X %Y ; break }
    bind $t <Motion> { items_pack_hover %x %y }
    bind $t <B1-Motion> { items_motion %X %Y ; break }
    bind $t <ButtonRelease-1> { items_release %X %Y ; break }
    bind $t <Double-1> { items_wear ; break }
    bind $t <Up>     { items_step up ; break }
    bind $t <Down>   { items_step down ; break }
    bind $t <Home>   { items_step first ; break }
    bind $t <End>    { items_step last ; break }
    bind $t <Return> { items_wear ; break }

    ttk::frame $inv.foot
    ttk::label $inv.foot.l -text "Pack:" -font [uifont 12]
    items_readout $inv.foot.n items(v,packcount) -anchor w
    items_readout $inv.foot.w items(v,packweight) -anchor e -width -8
    pack $inv.foot.l -side left -padx {0 6}
    pack $inv.foot.w -side right
    pack $inv.foot.n -side left -fill x -expand 1 -padx {0 6}

    pack $inv.hdr -fill x
    pack $inv.foot -side bottom -fill x -pady {8 0}
    pack $inv.list -fill both -expand 1

    grid $eq  -row 0 -column 0 -sticky nsew -padx {0 12}
    grid $inv -row 0 -column 1 -sticky nsew
    grid columnconfigure $body 1 -weight 1
    grid rowconfigure $body 0 -weight 1

    # Keys typed here still play the game, so the window can be left open and
    # the letter commands keep working -- which is the design's requirement
    # that dragging be an addition to the keyboard and never a replacement.
    # w, t, d and i act on the selection when it allows them.
    bind .items <Key> { angband_key [key_sym %N %k] %s %A }
    bind .items <Escape> { destroy .items ; break }
    bind .items <F1> { items_help ; break }
    foreach {k action} {w wear t takeoff d drop i inspect} {
        bind .items <Key-$k> "items_letter $action %N %s %A ; break"
        accel_bind .items $k "items_$action ; break"
    }
    accel_bind .items s { focus .items.f.tools.insc ; break }
    accel_bind .items c { destroy .items ; break }
    bind .items <Destroy> { if {"%W" eq ".items"} { items_forget } }

    items_refresh
    focus $t
}

# Dropping the window drops everything that pointed into it, so a later refresh
# cannot try to paint a widget that is gone.
proc items_forget {} {
    global items

    array unset items well,*
    unset -nocomplain items(shape) items(burdencolour)
    set items(lit) {}
    set items(drag) {}
    set items(packrows) {}
    items_select_none
    items_expect_clear
    items_ghost_hide
}

# Up to date whether or not anyone is looking: the bindings live on "." and
# cost nothing while the window is closed, because items_refresh returns at
# once when there is none.
foreach event $items(events) {
    bind . <<Angband_$event>> {+ items_refresh }
}
