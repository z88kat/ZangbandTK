# The menu bar, laid out as the original's was.
#
# File, Inventory, Action and Other, with Magic and Use under Inventory and
# Alter, Looking, Movement and Resting under Action, as ZangbandTk had them --
# the layout the project owner drew up on 12 October 2026.  It replaced menus
# that were the game's own command groups, one entry a command under the
# game's long descriptions, which were complete and hard to read.
#
# What the layout decides is where each command goes and what it is called.
# Everything else is still the game's: each entry names a command by its
# description in cmds_all[], and its key, whether it can be used now, and what
# choosing it does all come from angband_commands -- so the roguelike keyset
# shows its own keys, and a command the game does not have is not shown.  A
# bridge check fails if any of the game's commands is in none of the menus, so
# a command added to the game has to be given a place.
#
# The accelerators are the point rather than a decoration.  They are resolved
# through cmd_lookup_key() against the player's own keyset, so the menus teach
# the keyboard instead of replacing it: a player who uses them for a week
# learns the keys by reading them.
#
# Choosing an item runs angband_command, which dispatches exactly as
# textui_process_command does -- prereq, then hook or cmdq_push_repeat.  No
# keystroke is synthesised.
#
# An item is {cmd Label "the game's description"}, {menu Label {items}},
# {tk Label script} for the front end's own, {tiles Label} for the tile sets,
# or "-" for a rule.  {instead Label "the game's description" script} is a
# front-end window standing in for one of the game's commands -- the Options
# window for the game's options screen, as the original's Preferences had it.
# It counts as that command's place, and shows no key, because the key still
# does what the game does with it.
set commands(layout) {
    File {
        {cmd Save "Save and don't quit"}
        -
        {cmd "Quit with Save" "Save and quit"}
        {cmd Retire "Retire character and quit"}
    }
    Inventory {
        {cmd Equipment "Display equipment listing"}
        {cmd Inventory "Display inventory listing"}
        {cmd Quiver "Display quiver listing"}
        -
        {menu Magic {
            {cmd Activate "Activate an object"}
            {cmd "Aim Wand" "Aim a wand"}
            {cmd "Drink Potion" "Quaff a potion"}
            {cmd "Read Scroll" "Read a scroll"}
            {cmd "Use Staff" "Use a staff"}
            {cmd "Zap Rod" "Zap a rod"}
            -
            {cmd Browse "Browse a book"}
            {cmd Study "Gain new spells"}
            {cmd Cast "Cast a spell"}
            {cmd "Use Power" "Use a power"}
        }}
        {menu Use {
            {cmd Drop "Drop an item"}
            {cmd "Pick Up" "Pick up objects"}
            {cmd "Take Off" "Take off/unwield an item"}
            {cmd Wear/Wield "Wear/wield an item"}
            {cmd Ignore "Ignore an item"}
            -
            {cmd "Eat Food" "Eat some food"}
            {cmd "Fire Missile" "Fire your missile weapon"}
            {cmd "Fire at Nearest" "Fire at nearest target"}
            {cmd "Fuel Light" "Fuel your light source"}
            {cmd Throw "Throw an item"}
            {cmd "Use Any Item" "Use an item"}
        }}
        -
        {cmd Inspect "Examine an item"}
        -
        {cmd Inscribe "Inscribe an object"}
        {cmd Uninscribe "Uninscribe an object"}
        {cmd "Show Ignored" "Toggle ignoring of items"}
        {cmd Autopickup "Do autopickup"}
    }
    Action {
        {menu Alter {
            {cmd Alter "Alter a grid"}
            {cmd Close "Close a door"}
            {cmd Disarm "Disarm a trap or chest"}
            {cmd Open "Open a door or a chest"}
            {cmd Tunnel "Dig a tunnel"}
            {cmd "Walk into Trap" "Walk into a trap"}
        }}
        {menu Looking {
            {cmd Look "Look around"}
            {cmd Map "Full dungeon map"}
            {cmd Locate "Locate player on map"}
            {cmd "Center Map" "Center map"}
            {cmd "Item List" "Display visible item list"}
            {cmd "Monster List" "Display visible monster list"}
        }}
        {menu Movement {
            {cmd "Go Down" "Go down staircase"}
            {cmd "Go Up" "Go up staircase"}
            {cmd Run "Start running"}
            {cmd Walk "Walk"}
            {cmd Explore "Start exploring"}
        }}
        {menu Resting {
            {cmd Rest "Rest for a while"}
            {cmd Stay "Stand still"}
        }}
        -
        {cmd Note "Take notes"}
        {cmd Repeat "Repeat previous command"}
        {cmd Target "Target monster or location"}
        {cmd "Target Closest" "Target closest monster"}
        -
        {cmd Pets "Command pets"}
        {cmd Steal "Steal from a monster"}
    }
    Other {
        {cmd "Character Info" "Character description"}
        {cmd Feeling "Repeat level feeling"}
        {cmd Knowledge "Check knowledge"}
        {cmd "Quest Status" "Things you have taken on"}
        {cmd Abilities "View abilities"}
        {cmd "Identify Symbol" "Identify symbol"}
        -
        {cmd "Previous Message" "Show previous message"}
        {cmd "Message History" "Show previous messages"}
        {cmd "Screen Dump" {Save "screen dump"}}
        -
        {menu Preferences {
            {instead Options "Interact with options" options_window}
            {tiles Tiles}
            {cmd "Load Pref Line" "Load a single pref line"}
        }}
        -
        {cmd Redraw "Redraw the screen"}
        {cmd Version "Version info"}
        {cmd "Wizard Mode" "Toggle wizard mode"}
        {cmd "Debug Commands" "Debug mode commands"}
        {cmd Borg "Borg commands"}
        {cmd "Toggle Windows" "Toggle windows"}
    }
}

# The game's Help goes at the top of the front end's Help menu, above Tips.
set commands(help) "Help"

# The debug groups are a menu of their own, and only for a character who is
# already in wizard mode -- offering them otherwise is an invitation to spoil a
# game.  They nest: the "Debug" group's nine entries are access points with no
# key and no command, each naming the group it opens.
set commands(debug) Debug

# The key an entry answers to, as its -accelerator.
#
# An accelerator is what puts the keys in a column of their own down the right
# of the menu, where every other application keeps its shortcuts.  It replaced
# keys in brackets at the end of the label, padded with spaces to line up --
# which they never quite did, a proportional font being what it is.
#
# On the Mac, Tk hands an accelerator to AppKit as the item's key equivalent,
# and AppKit draws a key equivalent as a key, not a character: v is shown as V,
# and V would be too.  That was why there were no accelerators before, since V
# is Version info and v is Throw.  The answer is to describe the key the way
# the Mac does: an uppercase key is passed as Shift-V and drawn as ⇧V, ^S as
# Control-s and drawn as ⌃S, and the menu then reads like any other Mac menu.
#
# Two things had to be ruled out first, and both were tried rather than taken
# on trust.  A key equivalent is not a second way in for the key: Tk's menus
# run nothing from one, so the key reaches the game once, through Tk's own
# bindings, and at most the menu title flashes.  And since macOS 12 AppKit
# moves a key equivalent to wherever its character is on the current keyboard
# layout, which on a German one showed { as Ö, ? as ß and ) as 0.  The front
# end turns that off for the whole application as it starts, in
# src/tcl/menukeys.c.
#
# Elsewhere Tk draws an accelerator as the text it is given, so the game's own
# spelling stands: v, V, ^S.
proc commands_accel {key} {
    if {$key eq "" || [tk windowingsystem] ne "aqua"} { return $key }
    if {[string length $key] == 2 && [string index $key 0] eq "^"} {
        return "Control-[string tolower [string index $key 1]]"
    }
    if {[string length $key] == 1 && [string is upper $key]} {
        return "Shift-$key"
    }
    return $key
}

# Where a label sits in the menu bar, or -1.
#
# `.menubar index Foo` raises "bad menu entry index" when there is no such
# entry rather than answering "none", so every place that looks for one has to
# catch.  Doing it in one place means the rebuild cannot be aborted half way by
# a label that happens not to be there.
proc commands_bar_index {label} {
    if {[catch {.menubar index $label} i]} { return -1 }
    if {$i eq "none" || $i eq ""} { return -1 }
    return $i
}

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
            lassign $pair group m shown
            set i [commands_bar_index $shown]
            if {$i >= 0} { .menubar delete $i }
            if {[winfo exists $m]} { destroy $m }
        }
    }

    set commands(rows) $rows
    set commands(menus) {}
    set commands(absent) {}
    set commands(instead) {}
    set commands(byname) [dict create]

    # `array unset commands nested,*`, not `array unset commands(nested)`.
    # The latter unsets one element called "nested", which does not exist, so
    # the rows lappend onto the previous build's -- and the second build tries
    # to create every debug submenu a second time.  Reported as
    # "window name dbgobj already exists in parent", which is a rebuild
    # happening at all: ENTER_GAME and LEAVE_BIRTH both fire for a new
    # character.
    array unset commands nested,*
    array unset commands entries,*
    foreach row $rows {
        lassign $row gidx group idx label key enabled level code opens
        if {$level == 0} {
            dict set commands(byname) $label $row
        } else {
            lappend commands(nested,$group) $row
        }
    }

    # Inserted, not appended: the game's menus belong before the front end's
    # Window and Help, and those were built when the window was.
    set at 0
    foreach {title items} $commands(layout) {
        set m [commands_menu_path $title]
        commands_fill $m $items
        .menubar insert $at cascade -label $title -menu $m
        incr at
        lappend commands(menus) [list $title $m $title]
    }

    commands_help
    commands_build_debug
    commands_check_debug
}

# One menu of the layout, and the ones under it.
proc commands_fill {m items} {
    global commands

    if {[winfo exists $m]} { destroy $m }
    menu $m -tearoff 0 -postcommand [list commands_refresh $m]
    set commands(entries,$m) {}

    set n 0
    foreach item $items {
        switch -- [lindex $item 0] {
            - {
                $m add separator
            }
            cmd {
                lassign $item kind label desc
                if {![dict exists $commands(byname) $desc]} {
                    # Borg commands, in a build without the Borg.
                    lappend commands(absent) $desc
                    continue
                }
                lassign [dict get $commands(byname) $desc] gidx g idx name key
                $m add command -command [list angband_command $gidx $idx] \
                    -label $label -accelerator [commands_accel $key]
                lappend commands(entries,$m) [list [$m index end] $desc $label]
            }
            menu {
                lassign $item kind label sub
                set child $m.m[incr n]
                commands_fill $child $sub
                $m add cascade -label $label -menu $child
            }
            tk {
                lassign $item kind label script
                $m add command -label $label -command $script
            }
            instead {
                lassign $item kind label desc script
                $m add command -label $label -command $script
                lappend commands(instead) $desc
            }
            tiles {
                lassign $item kind label
                set child $m.tiles
                menu $child -tearoff 0
                angband_tiles_menu $child
                $m add cascade -label $label -menu $child
            }
        }
    }
}

# Help, at the top of the front end's Help menu.  It opens the Help window,
# which stands in for the game's help as the Options window does for its
# options -- and the game's ? opens the same window, through help_view_hook.
# Opened here directly rather than through the game, so it is never greyed:
# the manual can be read during a -more- or before there is a character.  The
# key shown is still the game's, and the entry is built with the others, and
# replaced when they are, so it follows the keyset.
proc commands_help {} {
    global commands

    set m .menubar.about
    if {![winfo exists $m]} return
    if {[info exists commands(helpshown)] && $commands(helpshown)} {
        $m delete 0
        set commands(helpshown) 0
    }
    if {![dict exists $commands(byname) $commands(help)]} return
    lassign [dict get $commands(byname) $commands(help)] gidx g idx name key
    $m insert 0 command -command help_window -label Help \
        -accelerator [commands_accel $key]
    lappend commands(instead) $commands(help)
    set commands(helpshown) 1
}

# The debug menu: one submenu per category, built but not shown.
#
# Its nine access points carry the readable names -- Items, Player, Teleport --
# and each names the group it opens, which is how "Player" is joined to
# "DbgPlayer".  Without that link the menu would have to know the internal
# names, and they are not the player's business.
proc commands_build_debug {} {
    global commands

    set m .menubar.cmddebug

    # Take the cascade out of the menubar before destroying what it points at,
    # or the entry is left referring to a window that no longer exists.
    if {[info exists commands(debugshown)] && $commands(debugshown)} {
        set i [commands_bar_index Debug]
        if {$i >= 0} { .menubar delete $i }
        set commands(debugshown) 0
    }
    if {[winfo exists $m]} { destroy $m }
    if {![info exists commands(nested,$commands(debug))]} return

    menu $m -tearoff 0

    foreach row $commands(nested,$commands(debug)) {
        lassign $row gidx group idx label key enabled level code opens

        if {$opens eq "" || ![info exists commands(nested,$opens)]} continue

        set sub $m.[string tolower $opens]
        menu $sub -tearoff 0

        foreach r $commands(nested,$opens) {
            lassign $r rgidx rgroup ridx rlabel rkey renabled rlevel rcode
            $sub add command -label $rlabel \
                -accelerator [commands_accel $rkey] \
                -command [list angband_command $rgidx $ridx]
        }

        $m add cascade -label $label -menu $sub
    }
}

# Shown only in wizard mode, and checked when the status line changes -- which
# is when the game notices the savefile has been marked.
proc commands_check_debug {} {
    global commands

    if {![winfo exists .menubar.cmddebug]} return

    # Either flag.  player->wizard is what ^W toggles and what the sidebar
    # shows, and it is what a player means by "wizard mode is on".
    # NOSCORE_DEBUG is what the debug commands' own prereq reads, and only ^A
    # sets it.  Keying the menu off the second alone meant it never appeared
    # for somebody who had just turned wizard mode on.
    if {[catch {angband_player wizard} wiz]} return
    if {[catch {angband_player debug} dbg]} { set dbg 0 }
    set on [expr {$wiz || $dbg}]

    set shown [expr {[info exists commands(debugshown)] && $commands(debugshown)}]
    if {$on == $shown} return

    if {$on} {
        .menubar insert [llength $commands(menus)] cascade -label Debug \
            -menu .menubar.cmddebug
    } else {
        set i [commands_bar_index Debug]
        if {$i >= 0} { .menubar delete $i }
    }
    set commands(debugshown) $on
}

# Grey out what cannot be done, just before the menu opens.
#
# A postcommand rather than an event handler: availability changes with almost
# everything the player does, and asking at the moment somebody looks is both
# always right and free the rest of the time.  The key is asked again too, in
# case the keyset has changed since the menu was built.
proc commands_refresh {m} {
    global commands

    if {![info exists commands(entries,$m)]} return
    if {[catch {angband_commands} rows]} return

    set by [dict create]
    foreach row $rows {
        if {[lindex $row 6] == 0} { dict set by [lindex $row 3] $row }
    }
    foreach entry $commands(entries,$m) {
        lassign $entry i desc label
        if {![dict exists $by $desc]} continue
        lassign [dict get $by $desc] gidx g idx name key enabled
        $m entryconfigure $i -state [expr {$enabled ? "normal" : "disabled"}] \
            -accelerator [commands_accel $key]
    }
}

# The menus are built once the game has a command table to build them from.
# cmd_init() runs inside textui_init(), which is after this file is sourced, so
# building at source time would produce a menu bar with no accelerators in it.
# do_cmd_wizard marks PR_TITLE and PR_STATUS, which player-calcs.c maps to
# EVENT_PLAYERTITLE and EVENT_STATUS.  Both are bound, and STATE with them: the
# check costs two accessor reads and missing it leaves the menu absent until
# something else happens to move the status line.
bind . <<Angband_STATUS>> {+ commands_check_debug }
bind . <<Angband_PLAYERTITLE>> {+ commands_check_debug }
bind . <<Angband_STATE>> {+ commands_check_debug }
bind . <<Angband_ENTER_GAME>> {+ commands_build }
bind . <<Angband_LEAVE_BIRTH>> {+ commands_build }
