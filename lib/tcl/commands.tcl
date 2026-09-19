# The menu bar, generated from the game's own command table.
#
# This is what the front end is for.  Every entry, its label, its keyboard
# accelerator and whether it can be used right now come from cmds_all[] by way
# of angband_commands -- nothing here is hand-authored, so a command added to
# the game appears in the menus without anyone editing this file, and a menu
# item and its key can never drift apart.
#
# The accelerators are the point rather than a decoration.  They are resolved
# through cmd_lookup_key() against the player's own keyset, so the menus teach
# the keyboard instead of replacing it: a player who uses them for a week
# learns the keys by reading them.
#
# Choosing an item runs angband_command, which dispatches exactly as
# textui_process_command does -- prereq, then hook or cmdq_push_repeat.  No
# keystroke is synthesised.

# The groups that become menus, in the order the game lists them.
#
# "Hidden" is left out, as its name asks: it holds Walk, Run, Stand still and
# Repeat, which already have keys and no business in a menu, alongside Toggle
# windows and Load a single pref line, which have no business anywhere near a
# player.  The Debug groups are nested under it and are left out with it --
# they appear only when the character is already in wizard mode, since offering
# them otherwise is an invitation to spoil a game.
set commands(skip) {Hidden}

proc commands_menu_path {name} {
    # A menu path has to be a legal Tk pathname, and the group names have
    # spaces in them.
    return .menubar.cmd[string map {" " "" "'" ""} [string tolower $name]]
}

proc commands_build {} {
    global commands

    if {[catch {angband_commands} rows]} return

    # Built again on a new character, so clear out the last lot: a cascade
    # left behind would point at a destroyed menu.
    if {[info exists commands(menus)]} {
        foreach pair $commands(menus) {
            lassign $pair group m
            set i [.menubar index $group]
            if {$i ne "none" && $i ne ""} { .menubar delete $i }
            if {[winfo exists $m]} { destroy $m }
        }
    }

    set commands(rows) $rows
    set commands(menus) {}

    # Group the rows, keeping the game's order.
    set order {}
    array unset byname
    foreach row $rows {
        lassign $row gidx group idx label key enabled level code
        if {$level != 0} continue
        if {$group in $commands(skip)} continue
        if {$group ni $order} { lappend order $group }
        lappend byname($group) $row
    }

    set at 0
    foreach group $order {
        set m [commands_menu_path $group]
        if {[winfo exists $m]} { destroy $m }
        menu $m -tearoff 0 -postcommand [list commands_refresh $group]

        # Inserted, not appended: the game's own menus belong before the front
        # end's Tiles and Window, and those were built when the window was.
        .menubar insert $at cascade -label $group -menu $m
        incr at
        lappend commands(menus) [list $group $m]

        foreach row $byname($group) {
            lassign $row gidx g idx label key enabled level code
            $m add command -label $label -accelerator $key \
                -command [list angband_command $gidx $idx]
        }
    }
}

# Grey out what cannot be done, just before the menu opens.
#
# A postcommand rather than an event handler: availability changes with almost
# everything the player does, and asking at the moment somebody looks is both
# always right and free the rest of the time.
proc commands_refresh {group} {
    global commands

    if {[catch {angband_commands} rows]} return

    set m [commands_menu_path $group]
    set i 0
    foreach row $rows {
        lassign $row gidx g idx label key enabled level code
        if {$g ne $group} continue
        if {$level != 0} continue
        $m entryconfigure $i -state [expr {$enabled ? "normal" : "disabled"}] \
            -accelerator $key
        incr i
    }
}

# The menus are built once the game has a command table to build them from.
# cmd_init() runs inside textui_init(), which is after this file is sourced, so
# building at source time would produce a menu bar with no accelerators in it.
bind . <<Angband_ENTER_GAME>> {+ commands_build }
bind . <<Angband_LEAVE_BIRTH>> {+ commands_build }
