# Drive the bridge from outside the game.
#
# Sourced by the front end when ZANGBAND_TCL_SCRIPT names it, once the window,
# the terms and the bridge commands all exist.  Reports to stdout and leaves a
# status behind for the runner; see scripts/run-tcl-bridge-test.
#
# Nothing here presses a key.  That is the point of T3: the front end asks the
# game questions and gives it commands, and a test can do the same.

set failures 0
set log {}

# Everything is written to the result file rather than to stdout.
#
# Tk closes stdout when it is not a terminal -- the same redirection that hid
# the whole of the T0 launch failure -- so a run from CI, or from a shell that
# redirects, would print nothing at all about which check failed.  The file is
# the report; the runner prints it.
proc say {line} {
	lappend ::log $line
}

proc check {what script expected} {
	global failures
	if {[catch {uplevel 1 $script} got]} {
		say "FAIL $what: error: $got"
		incr failures
		return
	}
	if {$expected ne "" && $got ne $expected} {
		say "FAIL $what: got '$got', wanted '$expected'"
		incr failures
		return
	}
	say "ok   $what ($got)"
}

# --- the command table ------------------------------------------------------

set cmds [angband_commands]
check "the command table is not empty" {expr {[llength $::cmds] > 50}} 1
check "every row has nine fields" {
	set bad 0
	foreach row $::cmds { if {[llength $row] != 9} { incr bad } }
	set bad
} 0

# The debug groups are reached through access points that name them.  Without
# the link, a Debug menu would have to know the internal group names.
check "the debug access points name the groups they open" {
	set opens {}
	foreach row $::cmds {
		lassign $row gidx group idx label key enabled level code opened
		if {$group eq "Debug" && $opened ne ""} { lappend opens $opened }
	}
	set groups {}
	foreach row $::cmds { lappend groups [lindex $row 1] }
	set missing {}
	foreach o $opens { if {$o ni $groups} { lappend missing $o } }
	list [expr {[llength $opens] > 5}] $missing
} "1 {}"

# The groups are the game's, not ours, so name one we know and count the rest.
check "the table names its groups" {
	set groups {}
	foreach row $::cmds {
		set g [lindex $row 1]
		if {$g ni $groups} { lappend groups $g }
	}
	expr {"Items" in $groups && "Action commands" in $groups}
} 1

# Descriptions are what a menu shows, so none may be blank.
check "every command has a description" {
	set bad 0
	foreach row $::cmds { if {[lindex $row 3] eq ""} { incr bad } }
	set bad
} 0

# Most commands have a key even now.  cmd_init runs inside textui_init, which
# is after the front end starts, so cmd_lookup_key answers nothing yet -- but
# struct cmd_info carries the table's own key as well, and the accessor falls
# back to it.  That is what lets a menu built early still show accelerators;
# once cmd_init has run, a command with a code answers from the live keymap
# instead, so a player's rebinding shows through.
check "most commands carry a key even before the keymap is built" {
	set keyed 0
	foreach row $::cmds { if {[lindex $row 4] ne ""} { incr keyed } }
	expr {$keyed > 100}
} 1

# Before a character exists nothing is allowed, and asking must not crash --
# the prereqs read the character and the level without checking either.
check "no command is enabled without a character" {
	set on 0
	foreach row $::cmds { if {[lindex $row 5]} { incr on } }
	set on
} 0

check "angband_command refuses a bad group" {
	catch {angband_command 9999 0} e
	set e
} "no such command group"

check "angband_command refuses a bad index" {
	catch {angband_command 0 9999} e
	set e
} "no such command"

check "angband_command refuses without a character" {
	catch {angband_command 0 0} e
	set e
} "not allowed just now"

# The menu bar is generated from the table, so the two numbers in every row
# have to address the row they came from.  If they ever stop agreeing, every
# menu item runs the wrong command.
check "each row's group index names its group" {
	set groups {}
	foreach row $::cmds {
		lassign $row gidx group idx label key enabled level code
		if {[dict exists $groups $gidx] && [dict get $groups $gidx] ne $group} {
			return "group $gidx is both [dict get $groups $gidx] and $group"
		}
		dict set groups $gidx $group
	}
	expr {[dict size $groups] > 5}
} 1

# Both ENTER_GAME and LEAVE_BIRTH fire for a new character, so the menus are
# built twice and must survive it.  They did not: the rows accumulated and the
# second build tried to create every debug submenu again.
check "building the menus twice changes nothing" {
	commands_build
	set first [.menubar index end]
	set items [[commands_menu_path Items] index end]
	commands_build
	commands_build
	list [expr {[.menubar index end] == $first}] \
		[expr {[[commands_menu_path Items] index end] == $items}]
} "1 1"

# The Debug menu follows player->wizard, which is what ^W toggles and what the
# sidebar shows -- not NOSCORE_DEBUG, which only ^A sets.  Keying it off the
# second alone meant it never appeared for somebody who had just turned wizard
# mode on.
check "the debug menu follows the wizard flag" {
	rename angband_player _player_was
	proc angband_player {args} {
		if {[llength $args] == 1 && [lindex $args 0] eq "wizard"} { return 1 }
		if {[llength $args] == 1 && [lindex $args 0] eq "debug"} { return 0 }
		return [_player_was {*}$args]
	}
	commands_check_debug
	set on [expr {[commands_bar_index Debug] >= 0}]

	proc angband_player {args} {
		if {[llength $args] == 1 && [lindex $args 0] in {wizard debug}} {
			return 0
		}
		return [_player_was {*}$args]
	}
	commands_check_debug
	set off [expr {[commands_bar_index Debug] < 0}]

	rename angband_player {}
	rename _player_was angband_player
	list $on $off
} "1 1"

check "the hidden group is shown, under a readable name" {
	list [commands_group_label Hidden] [commands_group_label Items]
} "Other Items"

# The key goes in the label, never in -accelerator: AppKit renders a lowercase
# key equivalent as the capital, so "Throw an item", which answers to v, was
# advertised as V -- and V is Version info.
check "a command's key is shown exactly as it must be typed" {
	# Padded to a column, so compare what matters: the key, in its own case,
	# at the end, and nothing at all when there is no key.
	# Compared, not globbed: string match reads [v] as a character class.
	set a [commands_label "Throw an item" v 400]
	set b [commands_label "Version info" V 400]
	list [string range $a end-2 end] [string range $b end-2 end] \
		[commands_label "Look around" "" 400]
} {{[v]} {[V]} {Look around}}

check "a group name becomes a legal menu path" {
	list [commands_menu_path "Action commands"] [commands_menu_path Items]
} ".menubar.cmdactioncommands .menubar.cmditems"

# --- the command queue ------------------------------------------------------

# Most table entries name a cmd_code; the rest are UI actions with a hook.
check "the table names its command codes" {
	set coded 0
	foreach row $::cmds { if {[lindex $row 7] ne ""} { incr coded } }
	expr {$coded > 60}
} 1

check "angband_push refuses a command that does not exist" {
	catch {angband_push CMD_FLY} e
	set e
} "no such command: CMD_FLY"

check "angband_push refuses the sentinel" {
	catch {angband_push CMD_NULL} e
	set e
} "CMD_NULL is the absence of a command"

check "angband_push wants argument names in pairs" {
	catch {angband_push CMD_WALK direction} e
	string match "wrong # args*" $e
} 1

check "angband_push refuses an argument it cannot type" {
	catch {angband_push CMD_DROP item 3} e
	set e
} "item: unknown argument, or one that needs an object"

check "angband_push wants a grid as a pair" {
	catch {angband_push CMD_PATHFIND point 12} e
	set e
} "point: wanted a grid as {x y}"

# CMD_REPEAT has no entry in the game's own command table -- it is handled in
# the UI -- so the queue refuses it, and the refusal has to say so.
check "angband_push reports what the queue would not take" {
	catch {angband_push CMD_REPEAT} e
	string match "CMD_REPEAT was not queued:*" $e
} 1

# The positive case.  This only proves the command reached the queue: nothing
# drains CTX_GAME until there is a character, so that a pushed command is then
# carried out is T8's to show, once birth can be driven from here too.
check "a command with an argument reaches the queue" {
	angband_push CMD_PATHFIND point {12 34}
	set _ ok
} "ok"

# --- the events -------------------------------------------------------------

# Every event the game can send has to be bindable.  Generating them by hand
# proves the names are real Tk virtual event names and that a binding fires.
set seen {}
foreach name {MAP STATS HP MANA MESSAGE INPUT_FLUSH ENTER_WORLD} {
	bind . <<Angband_$name>> [list lappend ::seen $name]
	event generate . <<Angband_$name>>
}
check "bindings fire for the game's events" {llength $::seen} 7

# --- the panes --------------------------------------------------------------

check "the layout reported a cell size" {
	expr {$::angband(cellw) > 0 && $::angband(cellh) > 0}
} 1
check "the layout built five panes" {llength $::angband(terms)} 5

# --- the input hooks --------------------------------------------------------

check "the hooks are listed, and none is taken yet" {
	set taken 0
	foreach row [angband_hook] { if {[lindex $row 1]} { incr taken } }
	list [llength [angband_hook]] $taken
} "8 0"

check "an unknown hook is refused by name" {
	catch {angband_hook fly {}} e
	string match "no such hook: fly*" $e
} 1

check "a hook remembers its script" {
	angband_hook check {say yes}
	angband_hook check
} "say yes"

check "and gives it back" {
	angband_hook check {}
	angband_hook check
} ""

# --- the options ------------------------------------------------------------

# These belong to the character, so before there is one the answer is an error
# that says so rather than a default nobody asked for.
check "options need a character" {
	catch {angband_option use_old_target} e
	set e
} "the options belong to a character, and there is not one yet"

# --- the player ---------------------------------------------------------------

# The names are discoverable without a character, because a window is laid out
# before there is one to put in it.
check "the player's fields can be listed before there is a player" {
	set f [angband_player fields]
	expr {[llength $f] > 30 && "level" in $f && "hitpoints" in $f
			&& "armor_class" in $f}
} 1

check "and reading one says why it cannot" {
	catch {angband_player level} e
	set e
} "there is no character yet"

check "a field nobody has heard of is refused" {
	catch {angband_player wisdom} e
	string match "no such field: wisdom*" $e
} 1

check "angband_player takes at most one field" {
	catch {angband_player level gold} e
	string match "wrong # args*" $e
} 1

# --- the character window -------------------------------------------------

# The first window that is not a pane.  It can be opened before there is a
# character -- the menu is always there -- and it shows em-dashes rather than
# raising an error dialog at somebody who clicked it too early.
check "the character window opens without a character" {
	character_window
	list [winfo exists .character] [set ::character(v,name)]
} "1 —"

check "the character window has a page per entry" {
	set pages {}
	foreach page $::character(pages) {
		lassign $page key label numeral
		lappend pages [winfo exists .character.card.body.$key]
	}
	list [llength $::character(pages)] [lsort -unique $pages]
} "4 1"

check "the pages switch" {
	set ::character(page) notes
	character_switch
	set a [grid info .character.card.body.notes]
	set ::character(page) info
	character_switch
	list [expr {$a ne ""}] [expr {[grid info .character.card.body.notes] eq ""}]
} "1 1"

# The three per-character lists all refuse the same way before there is one.
check "virtues, mutations and the timeline want a character" {
	set said {}
	foreach cmd {angband_virtue angband_mutation angband_history} {
		catch {$cmd list} e
		lappend said $e
	}
	lsort -unique $said
} "{there is no character yet}"

check "it holds the stat rows its sections asked for" {
	set wanted 0
	foreach section $::character(sections) {
		lassign $section name title numeral rows
		incr wanted [expr {[llength $rows] / 2}]
	}
	expr {[llength $::character(fields)] == $wanted && $wanted > 10}
} 1

# The design system is what every window is drawn with, so a missing face is
# worth knowing about here rather than in a screenshot.
check "the three faces resolved" {
	list [font actual cl_title -family] [font actual cl_label -family] \
		[font actual cl_footer -family]
} "{Cormorant Garamond} Lora {Courier Prime}"

check "closing it is just closing it" {
	destroy .character
	character_refresh
	winfo exists .character
} 0

check "and it can be opened again" {
	character_window
	set was [winfo exists .character]
	destroy .character
	set was
} 1

# --- monster knowledge ------------------------------------------------------

# Everything here reads r_info and the lore, and the front end is answering
# before init_angband has read either, so it says so rather than indexing into
# a null array.
check "the monster list says when it is not loaded" {
	catch {angband_monster max} e
	set e
} "the monster list is not loaded yet"

check "and refuses a verb it does not have" {
	catch {angband_monster sideways} e
	set e
} "the monster list is not loaded yet"

check "the object list says when it is not loaded" {
	catch {angband_object max} e
	set e
} "the object list is not loaded yet"

check "the artifact list says when it is not loaded" {
	catch {angband_artifact max} e
	set e
} "the artifact list is not loaded yet"

# Every knowledge table answers the same four verbs and refuses the same way,
# which is what lets the browser be one generic pane per category.
check "every table takes the same four verbs" {
	set bad {}
	foreach cmd {angband_monster angband_object angband_artifact angband_ego
	             angband_rune angband_feature angband_trap} {
		foreach verb {max list info lore} {
			# Before a game they all refuse; what matters is that none of them
			# refuses with "expected max, list, info or lore".
			catch {$cmd $verb 1} e
			if {[string match "expected max*" $e]} { lappend bad "$cmd $verb" }
		}
		catch {$cmd sideways} e
		if {![string match "*not loaded yet*" $e]
				&& ![string match "expected max*" $e]} {
			lappend bad "$cmd sideways: $e"
		}
	}
	set bad
} ""

check "the knowledge window opens without a character" {
	knowledge_window
	list [winfo exists .knowledge] $::knowledge(count)
} "1 {0 known}"

check "it has a pane per category" {
	set panes {}
	foreach c $::knowledge(categories) {
		lassign $c key label accessor fields
		lappend panes [winfo exists .knowledge.card.body.$key]
	}
	list [llength $::knowledge(categories)] [lsort -unique $panes]
} "7 1"

check "the tabs switch which pane is shown" {
	set ::knowledge(tab) objects
	knowledge_switch
	set a [grid info .knowledge.card.body.objects]
	set ::knowledge(tab) monsters
	knowledge_switch
	list [expr {$a ne ""}] [expr {[grid info .knowledge.card.body.objects] eq ""}]
} "1 1"

# The search box has no placeholder in Tk, so the hint is a label behind it.
# Design-system machinery, and every window with a search box will use it.
check "an empty search box shows its hint" {
	set f .knowledge.card.body.monsters.left.esearch
	set was [winfo manager $f.hint]
	set ::knowledge(search,monsters) "kobold"
	update idletasks
	set now [winfo manager $f.hint]
	set ::knowledge(search,monsters) ""
	update idletasks
	list $was $now [winfo manager $f.hint]
} "place {} place"

check "closing it is just closing it" {
	destroy .knowledge
	winfo exists .knowledge
} 0

# --- the second pass --------------------------------------------------------

# Everything above ran while the front end was still starting.  This runs from
# the event loop, which the game only turns once it asks for input -- by which
# point textui_init has called cmd_init and filled the keymap.  It is the same
# table; if the keys are still missing here, the front end and the keyboard are
# not sharing one.
proc second_pass {} {
	check "the keys are still there once the game's UI is up" {
		set keyed 0
		foreach row [angband_commands] {
			if {[lindex $row 4] ne ""} { incr keyed }
		}
		expr {$keyed > 100}
	} 1

	# Events reaching us from the game itself, rather than ones we generated.
	check "the game signalled its own events" {
		expr {"ENTER_INIT" in $::seen && "INITSTATUS" in $::seen}
	} 1

	# The round trip.  angband_ask goes through game-input.c's get_check and
	# friends, which is to say through whatever hook is installed -- so this
	# only answers from Tcl if the override really did replace textui's.  The
	# hooks cannot be installed before this point: there would be nothing to
	# save as the original.
	angband_hook check {apply {{prompt} {list [string match "yes*" $prompt]}}}
	check "a taken-over prompt is answered from Tcl" {
		list [angband_ask check "yes please? "] [angband_ask check "no? "]
	} "1 0"

	angband_hook quantity {apply {{prompt max} {list [expr {$max - 1}]}}}
	check "and the answer reaches the game as the right type" {
		angband_ask quantity "how many? " 40
	} 39

	check "an answer outside the range is clamped, not trusted" {
		angband_hook quantity {apply {{prompt max} {list 9999}}}
		angband_ask quantity "how many? " 40
	} 40

	check "the empty list is a cancelled prompt" {
		angband_hook string {apply {{prompt current} {list}}}
		angband_ask string "name? " "Corwin"
	} ""

	check "and a list with a value in it is an answer" {
		angband_hook string {apply {{prompt current} {list "Random"}}}
		angband_ask string "name? " "Corwin"
	} "Random"

	check "a grid comes back as a grid" {
		angband_hook point {apply {{} {list {12 34}}}}
		angband_ask point
	} "12 34"

	# A dialog with a bug in it must not be able to wedge the game at a
	# prompt, so a script that fails hands the question back to textui.  There
	# is no keyboard here, so this only checks that it is reported and does not
	# return the script's non-answer.
	check "a failing script does not answer for the game" {
		angband_hook confirm_debug {apply {{} {error "deliberate"}}}
		angband_ask confirm_debug
	} 0

	# The options, now that there is a character to own them.
	check "the options are the game's own list" {
		set names {}
		foreach row [angband_option] { lappend names [lindex $row 0] }
		expr {[llength $names] > 50
				&& "use_old_target" in $names
				&& "hitpoint_warn" in $names
				&& "none" ni $names}
	} 1

	check "an option can be read, written and read back" {
		angband_option use_old_target 1
		angband_option use_old_target
	} 1

	check "a numeric setting is refused outside its range" {
		catch {angband_option hitpoint_warn 12} e
		set e
	} "hitpoint_warn runs from 0 to 9"

	check "an option nobody has heard of is refused" {
		catch {angband_option nonesuch} e
		set e
	} "no such option: nonesuch"

	foreach row [angband_hook] { angband_hook [lindex $row 0] {} }
	check "every hook can be given back" {
		set taken 0
		foreach row [angband_hook] { if {[lindex $row 1]} { incr taken } }
		set taken
	} 0

	finish
}

proc finish {} {
	global failures log
	set f [open $::env(ZANGBAND_TCL_RESULT) w]
	puts $f $failures
	foreach line $log { puts $f $line }
	close $f
	exit [expr {$failures ? 1 : 0}]
}

foreach name {ENTER_INIT INITSTATUS} {
	bind . <<Angband_$name>> [list lappend ::seen $name]
}

# Two seconds is generous: the data files load in well under one.  A session
# that never gets here has hung, and leaves no result behind -- which is what
# the runner reports rather than passing by default.
after 2000 second_pass
