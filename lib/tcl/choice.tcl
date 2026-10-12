# The Choice pane: the bottom right of the main window.
#
# The original's Choice window, as a pane (decision 16 puts it in the main
# window with the map, the minimap, the messages and the recall).  Most of the
# time it shows the pack, and clicking an item opens a menu of what can be done
# with it -- the game's own item menu, the same entries greyed the same way,
# each with the key that does it from the keyboard.
#
# When the game asks a question it shows that instead, and a click answers it:
# an item ("Quaff which potion?"), a spell ("Cast which spell?", with the
# level, mana, failure and information columns of the game's spell menu), a
# choice from one of the game's menus -- racial, class and mutation powers, the
# Mindcrafter's psionics, pet orders, the town's services -- or yes or no ("A
# bed until morning for 50 au?").  The keyboard answers
# it as it always did: the game's own prompt is still running, and the pane only
# offers a second way in.  See angband_choice in main-tcl.c for all of it.
#
# It is a text widget rather than a term, because a term cannot be clicked on,
# and it is drawn the way its neighbours are: black, in the term's font, each
# name in the colour the game's own item list gives it.

set choicepane(open) 0
set choicepane(kind) ""
set choicepane(rows) {}
set choicepane(valid) {}
set choicepane(first) 2
set choicepane(hot) ""

proc choice_text {} { return .pw.bottom.choice.t }

# Each row's line in the text.  The header takes one line, and a spell list a
# second for its column headings.
proc choice_line {i} {
    global choicepane
    return [expr {$i + $choicepane(first)}]
}

proc choice_refresh {} {
    global choicepane

    set t [choice_text]
    if {![winfo exists $t]} return

    if {[catch {angband_choice} state]} {
        set state {0 "" ""}
        set rows {}
        set have 0
    } else {
        set rows [angband_choice list]
        set have 1
    }
    lassign $state open prompt kind
    set choicepane(open) $open
    set choicepane(kind) $kind
    set choicepane(rows) $rows
    set choicepane(valid) {}
    set choicepane(first) 2
    set choicepane(hot) ""

    $t configure -state normal
    $t delete 1.0 end

    # The header: the game's question while it is asking, the burden line the
    # game's own inventory window puts there otherwise.  Wrapped by hand to the
    # pane, because a menu's question is whatever sentence the game printed
    # above it and the list itself must not wrap.
    set cols [choice_cols]
    if {$open} {
        set lines [choice_wrap $prompt $cols]
        set last [lindex $lines end]
        set hint "(click to choose)"
        foreach line [lrange $lines 0 end-1] { $t insert end $line prompt "\n" }
        if {[string length "$last  $hint"] <= $cols} {
            $t insert end $last prompt "  $hint" hint "\n"
        } else {
            $t insert end $last prompt "\n" {} $hint hint "\n"
            lappend lines $hint
        }
        set choicepane(first) [expr {[llength $lines] + 1}]
    } elseif {$have} {
        $t insert end [angband_choice burden] hint "\n"
    } else {
        $t insert end "\n"
    }

    switch -- $kind {
        spell {
            # The game's spell menu, column for column.  The name column gives
            # way to the information column, which is the one that says what
            # a spell does: at a term's width the game's own 30 characters of
            # name leave no room for it at all.
            set infow 0
            foreach row $rows {
                set infow [expr {max($infow, [string length [lindex $row 6]])}]
            }
            set infow [expr {min($infow, 16)}]
            set namew [expr {max(10, min(26, $cols - 18 - $infow))}]
            $t insert end [format "   %-*s %2s %4s %4s  %s\n" $namew \
                Name Lv Mana Fail Info] hint
            incr choicepane(first)
            foreach row $rows {
                lassign $row n label name level mana fail comment colour valid
                $t tag configure fg$colour -foreground $colour
                $t insert end "$label) " [expr {$valid ? "label" : "off"}] \
                    [format "%-*s %2d %4d %3d%%  %s" $namew \
                        [string range $name 0 [expr {$namew - 1}]] \
                        $level $mana $fail $comment] \
                    [expr {$valid ? "fg$colour" : "off"}] "\n"
                lappend choicepane(valid) $valid
            }
        }
        menu - check {
            foreach row $rows {
                lassign $row n label text valid
                set tag [expr {$valid ? "label" : "off"}]
                $t insert end [expr {$label ne "" ? "$label) " : "   "}] $tag \
                    $text [expr {$valid ? {} : "off"}] "\n"
                lappend choicepane(valid) $valid
            }
        }
        default {
            foreach row $rows {
                lassign $row n view label name weight glyph gcolour ncolour
                foreach c [list $gcolour $ncolour] {
                    if {$c ne ""} { $t tag configure fg$c -foreground $c }
                }
                set where [expr {$open && $view ne "inventory" ? "  ($view)" : ""}]
                $t insert end "$label) " label " $glyph " [list glyph fg$gcolour] \
                    " $name" fg$ncolour $where hint \
                    "\t[choice_weight $weight] lb\n" weight
                lappend choicepane(valid) 1
            }
        }
    }
    if {$have && ![llength $rows]} {
        $t insert end [expr {$open ? "Nothing to choose." : "Your pack is empty."}] hint
    }
    $t tag raise glyph
    $t configure -state disabled
    $t yview moveto 0
}

# How many of the term font's characters fit across the pane.
proc choice_cols {} {
    set t [choice_text]
    return [expr {max(30, ([winfo width $t] - 2 * [$t cget -padx])
        / [font measure termfont 0])}]
}

# Words into lines of at most `cols` characters.
proc choice_wrap {text cols} {
    set lines {}
    set line ""
    foreach word [split $text " "] {
        if {$word eq ""} continue
        if {$line ne "" && [string length "$line $word"] > $cols} {
            lappend lines $line
            set line $word
        } else {
            set line [expr {$line eq "" ? $word : "$line $word"}]
        }
    }
    lappend lines $line
    return $lines
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
    set i [expr {$line - $choicepane(first)}]
    if {$i < 0 || $i >= [llength $choicepane(rows)]} { return "" }
    return $i
}

# Whether a row can be chosen: everything in the pack can be clicked for its
# menu, and while the game is asking, whatever the game would accept.
proc choice_row_ok {i} {
    global choicepane
    if {$i eq ""} { return 0 }
    return [expr {[lindex $choicepane(valid) $i] ne "0"}]
}

# The row under the pointer lights up, which is the whole of the invitation to
# click it: to answer the game's question while it is asking, and for the
# item's menu otherwise.  An item pointed at is described in the Recall pane,
# as the original's Choice window did, so the pane answers "what is this?"
# before the click rather than after it.
proc choice_motion {x y} {
    global choicepane

    set i [choice_row_at $x $y]
    if {![choice_row_ok $i]} { set i "" }
    if {$i eq $choicepane(hot)} return
    set t [choice_text]
    $t tag remove hot 1.0 end
    set choicepane(hot) $i
    if {$i ne ""} {
        set line [choice_line $i]
        $t tag add hot $line.0 "$line.0 lineend + 1 char"
        if {$choicepane(kind) in {"" item}} {
            catch {angband_choice recall [lindex $choicepane(rows) $i 0]}
        }
    }
    $t configure -cursor [expr {$i ne "" ? "hand2" : "arrow"}]
}

proc choice_click {x y X Y} {
    global choicepane

    set i [choice_row_at $x $y]
    if {![choice_row_ok $i]} return
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
    $t tag configure off -foreground #606060
    $t tag configure glyph -background black
    $t tag configure hot -background #303060

    # The Text class's own bindings would select, scroll and move an insert
    # cursor nobody can see.  Without them, keys go on to "." and so to the
    # game, as they do from every other pane.
    bindtags $t [list $t . all]
    bind $t <Configure> {
        choice_tabs
        # A question's lines and a spell list's columns are sized to the
        # pane, so follow it.
        if {$choicepane(open)} { choice_refresh }
    }
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
    # Three lines a notch, as Tk's own lists scroll.  Tk 9 reports a notch as
    # 120, and this used to scroll by that many lines -- to the end, at once.
    bind $t <MouseWheel> { tk::MouseWheel %W y %D -40.0 }
}

choice_build
choice_refresh

# Up to date whether or not the game is asking: the pack changes, and the
# prompt opens and closes.
foreach event {CHOOSE INVENTORY EQUIPMENT ENTER_GAME LEAVE_BIRTH ENTER_WORLD} {
    bind . <<Angband_$event>> {+ choice_refresh }
}
