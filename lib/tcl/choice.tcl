# The Choice pane: the bottom right of the main window.
#
# The original's Choice window, as a pane (decision 16 puts it in the main
# window with the map, the minimap, the messages and the recall).  Most of the
# time it shows the pack, and clicking an item opens a menu of what can be done
# with it -- the game's own item menu, the same entries greyed the same way,
# each with the key that does it from the keyboard.  When the game asks for an
# item -- "Quaff which potion?" -- it shows the question and exactly the items
# the game will accept, and a click answers it.  The keyboard answers it as it
# always did: the game's own prompt is still running, and the pane only offers
# a second way in.  See angband_choice in main-tcl.c for both.
#
# It is a text widget rather than a term, because a term cannot be clicked on,
# and it is drawn the way its neighbours are: black, in the term's font, each
# name in the colour the game's own item list gives it.

set choicepane(open) 0
set choicepane(rows) {}
set choicepane(hot) ""

proc choice_text {} { return .pw.bottom.choice.t }

# Each row's line in the text, counting the header as line 1.
proc choice_line {i} { return [expr {$i + 2}] }

proc choice_refresh {} {
    global choicepane

    set t [choice_text]
    if {![winfo exists $t]} return

    if {[catch {angband_choice} state]} {
        set state {0 ""}
        set rows {}
        set have 0
    } else {
        set rows [angband_choice list]
        set have 1
    }
    lassign $state open prompt
    set choicepane(open) $open
    set choicepane(rows) $rows
    set choicepane(hot) ""

    $t configure -state normal
    $t delete 1.0 end

    # The header: the game's question while it is asking, the burden line the
    # game's own inventory window puts there otherwise.
    if {$open} {
        $t insert end $prompt prompt "  " {} "(click to choose)" hint "\n"
    } elseif {$have} {
        $t insert end [angband_choice burden] hint "\n"
    } else {
        $t insert end "\n"
    }

    foreach row $rows {
        lassign $row n view label name weight glyph gcolour ncolour
        foreach c [list $gcolour $ncolour] {
            if {$c ne ""} { $t tag configure fg$c -foreground $c }
        }
        set where [expr {$open && $view ne "inventory" ? "  ($view)" : ""}]
        $t insert end "$label) " label " $glyph " [list glyph fg$gcolour] \
            " $name" fg$ncolour $where hint \
            "\t[choice_weight $weight] lb\n" weight
    }
    if {$have && ![llength $rows]} {
        $t insert end [expr {$open ? "Nothing to choose." : "Your pack is empty."}] hint
    }
    $t tag raise glyph
    $t configure -state disabled
    $t yview moveto 0
}

proc choice_weight {tenths} {
    return [format "%d.%d" [expr {$tenths / 10}] [expr {$tenths % 10}]]
}

# The weight column is a right-aligned tab at the far edge, which moves when
# the pane is resized.
proc choice_tabs {} {
    set t [choice_text]
    set inset [expr {2 * [$t cget -padx] + 4}]
    $t configure -tabs [list [expr {[winfo width $t] - $inset}] right]
}

# Which row is under the pointer, or "".
proc choice_row_at {x y} {
    global choicepane
    set line [lindex [split [[choice_text] index @$x,$y] .] 0]
    set i [expr {$line - 2}]
    if {$i < 0 || $i >= [llength $choicepane(rows)]} { return "" }
    return $i
}

# The row under the pointer lights up, which is the whole of the invitation to
# click it: to answer the game's question while it is asking, and for the
# item's menu otherwise.
proc choice_motion {x y} {
    global choicepane

    set i [choice_row_at $x $y]
    if {$i eq $choicepane(hot)} return
    set t [choice_text]
    $t tag remove hot 1.0 end
    set choicepane(hot) $i
    if {$i ne ""} {
        set line [choice_line $i]
        $t tag add hot $line.0 "$line.0 lineend + 1 char"
    }
    $t configure -cursor [expr {$i ne "" ? "hand2" : "arrow"}]
}

proc choice_click {x y X Y} {
    global choicepane

    set i [choice_row_at $x $y]
    if {$i eq ""} return
    if {!$choicepane(open)} {
        choice_menu $i $X $Y
        return
    }
    if {[catch {angband_choice pick [lindex $choicepane(rows) $i 0]} err]} {
        bgerror $err
    }
}

# An item's menu: its name, then what can be done with it.  The entries and
# which are greyed are the game's -- angband_choice actions asks the same
# questions the game's own item menu asks -- and each carries its key in
# brackets, as the menu bar's do, so the menu teaches the keyboard rather than
# replacing it.
proc choice_menu {i X Y} {
    global choicepane

    if {$choicepane(open)} return
    lassign [lindex $choicepane(rows) $i] n view label name
    if {[catch {angband_choice actions $n} actions]} return

    set m .pw.bottom.choice.menu
    destroy $m
    menu $m -tearoff 0
    $m add command -label [string toupper $name 0 0] -state disabled
    $m add separator
    set texts {}
    foreach action $actions { lappend texts [lindex $action 1] }
    set column [commands_pad $texts]
    foreach action $actions {
        lassign $action id text key valid
        $m add command -label [commands_label $text $key $column] \
            -state [expr {$valid ? "normal" : "disabled"}] \
            -command [list choice_act $n $id $name]
    }
    tk_popup $m $X $Y
}

proc choice_act {n id name} {
    # Inspect is the window's: the game's description in a dialog, as the
    # Items window shows it, rather than a screen drawn over the map.
    if {$id == -1} {
        set lore [angband_gear lore inventory $n]
        tk_messageBox -parent . -icon info -title "Inspect" \
            -message [string toupper $name 0 0] \
            -detail [regsub -all {[ \t]+\n} [string trim $lore] "\n"]
        return
    }
    # Refused only when the game has something else on screen, a -more- or a
    # prompt, and then the game's own screen is the explanation.
    if {[catch {angband_choice act $n $id}]} { bell }
}

proc choice_build {} {
    global angband

    set f .pw.bottom.choice
    pack propagate $f 0

    set t $f.t
    text $t -font termfont -background black -foreground white \
        -borderwidth 0 -highlightthickness 0 -padx 4 -pady 2 \
        -wrap none -cursor arrow -insertwidth 0 -state disabled \
        -selectbackground black -inactiveselectbackground black
    pack $t -fill both -expand 1

    $t tag configure prompt -foreground white
    $t tag configure hint -foreground #808080
    $t tag configure label -foreground white
    $t tag configure weight -foreground #c0c0c0
    $t tag configure glyph -background black
    $t tag configure hot -background #303060

    # The Text class's own bindings would select, scroll and move an insert
    # cursor nobody can see.  Without them, keys go on to "." and so to the
    # game, as they do from every other pane.
    bindtags $t [list $t . all]
    bind $t <Configure> { choice_tabs }
    bind $t <Motion> { choice_motion %x %y }
    bind $t <Leave> { choice_motion -1 -1 }
    bind $t <ButtonPress-1> { choice_click %x %y %X %Y }
    # The menu from the other button too, which is where the original kept it;
    # Control-click is the Mac's right button.
    foreach b {<ButtonPress-3> <Control-ButtonPress-1>} {
        bind $t $b {
            set i [choice_row_at %x %y]
            if {$i ne ""} { choice_menu $i %X %Y }
        }
    }
    bind $t <MouseWheel> { %W yview scroll [expr {-(%D)}] units }
}

choice_build
choice_refresh

# Up to date whether or not the game is asking: the pack changes, and the
# prompt opens and closes.
foreach event {CHOOSE INVENTORY EQUIPMENT ENTER_GAME LEAVE_BIRTH ENTER_WORLD} {
    bind . <<Angband_$event>> {+ choice_refresh }
}
