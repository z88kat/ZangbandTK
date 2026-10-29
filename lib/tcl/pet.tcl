# The Pets window.
#
# The figurine (DEC-144) was the last of Zangband's routes to a pet, so the
# game can now give a character animals by five separate means and had nowhere
# to look at them except the keyboard's own text screen.  This is that screen
# as a window: what follows you, what it costs, and the orders you can give.
#
# Built on angband_pet, which is the testable half.  Everything here is a
# rendering of what that command returns, so the numbers can be checked
# without anyone looking at a window -- see the pet checks in
# src/tcl/tests/bridge.tcl.
#
# Orders are a policy for all pets at once rather than instructions to an
# individual.  That is Zangband's shape and do_cmd_pet's: the player sets how
# their animals behave, not what each one is doing.  So the orders are
# radiobuttons over the whole stable and the roster below them is read-only.
#
# Dismissing is not here.  dismiss_pets() asks through get_check(), the game's
# own text prompt, and opening one from a Tk binding is where both of this
# front end's known crashes came from.  It stays on the keyboard command until
# it has a prompt of its own.

namespace eval pet {
    variable P

    # The order in force, as a radiobutton variable.  Written from the game's
    # answer on every refresh, so it can never drift from what the game holds.
    set P(order) ""
    set P(doors) 0
    set P(pickup) 0
    set P(msg) ""
}

# The five orders, and what to call them.  The names are angband_pet's; the
# labels are do_cmd_pet's, so the window and the keyboard say the same words.
proc pet_labels {} {
    return {
        close   "Stay close"
        follow  "Follow me"
        destroy "Seek and destroy"
        space   "Give me space"
        away    "Stay away"
    }
}

# How to say a distance, given both numbers the game reports.
#
# This is the whole reason `effective` exists.  "Seek and destroy" sets the
# leash to 255 and no pet ever wanders 255 grids: a pet further off than the
# seek distance comes looking for you whatever its leash, so 10 is what
# happens.  A window that printed 255 would be stating a number the game never
# uses.  Negative leashes are keep-away distances and are not clamped, so for
# those the two agree and only one is shown.
proc pet_distance_text {set effective seek} {
    if {$set < 0} {
        return "keeps [expr {abs($set)}] grids away"
    }
    if {$effective != $set} {
        return "$effective grids -- the order says $set, but a pet further\
                off than $seek comes back whatever its leash"
    }
    return "$effective grids"
}

# What the upkeep is doing, in a sentence.
#
# The allowance is a count and the charge is a sum of levels, so one pet over
# the allowance starts charging for the whole stable rather than for the extra
# one.  That is the thing players get wrong, so it is said before it happens
# as well as after.
proc pet_upkeep_text {d} {
    set count   [dict get $d count]
    set free    [dict get $d free]
    set levels  [dict get $d levels]
    set upkeep  [dict get $d upkeep]

    if {$count == 0} {
        return "Nothing follows you. You could keep $free for nothing."
    }
    if {[dict get $d charged]} {
        return "$count following, $free free -- over the allowance, so the\
                whole stable is charged: $upkeep% less mana regained\
                ($levels levels between them)."
    }
    set room [expr {$free - $count}]
    if {$room == 0} {
        return "$count following, and $free is your allowance. One more and\
                the whole stable is charged, not just the extra one."
    }
    return "$count following, $free free. Room for $room more before the\
            whole stable is charged."
}

proc pet_refresh {} {
    variable pet::P

    if {![winfo exists .pet]} return

    if {[catch {angband_pet} d]} {
        set pet::P(msg) $d
        return
    }

    set pet::P(order)  [dict get $d order]
    set pet::P(doors)  [dict get $d doors]
    set pet::P(pickup) [dict get $d pickup]
    set pet::P(msg)    [pet_upkeep_text $d]

    .pet.f.hdr.dist configure -text "Orders: [pet_distance_text \
        [dict get $d distance] [dict get $d effective] [dict get $d seek]]"

    set t .pet.f.list.t
    $t configure -state normal
    $t delete 1.0 end

    if {[catch {angband_pet list} rows]} { set rows {} }
    if {![llength $rows]} {
        $t insert end "Nothing follows you.\n" dim
    } else {
        $t insert end [format "%-44s %6s %9s\n" "" "level" "distance"] head
        foreach row $rows {
            lassign $row idx name level dist
            $t insert end [format "%-44s %6d %9d\n" $name $level $dist]
        }
    }
    $t configure -state disabled
}

# Give an order.
#
# The game can refuse -- it is in the middle of something else -- and the
# radiobutton has already moved by the time this runs, so a refusal has to put
# it back.  Reading the answer from the game rather than from the variable is
# the same rule the distance follows: show what is true, not what was asked
# for.
proc pet_order {name} {
    if {[catch {angband_pet order $name} err]} {
        errorsink::record "pet order" $err $::errorInfo
        bell
    }
    pet_refresh
}

proc pet_toggle {what var} {
    if {[catch {angband_pet $what [set $var]} err]} {
        errorsink::record "pet $what" $err $::errorInfo
        bell
    }
    pet_refresh
}

proc pet_help {} {
    help_window [help_first pets#what-they-cost pets]
}

proc pet_window {} {
    if {[winfo exists .pet]} {
        pet_refresh
        wm deiconify .pet
        raise .pet
        return
    }

    toplevel .pet
    wm title .pet "Zangband: Pets"
    wm minsize .pet 560 360

    set mb .pet.mb
    menu $mb
    .pet configure -menu $mb
    menu $mb.pets -tearoff 0
    $mb add cascade -label Pets -underline 0 -menu $mb.pets
    $mb.pets add command -label Refresh -underline 0 -command pet_refresh
    $mb.pets add separator
    $mb.pets add command -label Close -underline 0 -command {destroy .pet}
    # Not $mb.help, for the reason store.tcl gives.
    menu $mb.about -tearoff 0
    $mb add cascade -label Help -underline 0 -menu $mb.about
    $mb.about add command -label "On pets..." -underline 3 \
        -accelerator F1 -command pet_help

    set w .pet.f
    ttk::frame $w
    pack $w -fill both -expand 1

    ttk::label $w.status -textvariable pet::P(msg) -font [uifont 12] \
        -relief sunken -borderwidth 1 -anchor w -padding {4 1} -wraplength 700
    pack $w.status -side bottom -fill x -padx 2 -pady 2

    # Close, as character.tcl, knowledge.tcl, items.tcl and book.tcl have it:
    # the button, the accelerator its underline advertises, and the window
    # manager's own close box bound explicitly rather than left to Tk's
    # default. Packed after the status line and also to the bottom, so the
    # status sits below the buttons exactly as it does in those four.
    #
    # Nothing here asks command_may_start(). Closing is a Tk operation --
    # `destroy` and nothing else -- and no close path in any window in this
    # front end goes near the command guard. DEC-149 draws the line at acting
    # on the game, and shutting a window is not that: pet_refresh returns at
    # once when the window is gone, so a close landing in the middle of an
    # order leaves nothing half-done.
    ttk::frame $w.btns
    pack $w.btns -side bottom -fill x -padx 10 -pady 10
    ttk::button $w.btns.close -text Close -underline 0 -width 10 \
        -default active -command {destroy .pet}
    pack $w.btns.close -side right

    set h $w.hdr
    ttk::frame $h
    pack $h -side top -fill x -padx 10 -pady {10 4}
    ttk::label $h.dist -font [uifont 12] -anchor w
    pack $h.dist -side top -fill x

    set o $w.orders
    ttk::labelframe $o -text "Orders to the whole stable" -padding 6
    pack $o -side top -fill x -padx 10 -pady 4
    foreach {name label} [pet_labels] {
        ttk::radiobutton $o.$name -text $label -value $name \
            -variable pet::P(order) -command [list pet_order $name]
        pack $o.$name -side top -anchor w
    }
    ttk::separator $o.sep -orient horizontal
    pack $o.sep -side top -fill x -pady 4
    ttk::checkbutton $o.doors -text "Pets may open doors" \
        -variable pet::P(doors) \
        -command [list pet_toggle doors pet::P(doors)]
    ttk::checkbutton $o.pickup -text "Pets may pick up items" \
        -variable pet::P(pickup) \
        -command [list pet_toggle pickup pet::P(pickup)]
    pack $o.doors $o.pickup -side top -anchor w

    set f $w.list
    ttk::frame $f
    pack $f -fill both -expand 1 -padx 10 -pady {4 10}
    text $f.t -width 70 -height 12 -font termfont -wrap none \
        -background black -foreground white -relief sunken -borderwidth 2 \
        -highlightthickness 0 -cursor {} -yscrollcommand [list $f.sb set] \
        -state disabled
    ttk::scrollbar $f.sb -orient vertical -command [list $f.t yview]
    pack $f.sb -side right -fill y
    pack $f.t -side left -fill both -expand 1
    $f.t tag configure head -foreground #a0a0a0
    $f.t tag configure dim -foreground #808080
    # Read only: the window's own bindings and the wheel.
    bindtags $f.t [list $f.t .pet all]
    bind $f.t <MouseWheel> { tk::MouseWheel %W y %D -40.0 }

    wm protocol .pet WM_DELETE_WINDOW { destroy .pet }
    bind .pet <Escape> { destroy .pet }
    bind .pet <F1> { pet_help ; break }
    accel_bind .pet c { destroy .pet ; break }

    pet_refresh
}

# The window stays current whether or not it is open: pet_refresh returns at
# once when there is no window, so the bindings cost nothing while it is shut.
#
# MONSTERLIST covers a pet arriving, dying or wandering; PLAYERLEVEL because
# the free allowance is a function of the character's level, so gaining one
# can move the stable from free to charged without any pet changing.
foreach event {MONSTERLIST PLAYERLEVEL ENTER_GAME NEW_LEVEL_DISPLAY} {
    bind . <<Angband_$event>> {+ refresh_soon pet_refresh }
}
