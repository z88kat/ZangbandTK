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
check "every row has seven fields" {
	set bad 0
	foreach row $::cmds { if {[llength $row] != 7} { incr bad } }
	set bad
} 0

# The groups are the game's, not ours, so name one we know and count the rest.
check "the table names its groups" {
	set groups {}
	foreach row $::cmds {
		set g [lindex $row 0]
		if {$g ni $groups} { lappend groups $g }
	}
	expr {"Items" in $groups && "Action commands" in $groups}
} 1

# Descriptions are what a menu shows, so none may be blank.
check "every command has a description" {
	set bad 0
	foreach row $::cmds { if {[lindex $row 2] eq ""} { incr bad } }
	set bad
} 0

# The keys are not filled in yet: cmd_init runs in textui_init, which is after
# the front end starts.  Checked here so that the second pass below, once the
# event loop is turning, means something.
check "no keys before the game's UI is up" {
	set keyed 0
	foreach row $::cmds { if {[lindex $row 3] ne ""} { incr keyed } }
	set keyed
} 0

# Before a character exists nothing is allowed, and asking must not crash --
# the prereqs read the character and the level without checking either.
check "no command is enabled without a character" {
	set on 0
	foreach row $::cmds { if {[lindex $row 4]} { incr on } }
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

# --- the command queue ------------------------------------------------------

# Most table entries name a cmd_code; the rest are UI actions with a hook.
check "the table names its command codes" {
	set coded 0
	foreach row $::cmds { if {[lindex $row 6] ne ""} { incr coded } }
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

# --- the minimap pane and the status bar ------------------------------------

check "the minimap reports which map it is showing" {
	lassign [angband_minimap] mode ox oy blocks
	expr {$mode in {world level} && [string is integer -strict $ox]
			&& [string is integer -strict $oy]
			&& [string is integer -strict $blocks]}
} 1

check "panning without a world is harmless" {
	angband_minimap pan 3 4
	lindex [angband_minimap] 0
} "level"

check "the minimap refuses an instruction it does not have" {
	catch {angband_minimap sideways} e
	string match "expected*" $e
} 1

# Nothing to describe before there is a level, and asking must not reach into
# a NULL cave to find that out.
check "describing a cell without a game says nothing" {
	angband_describe 20 10
} ""

check "angband_describe wants two numbers" {
	catch {angband_describe 20} e
	string match "wrong # args*" $e
} 1

# --- the second pass --------------------------------------------------------

# Everything above ran while the front end was still starting.  This runs from
# the event loop, which the game only turns once it asks for input -- by which
# point textui_init has called cmd_init and filled the keymap.  It is the same
# table; if the keys are still missing here, the front end and the keyboard are
# not sharing one.
proc second_pass {} {
	check "the keys arrive once the game's UI is up" {
		set keyed 0
		foreach row [angband_commands] {
			if {[lindex $row 3] ne ""} { incr keyed }
		}
		expr {$keyed > 30}
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
