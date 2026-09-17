# Drive the bridge from outside the game.
#
# Sourced by the front end when ZANGBAND_TCL_SCRIPT names it, once the window,
# the terms and the bridge commands all exist.  Reports to stdout and leaves a
# status behind for the runner; see scripts/run-tcl-bridge-test.
#
# Nothing here presses a key.  That is the point of T3: the front end asks the
# game questions and gives it commands, and a test can do the same.

set failures 0

proc check {what script expected} {
	global failures
	if {[catch {uplevel 1 $script} got]} {
		puts "FAIL $what: error: $got"
		incr failures
		return
	}
	if {$expected ne "" && $got ne $expected} {
		puts "FAIL $what: got '$got', wanted '$expected'"
		incr failures
		return
	}
	puts "ok   $what ($got)"
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

	finish
}

proc finish {} {
	global failures
	if {$failures} {
		puts "==> $failures check(s) failed."
	} else {
		puts "==> bridge checks passed."
	}
	set f [open $::env(ZANGBAND_TCL_RESULT) w]
	puts $f $failures
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
