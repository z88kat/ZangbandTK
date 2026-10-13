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
	set rows [[commands_menu_path Inventory] index end]
	set help [.menubar.about index end]
	commands_build
	commands_build
	list [expr {[.menubar index end] == $first}] \
		[expr {[[commands_menu_path Inventory] index end] == $rows}] \
		[expr {[.menubar.about index end] == $help}]
} "1 1 1"

# The menus are the original's layout, and every one of the game's commands is
# somewhere in them -- a command added to the game and given no place fails
# here.  The layout may name one the game lacks only if it is the Borg's, which
# a build can leave out.
check "the menu bar is the original's, and every command has a place" {
	commands_build
	set bar {}
	for {set i 0} {$i <= [.menubar index end]} {incr i} {
		if {[.menubar type $i] eq "cascade"} {
			lappend bar [.menubar entrycget $i -label]
		}
	}
	set placed $commands(instead)
	foreach {name entries} [array get commands entries,*] {
		foreach e $entries { lappend placed [lindex $e 1] }
	}
	set missing {}
	foreach row [angband_commands] {
		lassign $row gidx group idx label key enabled level
		if {$level == 0 && $label ni $placed} { lappend missing $label }
	}
	set extra [lsearch -all -inline -not -exact $commands(absent) "Borg commands"]
	list $bar $missing $extra
} "{File Inventory Action Other Window Help} {} {}"

check "the entries carry their own short names and the game's keys" {
	set m [commands_menu_path Inventory]
	set magic [$m entrycget [$m index Magic] -menu]
	set o [commands_menu_path Other]
	set prefs [$o entrycget [$o index Preferences] -menu]
	list [$m entrycget 0 -label] [$m entrycget 0 -accelerator] \
		[$magic entrycget 2 -label] [$magic entrycget 2 -accelerator] \
		[$prefs entrycget 0 -label] [$prefs entrycget 0 -command] \
		[$prefs entrycget 0 -accelerator] \
		[expr {"Interact with options" in $commands(instead)}]
} "Equipment e {Drink Potion} q Options options_window {} 1"

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

# AppKit draws a key equivalent as a key, so v is shown as V -- and V is
# Version info, not Throw.  On the Mac an uppercase key has to be asked for as
# Shift-V, which is drawn as ⇧V, and ^S as Control-s; anywhere else the
# accelerator is drawn as given, and the game's own spelling is the one to give.
check "a command's key is shown exactly as it must be typed" {
	set got {}
	foreach k {v V ^S | Tab ""} { lappend got [commands_accel $k] }
	set got
} [expr {[tk windowingsystem] eq "aqua"
	? {v Shift-V Control-s | Tab {}} : {v V ^S | Tab {}}}]

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
# Five panes, four of them terms: the fifth is the Choice pane, a list rather
# than a term, so that it can be clicked.
check "the layout built five panes, four of them terms" {
	list [llength $::angband(terms)] \
		[expr {[llength [.pw.top panes]] + [llength [.pw.bottom panes]]}] \
		[winfo class .pw.bottom.choice.t]
} "4 5 Text"

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

# Decision 18: the windows are Tk's own widgets in Tk's classic theme, and
# nothing drawn by hand.  A widget class outside this list is a widget somebody
# invented, which is the thing the decision rules out.  Returns the theme and
# whatever did not belong.
proc stock_widgets_only {top} {
	set stock {Toplevel Menu TFrame TLabel TLabelframe TRadiobutton TEntry
	           TMenubutton TButton TScrollbar TSeparator TProgressbar
	           Treeview TSpinbox Listbox Text Canvas Label TCheckbutton
	           TNotebook}
	set odd {}
	set todo $top
	while {[llength $todo]} {
		set todo [lassign $todo w]
		if {[winfo class $w] ni $stock} { lappend odd "$w [winfo class $w]" }
		lappend todo {*}[winfo children $w]
	}
	list [ttk::style theme use] $odd
}

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
		lappend pages [winfo exists .character.f.page.[lindex $page 0]]
	}
	list [llength $::character(pages)] [lsort -unique $pages]
} "5 1"

check "the pages switch" {
	set ::character(page) notes
	character_switch
	set a [grid info .character.f.page.notes]
	set ::character(page) info
	character_switch
	list [expr {$a ne ""}] [expr {[grid info .character.f.page.notes] eq ""}]
} "1 1"

check "it is built from stock Tk widgets in the classic theme" {
	stock_widgets_only .character
} "classic {}"

# The two things the sheet can do to the character, and both want one.
check "renaming and dumping want a character" {
	set said {}
	foreach cmd {{angband_rename Bob} {angband_dump suggest}
	             {angband_dump write /tmp/never.txt}} {
		catch $cmd e
		lappend said $e
	}
	lsort -unique $said
} "{there is no character yet}"

check "the place is a field, for the sheet's Depth" {
	expr {"place" in [angband_player fields]}
} 1

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
		lassign $section name title rows
		incr wanted [expr {[llength $rows] / 2}]
	}
	expr {[llength $::character(fields)] == $wanted && $wanted > 10}
} 1

# Decision 18 retired the Classical design system with its last window.  Its
# namespace and its named fonts coming back would mean somebody had brought it
# back.
check "the design system is gone, fonts and all" {
	list [namespace exists ::classical] [lsearch -all -inline [font names] cl_*]
} "0 {}"

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

check "it has a radio button per category" {
	set radios {}
	foreach c $::knowledge(categories) {
		lappend radios [winfo class .knowledge.f.cat.[lindex $c 0]]
	}
	list [llength $::knowledge(categories)] [lsort -unique $radios]
} "7 TRadiobutton"

check "choosing a category changes the list and its facts" {
	set ::knowledge(cat) objects
	knowledge_switch
	set a [list [.knowledge.f.body.list cget -text] \
		[winfo exists .knowledge.f.body.detail.facts.vweight]]
	set ::knowledge(cat) monsters
	knowledge_switch
	lappend a [winfo exists .knowledge.f.body.detail.facts.vweight]
} "{Objects — 0 known} 1 0"

# Decision 18, as for the character window.
check "it is built from stock Tk widgets in the classic theme" {
	stock_widgets_only .knowledge
} "classic {}"

check "closing it is just closing it" {
	destroy .knowledge
	winfo exists .knowledge
} 0

# --- the items window ------------------------------------------------------

# Like the other two, it opens before there is a character and says nothing
# rather than "Your pack is empty", which would be a different claim.
check "the items window opens without a character" {
	items_window
	list [winfo exists .items] [.items.f.body.inv.list.t get 1.0 end-1c]
} "1 {}"

check "it is built from stock Tk widgets in the classic theme" {
	stock_widgets_only .items
} "classic {}"

# Every button and menu entry that acts on an item is off until one is chosen.
check "with nothing chosen, nothing can be done" {
	set off {}
	foreach b {wear takeoff drop inspect} {
		lappend off [.items.f.btns.$b instate disabled]
	}
	list [items_can] [lsort -unique $off]
} "{0 0 0 0 0} 1"

check "closing it is just closing it" {
	destroy .items
	items_refresh
	winfo exists .items
} 0

# --- the spell book ---------------------------------------------------------

check "the spell book wants a character" {
	catch {angband_book books} e
	set e
} "there is no character yet"

check "the spell book window opens without a character" {
	book_window
	list [winfo exists .book] [llength [.book.f.body.tv children {}]] \
		[stock_widgets_only .book]
} "1 0 {classic {}}"

check "and closes" {
	destroy .book
	book_refresh
	winfo exists .book
} 0

# --- the store --------------------------------------------------------------

check "the store wants a character" {
	catch {angband_store} e
	set e
} "there is no character yet"

# --- the town's buildings ---------------------------------------------------

check "a building wants a character" {
	catch {angband_service} e
	set e
} "there is no character yet"

# Opened by a building's first question, but it can be built at any time, and
# it is Tk's own widgets like the other windows.
check "the building window is stock Tk" {
	building_window
	list [winfo exists .building] [stock_widgets_only .building]
} "1 {classic {}}"

check "and closes" {
	building_close
	winfo exists .building
} 0

check "with nothing being asked there is no building window" {
	building_check
	winfo exists .building
} 0

# The game lays a menu line out in columns with spaces; the window splits it
# where there are two or more, and a last column in gold is the price.
check "a building's menu line splits into its columns" {
	list [building_row menu {0 a {Bind your wounds              120 au} 1}] \
		[building_row menu {3 d {The Grove of the Unicorn  dungeon       1755 au} 1}] \
		[building_row menu {4 q {Nothing today} 1}]
} "{0 a {Bind your wounds} 120 1} {3 d {The Grove of the Unicorn (dungeon)} 1755 1}\
 {4 q {Nothing today} {} 1}"

# --- the gear ---------------------------------------------------------------

check "the Choice pane's command wants a character" {
	catch {angband_choice} e
	set e
} "there is no character yet"

check "the gear wants a character" {
	catch {angband_gear inventory} e
	set e
} "there is no character yet"

check "and refuses a view it does not have" {
	catch {angband_gear sideways} e
	set e
} "there is no character yet"

# --- the debug console and the error sink -----------------------------------

# T3's exit criterion is that the debug loop exists before the UI does, so
# these run in the first pass, before there is a character.  That is also where
# they are worth most: the errors this front end has actually lost were all
# raised during startup, which is before anyone has opened a window.

check "the background error handler is the sink" {
	interp bgerror {}
} "::errorsink::background"

# The sink has to work with nothing on screen, because an error at startup
# happens long before a person thinks to open a window.  The records are the
# thing; the window is a view of them.
check "the sink records with no window open" {
	errorsink::clear
	errorsink::note "a line"
	list [winfo exists .errors] [errorsink::text]
} "0 {a line}"

# The one this was built for: a binding that throws is reported instead of
# vanishing.  Before the sink there was nowhere for it to go -- Tk's default
# handler puts up a modal dialog, which is unusable for something that fails
# once per redraw, and the plog in main-tcl.c goes to the stderr Tk redirects
# to /dev/null for a bundled application.
#
# Three details here are not incidental, and each was a wrong first attempt:
#
#   - The binding goes on ".", because that is what main-tcl.c generates the
#     events on.  Bound to a frame that is never packed, the binding does not
#     fire at all: Tk does not deliver a virtual event to an unmapped widget,
#     so the first version of this check passed the error nowhere and then
#     reported the empty sink as the failure.
#   - `-when tail`, because that is what main-tcl.c uses, and it is the whole
#     difference.  Generated `-when now` the script runs inside this one's call
#     stack and its error is an ordinary error, not a background one.
#   - BELL, because nothing else binds it and the game does not signal it
#     during startup, so a deliberately broken script cannot leak into the
#     checks that follow.  The binding is removed either way.
#   - The `update` before generating, because a queued virtual event aimed at a
#     window that is not realised yet is discarded rather than delivered.  The
#     front end has already done this by the time a script runs -- main.tcl
#     updates before measuring, and main-tcl.c updates again before returning
#     -- so this one is belt and braces, and costs nothing.
check "a deliberately broken script's error reaches the sink" {
	errorsink::clear
	bind . <<Angband_BELL>> { error "deliberately broken" }
	update
	event generate . <<Angband_BELL>> -when tail
	update
	bind . <<Angband_BELL>> {}
	list [errorsink::count] \
		[string match "*deliberately broken*" [errorsink::text]]
} "1 1"

# A message alone names the symptom; the trace names the line.  The 2001 window
# captured errorInfo and this would be a regression without it.
check "and brings its stack trace with it" {
	string match "*while executing*" [errorsink::text]
} 1

# The console, which is the half that answers questions rather than reporting
# them: "what does this accessor actually return" asked against the running
# game.  `fields` is used because it is the one player call that works before
# there is a character.
check "the console evaluates an accessor and returns what it returns" {
	expr {[console::eval {angband_player fields}] eq [angband_player fields]}
} 1

check "the console evaluates at the global level" {
	console::eval {llength $angband(terms)}
} 4

# An error at the console is text, not an exception -- the caller is a typist
# -- and it goes to the sink as well, so a mistake made while debugging is
# recorded next to the bug being chased.
check "a console error is returned rather than raised, and is recorded" {
	errorsink::clear
	set r [console::eval {angband_player wisdom}]
	list [console::failed] [errorsink::count] \
		[string match "no such field: wisdom*" $r]
} "1 1 1"

check "and a good result is not an error" {
	console::eval {expr {6 * 7}}
	list [console::failed] $::console::P(result)
} "0 42"

# `info complete` decides when a command has finished, so a pasted multi-line
# proc accumulates instead of failing on its first line.
check "an unfinished command is held rather than run" {
	info complete "proc p {} \{"
} 0

errorsink::clear

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

	# The three views of the gear, now that there is a character.
	# Answering at all is the check.  How much is in them depends on the
	# character, and at this point in the session there is barely one -- the
	# body is not assigned until birth, so equipment can legitimately be
	# empty.  The filled case is exercised against a real savefile instead.
	check "the pack, what is worn and the quiver all answer" {
		set bad {}
		foreach view {inventory equipment quiver} {
			if {[catch {angband_gear $view} e]} { lappend bad "$view: $e" }
		}
		set bad
	} ""

	# Every equipment slot is a row whether or not it holds anything: the
	# paper doll and the wear prompt both have to show what is empty.
	check "every equipment slot is a row, filled or not" {
		set named 0
		foreach row [angband_gear equipment] {
			if {[lindex $row 3] ne ""} { incr named }
		}
		expr {$named == [llength [angband_gear equipment]]}
	} 1

	# Every equipment row carries the slot's type as the game names it, which
	# is what the paper doll matches on.  The body parts are the game's
	# ("right hand"); the types are EQUIP_RING and the rest, lower-cased.
	check "an equipment row names its slot type" {
		set types {}
		foreach row [angband_gear equipment] {
			if {[lindex $row 7] ni $types} { lappend types [lindex $row 7] }
		}
		expr {"weapon" in $types || ![llength [angband_gear equipment]]}
	} 1

	# Three numbers, in tenths of a pound: what is carried, where speed starts
	# to go, and what cannot be exceeded.  The middle one is half the last,
	# which is the rule in calc_bonuses().
	check "burden comes back as three weights" {
		lassign [angband_gear burden] carried slow capacity
		list [expr {$carried >= 0}] [expr {$slow * 2 == $capacity}] \
			[expr {$capacity > 0}]
	} "1 1 1"

	# A row's tenth field is its symbol's colour, in the map's own form, or
	# empty for an empty slot.
	check "a gear row carries its symbol's colour" {
		set bad {}
		foreach view {inventory equipment} {
			foreach row [angband_gear $view] {
				set colour [lindex $row 9]
				if {[llength $row] != 10} { lappend bad "$view: [llength $row]" }
				if {$colour ne "" && ![regexp {^#[0-9a-f]{6}$} $colour]} {
					lappend bad "$view: $colour"
				}
			}
		}
		set bad
	} ""

	# Between prompts the Choice pane shows the pack, and has nothing to
	# answer: a pick is refused rather than kept for a prompt to come.
	check "the Choice pane is idle between prompts" {
		set rows [angband_choice list]
		set eight 1
		foreach row $rows { if {[llength $row] != 8} { set eight 0 } }
		catch {angband_choice pick 0} e
		list [angband_choice] \
			[expr {[llength $rows] == [llength [angband_gear inventory]]}] \
			$eight $e
	} "{0 {} {}} 1 1 {the game is not asking for anything}"

	# An item's menu is the game's item menu: Inspect first, always, with the
	# key the game gives it; it is the window's to show, never a command.
	check "an item's menu starts with Inspect, which is not a command" {
		if {![llength [angband_gear inventory]]} {
			catch {angband_choice actions 0} e
			list skip $e
		} else {
			set first [lindex [angband_choice actions 0] 0]
			catch {angband_choice act 0 -1} e
			list $first $e
		}
	} [expr {[llength [angband_gear inventory]]
		? "{-1 Inspect I 1} {that cannot be done with it just now}"
		: "skip {nothing at inventory 0}"}]

	check "and an item that is not there has no menu" {
		catch {angband_choice actions 999} e
		set e
	} "nothing at inventory 999"

	# Pointing at an item in the pack puts it in the Recall pane, by tracking
	# it as the game's own item menu does; a row with nothing in it is refused.
	check "an item pointed at is the one the Recall pane describes" {
		catch {angband_choice recall 999} e
		list [expr {[llength [angband_gear inventory]]
			? [angband_choice recall 0] : "empty"}] $e
	} [list [expr {[llength [angband_gear inventory]] ? "" : "empty"}] \
		"no item at row 999"]

	# Not in a store, so the store is empty and its lists are refused by name;
	# and no Store window, which opens only on walking into one.
	check "outside a store there is no store" {
		catch {angband_store stock} e
		list [angband_store] $e [winfo exists .store]
	} "{} {you are not in a store} 0"

	# Nor in a building: a visit is what ENTER_SERVICE and LEAVE_SERVICE
	# bracket, and none is under way.
	check "outside a building there is no visit" {
		list [angband_service] [winfo exists .building]
	} "{} 0"

	# The Progress strip: four meters, each {name value current maximum
	# colour}, in the colours the game's own display gives them.
	check "the Progress strip's meters are the game's" {
		set shape {}
		foreach row [angband_player meters] {
			lappend shape [lindex $row 0] [llength $row] \
				[string match "#??????" [lindex $row 4]] \
				[expr {[lindex $row 5] in {good fair poor {}}}]
		}
		set shape
	} "hp 6 1 1 sp 6 1 1 food 6 1 1 exp 6 1 1"

	check "and the strip is Tk's own widgets" {
		progress_refresh
		list [string is integer -strict [[progress_frame].vhp cget -text]] \
			[stock_widgets_only [progress_frame]]
	} "1 {classic {}}"

	# DEC-136: a character made now sells for gold, as in Zangband.
	check "a new character is born able to sell" {
		angband_option birth_no_selling
	} 0

	check "the pack's slots come back as used and size" {
		lassign [angband_gear slots] used size
		list [expr {$used >= 0}] [expr {$size > 0 && $used <= $size}]
	} "1 1"

	check "an item that is not there is refused" {
		catch {angband_gear info inventory 999} e
		set e
	} "nothing at inventory 999"

	check "and is not described in the Recall pane either" {
		catch {angband_gear recall inventory 999} e
		set e
	} "nothing at inventory 999"

	check "nothing fits nowhere" {
		catch {angband_gear fits inventory 999} e
		set e
	} ""

	check "and a word that is not a subcommand is refused by name" {
		catch {angband_gear sideways} e
		set e
	} "expected inventory, equipment, quiver, slots, burden, info, lore,\
 recall, fits, inscribe, wield, takeoff or drop, not: sideways"

	check "and a view that does not exist is refused by name" {
		catch {angband_gear info pockets 0} e
		set e
	} "expected inventory, equipment or quiver, not: pockets"

	foreach row [angband_hook] { angband_hook [lindex $row 0] {} }
	check "every hook can be given back" {
		set taken 0
		foreach row [angband_hook] { if {[lindex $row 1]} { incr taken } }
		set taken
	} 0

	# Character creation (T8).  Nothing is asked until the splash is past and
	# a new character has been chosen, and until then there is none at all.
	check "nothing is asked about a character before creation begins" {
		catch {angband_birth answer back} e
		list [angband_birth] [winfo exists .birth] $e
	} "{} 0 {there is no character yet}"

	birth_pass
}

# --- the third pass: making a character -------------------------------------

# Past the splash, the Choose Game window and the Birth window -- the first
# point at which this session has a character the game is really playing.  The
# runner gives the game a save directory of its own, so this is always a new
# character.
proc birth_pass {} {
	set ::birthseen {}
	set ::birthstep ""
	set ::chooses 0
	bind . <<Angband_CHOOSEGAME>> {+
		after 300 [list choose_pass [incr ::chooses]]
	}
	bind . <<Angband_BIRTHVIEW>> {+
		after cancel $::birthstep
		set ::birthstep [after 300 birth_pass_step]
	}
	bind . <<Angband_ENTER_GAME>> {+ after 300 birth_pass_done }
	set ::birthwatch [after 30000 {
		check "the character was made in the Birth window" {
			list timed out at $::birthseen
		} "made"
		finish
	}]
	if {[llength [angband_savefile]]} {
		# Already choosing: the key that follows angband_push in the first
		# pass is the one the splash took.
		after 300 [list choose_pass [incr ::chooses]]
	} else {
		angband_key 0 0 " "
	}
}

# The Choose Game window: at the start, with nothing in the save directory
# but a new character to make, and again after New Game, where the character
# just finished is among the dead and a new one is still what is offered.
proc choose_pass {n} {
	if {$n == 1} {
		check "the game starts at the Choose Game window, offering a new character" {
			list [angband_savefile] \
				[lmap row [angband_savefile list] {lindex $row 3}] \
				[.choose.f.btns.new cget -default] \
				[.choose.f.btns.open instate disabled] \
				[stock_widgets_only .choose]
		} "{0 0} new active 1 {classic {}}"
		choose_new
		return
	}

	after cancel $::deathwatch
	check "New Game goes back to the Choose Game window, the character dead" {
		set row [lsearch -inline -index 1 [angband_savefile list] Bridge]
		list [lindex $row 3] [lindex $row 9] \
			[.choose.f.list.tv selection] [.choose.f.btns.new cget -default]
	} "dead Retiring {} active"
	finish
}

proc birth_pass_step {} {
	lassign [angband_birth] stage
	if {$stage eq ""} return
	lappend ::birthseen $stage

	switch -- $stage {
		race {
			check "the Birth window asks for a race, from the game's list" {
				lassign [angband_birth choices] cursor rows
				list [winfo exists .birth] [expr {[llength $rows] > 10}] \
					[lindex $rows 0 1] [stock_widgets_only .birth]
			} "1 1 Human {classic {}}"

			check "and describes it in the game's words" {
				string match "*Hit die:*XP mod:*" [angband_birth describe 0]
			} 1

			# And in words, from each data file's description: every race,
			# class and realm has one.
			check "and in prose, every race, class and realm of them" {
				set bare {}
				foreach kind {race class realm} {
					foreach name [angband_birth names $kind] {
						if {[angband_birth prose $kind $name] eq ""} {
							lappend bare "$kind $name"
						}
					}
				}
				list [string match "The human is*" [angband_birth prose race Human]] \
					[llength [angband_birth names class]] $bare
			} "1 14 {}"

			# Beside the steps, the character as its portrait will show it:
			# while a race is being chosen, the one under the cursor.
			check "the Birth window shows the race under the cursor" {
				update
				set tv .birth.f.menu.list.tv
				$tv selection set [lindex [$tv children {}] 0]
				update
				set one [birth_likeness data]
				$tv selection set [lindex [$tv children {}] 3]
				update
				set other [birth_likeness data]
				list [winfo ismapped .birth.f.head.art] \
					[image width birth_likeness] [expr {$one ne $other}]
			} "1 64 1"

			# The rest at random, as '@' does on the text screen.
			birth_answer finish
		}
		confirm {
			# What the Birth window shows at the end is what the windows will
			# show in play: the same race, class and sex, from the same rolls.
			check "the portrait at birth is the one the character will have" {
				image create photo probe_birth
				portrait_put probe_birth 64
				set same [expr {[probe_birth data] eq [birth_likeness data]}]
				image delete probe_birth
				set same
			} 1

			check "the character waits to be confirmed" {
				set c [angband_birth character]
				list [expr {[dict get $c name] ne ""}] \
					[expr {[dict get $c race] ne ""}] \
					[expr {[dict get $c hitpoints] > 0}]
			} "1 1 1"
			# Return, on the button that has the focus -- which did nothing
			# until the window pressed it.  The pass ends only if it works.
			event generate .birth.f.btns.next <Return>
		}
		default {
			# A stage the random finish should have answered; take it
			# forward so that the pass still ends.
			birth_next
		}
	}
}

proc birth_pass_done {} {
	after cancel $::birthwatch

	# The race's checks run only if the race is the first thing seen, so
	# say so when it was not, rather than passing without them.  Seen once,
	# not reproduced: the pass went straight to the confirmation.
	check "the Birth window was seen asking for the race first" {
		lindex $::birthseen 0
	} "race"
	check "the character was made in the Birth window" {
		list [winfo exists .birth] [expr {[angband_player race] ne ""}] \
			[expr {[lindex [angband_player hitpoints] 1] > 0}]
	} "0 1 1"

	# The spell book's view of the character: the books carried that can be
	# read, and a refusal by name for a pack position that is not one.
	check "the spell book lists books and refuses what is not one" {
		set books [angband_book books]
		set shapes {}
		foreach row $books { lappend shapes [llength $row] }
		catch {angband_book spells 999} e
		list [expr {[lsort -unique $shapes] in {{} 3}}] \
			[string is boolean -strict [angband_book chooses]] $e
	} "1 1 {no book at inventory 999}"

	# The options, now that the character exists: a birth option is the
	# character's, which option_set() alone does not enforce, and each row
	# says whether it can be changed so a window can grey it.
	check "a birth option is fixed once the character exists" {
		catch {angband_option birth_no_selling 1} e
		set row [lsearch -inline -index 0 [angband_option] birth_no_selling]
		list $e [lindex $row 4] \
			[lindex [lsearch -inline -index 0 [angband_option] use_old_target] 4]
	} "{birth options are fixed once a character is made} 0 1"

	check "the Options window is Tk's own widgets, birth options greyed" {
		options_window
		set dis 0
		foreach w [winfo children .options.f.nb.birth.scroll.c.in] {
			if {[$w instate disabled]} { incr dis }
		}
		set r [list [stock_widgets_only .options] [expr {$dis > 10}]]
		destroy .options
		set r
	} "{classic {}} 1"

	# The windows that show the character (OBS-46) show its portrait, which
	# is the same whether the map is drawn in tiles or in letters.  The map
	# tile is still there to fall back on, and in letters there is none.
	check "the Character window shows the portrait, in tiles and in letters" {
		image create photo probe_tile
		set ok [angband_tile player probe_tile 64]
		set big [expr {max([image width probe_tile], [image height probe_tile])}]
		character_window
		update
		set shown [.character.f.top.id.glyph itemcget tile -state]
		set was [angband_tileset]
		set text [expr {[catch {angband_tileset 0}] ? "?"
			: [angband_tile player probe_tile 64]}]
		event generate . <<TilesChanged>>
		update
		set still [.character.f.top.id.glyph itemcget tile -state]
		set letter [.character.f.top.id.glyph itemcget letter -state]
		angband_tileset $was
		event generate . <<TilesChanged>>
		update
		destroy .character
		image delete probe_tile
		list $ok $big $shown $text $still $letter \
			[image width character_tile]
	} "1 64 normal 0 normal hidden 64"

	# 784 portraits, made by scripts/portraits/build-portraits.py: one for
	# every race and class the game has, as a man and as a woman, each with a
	# figure in it -- a cell left empty would be a blank square in a window.
	check "every race and class the game has, has a portrait" {
		portrait_load
		set races [angband_birth names race]
		set classes [angband_birth names class]
		set missing {}
		foreach r $races {
			if {$r ni $::portrait(races)} { lappend missing $r }
		}
		foreach c $classes {
			if {$c ni $::portrait(classes)} { lappend missing $c }
		}
		set empty 0
		set s $::portrait(size)
		for {set row 0} {$row < 2 * [llength $::portrait(classes)]} {incr row} {
			for {set col 0} {$col < [llength $::portrait(races)]} {incr col} {
				if {[portrait_sheet transparency get \
						[expr {$col * $s + $s / 2}] [expr {$row * $s + $s - 4}]]} {
					incr empty
				}
			}
		}
		list [llength $races] [llength $classes] $missing $empty
	} "28 14 {} 0"

	# The game has no sex for a character; the portrait takes one from what
	# birth rolled, so it is the same every time.  On its class's backdrop in a
	# window, and on nothing on the tombstone.
	check "a portrait is the same every time, on its backdrop or on nothing" {
		image create photo probe_portrait
		set sex [portrait_sex]
		set drawn [portrait_put probe_portrait 64]
		set size [list [image width probe_portrait] [image height probe_portrait]]
		set row [lsearch -exact $::portrait(classes) [angband_player class]]
		set ground [portrait_sheet get [expr {$row * 32}] \
			[expr {2 * [llength $::portrait(classes)] * 32}]]
		set corner [probe_portrait get 0 0]
		portrait_put probe_portrait 64 0
		set bare [probe_portrait transparency get 0 0]
		image delete probe_portrait
		list [expr {$sex in {M F}}] [expr {[portrait_sex] eq $sex}] $drawn \
			$size [expr {$corner eq $ground}] $bare
	} "1 1 1 {64 64} 1 1"

	# The Hall of Fame is the game's table, and the character being played is
	# in it where its score would put it, as the game's own prediction does.
	check "the Hall of Fame places the character being played" {
		set mine {}
		foreach row [angband_scores] {
			if {[dict get $row you]} { set mine $row }
		}
		fame_window
		update
		set r [list [dict get $mine how] \
			[expr {[dict get $mine who] eq [angband_player name]}] \
			[expr {[llength [.fame.f.list.t tag ranges you]] > 0}] \
			[stock_widgets_only .fame]]
		destroy .fame
		set r
	} "{nobody (yet!)} 1 1 {classic {}}"

	# A notch of the wheel is a few rows.  Tk 9 reports one as 120, and the
	# page used to scroll by that many units -- to the end, at once.
	check "a notch of the wheel scrolls the Options page a little" {
		options_window interface
		update
		set c .options.f.nb.interface.scroll.c
		set before [lindex [$c yview] 0]
		event generate $c <MouseWheel> -delta -120
		update
		set after [lindex [$c yview] 0]
		destroy .options
		list $before [expr {$after > 0 && $after < 0.5}]
	} "0.0 1"

	# The underlined letters.  On the Mac, Option and a letter arrives as the
	# character it types -- Option-B is "∫" -- so the shortcut is read back from
	# the key: Carbon's code for B is 11, in the top byte of Tk's key code.
	# Elsewhere it is Alt, bound as it always was.
	check "an underlined letter chooses its tab, but not in a text field" {
		options_window interface
		update
		if {[tk windowingsystem] eq "aqua"} {
			set code [expr {(11 << 24) | 0x222B}]
			set took [accel_option .options .options.f.nb $code]
			set tab [.options.f.nb select]
			ttk::entry .options.probe
			set typing [accel_option .options .options.probe $code]
			destroy .options.probe
			set r [list $took $tab $typing]
		} else {
			uplevel #0 [bind .options <Alt-b>]
			set r [list 1 [.options.f.nb select] 0]
		}
		destroy .options
		set r
	} "1 .options.f.nb.birth 0"

	set ::waits 0
	birth_pass_ready
}

# The note on angband_push above: with a character in play the queue is
# drained, so a command pushed now is carried out.  Pushed once the game is
# waiting for a command -- the menu bar's own test, a command being enabled --
# and holding still takes a turn, which the turn counter shows.
proc birth_pass_ready {} {
	set enabled [llength [lsearch -all -index 5 [angband_commands] 1]]
	if {!$enabled && [incr ::waits] < 50} {
		after 200 birth_pass_ready
		return
	}
	set ::turn0 [angband_player turn]
	set ::pushed [catch {angband_push CMD_HOLD} ::pusherr]
	set ::waits 0
	birth_pass_held
}

proc birth_pass_held {} {
	if {[angband_player turn] == $::turn0 && [incr ::waits] < 25} {
		after 200 birth_pass_held
		return
	}
	check "a pushed command is carried out once there is a character" {
		list [expr {[angband_player turn] > $::turn0}] $::pushed $::pusherr
	} "1 0 {}"

	# The roguelike keyset fires at the nearest target with Tab, which the
	# game codes as KC_TAB, above ASCII.  It came through as that byte, which
	# is not even valid UTF-8, and a menu cannot show it.  The keyset is the
	# character's, so this waits for one to be in play.
	check "a key with no character is named, not passed as a byte" {
		set was [angband_option rogue_like_commands]
		angband_option rogue_like_commands 1
		set row [lsearch -inline -index 3 [angband_commands] \
			"Fire at nearest target"]
		angband_option rogue_like_commands $was
		lindex $row 4
	} Tab

	# The Choice pane's item menu, built as clicking the first item builds
	# it.  It went missing when the menu bar's keys moved out of brackets: it
	# still called the procs that put them there, and nothing tested it.
	check "clicking an item builds its menu, with the keys in their own column" {
		set m [choice_menu_build 0]
		list [$m entrycget 2 -label] [$m entrycget 2 -accelerator]
	} [list Inspect [commands_accel I]]

	set ::waits 0
	tips_pass
}

# --- the Tips window --------------------------------------------------------

# The runner's user folder is new each time, so this is a first start: the
# Tips window opens on its own once the game is under way, at the first tip.
proc tips_pass {} {
	if {![winfo exists .tips] && [incr ::waits] < 25} {
		after 200 tips_pass
		return
	}
	check "the Tips window opens when the first game starts, at the first tip" {
		list [winfo exists .tips] $::tips(at) \
			[expr {$::tips(of) eq "1/[llength $::tips(list)]"}] \
			[stock_widgets_only .tips]
	} "1 0 1 {classic {}}"

	check "there are tips, and each is a sentence or more" {
		set short {}
		foreach tip $::tips(list) {
			if {![regexp {[.!?]\)?$} $tip] || [string length $tip] < 30} {
				lappend short $tip
			}
		}
		list [expr {[llength $::tips(list)] >= 32}] $short
	} "1 {}"

	# The original's box is ten lines of text; a tip that needs scrolling
	# would have nothing to say that it did.
	check "every tip fits the window without scrolling" {
		set over {}
		set i 0
		foreach tip $::tips(list) {
			tips_show $i
			update idletasks
			if {[lindex [.tips.f.tip yview] 1] < 1.0} { lappend over [expr {$i + 1}] }
			incr i
		}
		set over
	} ""

	# Where it left off, and whether it opens at all, are the front end's
	# settings, in the user folder -- the runner's own.
	check "the tips carry on where they left off, from the settings file" {
		tips_show 4
		tips_close
		settings_load
		set last [settings_get tips,last ""]
		tips_window
		set at $::tips(at)
		tips_close
		list $last $at [expr {[file dirname $::settings(file)] eq [angband_path user]}] \
			[file exists $::settings(file)]
	} "4 5 1 1"

	check "and whether the window opens at the start is kept" {
		set ::tips(show) 0
		settings_set tips,show 0
		settings_load
		set r [settings_get tips,show 1]
		settings_set tips,show 1
		set r
	} 0

	check "the Help menu opens the tips" {
		set i [.menubar index Help]
		set m [.menubar entrycget $i -menu]
		$m invoke [$m index "Tips…"]
		set r [winfo exists .tips]
		tips_close
		set r
	} 1

	help_pass
}

# --- the Help window --------------------------------------------------------

# The manual, as scripts/build-tk-help made it from docs/: every page the
# Contents names is there, and every link in it leads to a page that is, and
# to a place on that page.  A link that does not is the builder's fault or a
# page that has gone, and either way a player would click it and get nothing.
proc help_pass {} {
	check "the Help window carries the manual's pages" {
		set missing {}
		foreach doc [help_docs] {
			if {![llength [help_page $doc]]} { lappend missing $doc }
		}
		list [expr {[llength [help_docs]] >= 25}] $missing
	} "1 {}"

	check "every link in the manual leads to a page, and a place on it" {
		set marks [dict create]
		foreach doc [help_docs] {
			foreach op [help_page $doc] {
				if {[lindex $op 0] eq "M"} { dict lappend marks $doc [lindex $op 1] }
			}
		}
		set targets {}
		foreach doc [help_docs] {
			foreach op [help_page $doc] {
				if {[lindex $op 0] eq "L"} { lappend targets $doc [lindex $op 3] }
			}
		}
		set queue [help_contents]
		while {[llength $queue]} {
			set queue [lassign $queue node]
			lappend targets "" [lindex $node 2]
			lappend queue {*}[lindex $node 3]
		}
		set broken {}
		foreach {from target} $targets {
			if {[regexp {^(https?|mailto):} $target]} continue
			set at [string first # $target]
			set doc [expr {$at < 0 ? $target : [string range $target 0 $at-1]}]
			set anchor [expr {$at < 0 ? "" : [string range $target $at+1 end]}]
			if {$doc eq ""} { set doc $from }
			if {![dict exists $marks $doc]} {
				lappend broken "$from -> $target"
			} elseif {$anchor ne "" && $anchor ni [dict get $marks $doc]} {
				lappend broken "$from -> $target"
			}
		}
		list [expr {[llength $targets] > 300}] [lrange $broken 0 4]
	} "1 {}"

	check "the Help window opens at its contents, goes to a place, and back" {
		catch {destroy .help}
		help_window
		update
		set first $::helpwin(doc)
		help_show birth#races
		update
		set t .help.f.pw.page.t
		set top [string trim [$t get @0,0 "@0,0 lineend"]]
		set at $::helpwin(doc)
		help_show attack
		help_back
		set back $::helpwin(doc)
		help_next
		list $first $at $top $back $::helpwin(doc)
	} "@contents birth Races birth attack"

	# Showing a page selects it in the Contents tree, and the tree's
	# <<TreeviewSelect>> must not show it again: it did, which selected it
	# again, and the game spun for ever the first time a chapter was opened.
	check "a page selects its Contents entry, and nothing more" {
		set ::shows 0
		trace add execution help_show enter {apply {args { incr ::shows }}}
		help_show dungeon
		set ::shows 0
		update
		update
		trace remove execution help_show enter {apply {args { incr ::shows }}}
		list $::shows [.help.f.pw.side.nb.contents.tv selection]
	} "0 t:dungeon"

	check "Search finds the pages that mention a word, most mentions first" {
		set ::helpwin(query) Wilderness
		help_search
		list [expr {[llength $::helpwin(results)] > 3}] \
			[expr {"wilderness" in [lrange $::helpwin(results) 0 2]}]
	} "1 1"

	# F1 in a window opens the manual where it covers what the window shows:
	# the Character window's page, the Spell Book's realm or, with no book,
	# the realms.
	check "each window's F1 opens its place in the manual" {
		set got {}
		set was $::book(n)
		set ::book(n) ""
		set page $::character(page)
		foreach do {items_help knowledge_help book_help} {
			$do
			lappend got [lindex $::helpwin(history) $::helpwin(at)]
		}
		foreach ::character(page) {virtues info} {
			character_help
			lappend got [lindex $::helpwin(history) $::helpwin(at)]
		}
		set ::book(n) $was
		set ::character(page) $page
		destroy .help
		set got
	} "{command inventory-commands} {command game-status-commands} {realms {}}\
 {virtues {}} {birth character-characteristics}"

	check "Help ▸ Help opens the Help window" {
		destroy .help
		.menubar.about invoke 0
		set r [winfo exists .help]
		destroy .help
		set r
	} 1

	# The original's ? opened its help browser, and so does this one's:
	# do_cmd_help asks the front end through help_view_hook.
	angband_key 63 0 ?
	set ::waits 0
	help_key_wait
}

proc help_key_wait {} {
	if {![winfo exists .help] && [incr ::waits] < 25} {
		after 200 help_key_wait
		return
	}
	check "the game's ? opens the Help window" {
		set r [winfo exists .help]
		catch {destroy .help}
		set r
	} 1
	door_pass
}

# --- a door in the command menus --------------------------------------------

# "Debug mode commands" is no command but the door ^A opens, and clicking it
# did nothing: it has neither a hook nor a command to push.  It presses the key
# now, and the game puts up the prompt the key does, "Debug Command:" -- whose
# own confirmation comes later, when a debug command is chosen.  Escaped here,
# so the character is not marked.
proc door_pass {} {
	set row [lsearch -inline -index 3 [angband_commands] "Debug mode commands"]
	angband_command [lindex $row 0] [lindex $row 2]
	set ::waits 0
	door_wait
}

# The main screen's top line, where the game puts its prompts.
proc door_top_line {} {
	set c [lindex $::angband(terms) 0 0]
	set cells {}
	foreach id [$c find all] {
		if {[$c type $id] ne "text"} continue
		lassign [$c coords $id] x y
		if {int($y / $::angband(cellh)) != 0} continue
		dict set cells [expr {int($x / $::angband(cellw))}] [$c itemcget $id -text]
	}
	set line ""
	foreach col [lsort -integer [dict keys $cells]] {
		while {[string length $line] < $col} { append line " " }
		append line [dict get $cells $col]
	}
	string trim $line
}

proc door_wait {} {
	set top [door_top_line]
	if {![string match "Debug Command*" $top] && [incr ::waits] < 25} {
		after 200 door_wait
		return
	}
	check "the Debug mode commands entry opens what ^A opens" {
		lassign [angband_option scoring] cheated wizard eligible
		list [string match "Debug Command*" $top] $wizard $eligible
	} "1 0 1"
	angband_key 0xff1b 0 "\x1b"
	after 500 ctrl_pass
}

# The same door from the keyboard, with Control and a letter as Tk on the Mac
# reports them: the letter's keysym, the Control bit, and no character at all.
# Taken for a bare modifier and dropped until 12 October 2026, so none of the
# game's ^ commands could be typed -- reported as ^F, Feeling, doing nothing.
proc ctrl_pass {} {
	set ::ctrlbefore [door_top_line]
	angband_key 97 4 ""
	set ::waits 0
	ctrl_wait
}

proc ctrl_wait {} {
	set top [door_top_line]
	if {![string match "Debug Command*" $top] && [incr ::waits] < 25} {
		after 200 ctrl_wait
		return
	}
	check "Control-A, as the Mac's Tk reports it, is the game's ^A" {
		list [string match "Debug Command*" $::ctrlbefore] \
			[string match "Debug Command*" $top]
	} "0 1"
	angband_key 0xff1b 0 "\x1b"
	after 500 death_pass
}

# --- the fourth pass: the end of a character --------------------------------

# Retired from the menu, as a player would: the death screen asks the Tomb
# window, which shows the game's own tombstone with the game's menu beside it,
# and New Game there goes back to the Choose Game window.
proc death_pass {} {
	bind . <<Angband_CHOOSE>> {+
		if {![catch {angband_choice} state]} {
			lassign $state open prompt kind
			if {$open && $kind eq "check" && [string match "*retire*" $prompt]} {
				after 300 { angband_choice pick 0; after 500 { angband_key 0 0 @ } }
			}
		}
	}
	bind . <<Angband_DEATHVIEW>> {+ after 500 death_pass_tomb }
	set ::deathwatch [after 20000 {
		check "the character retired to the Tomb window" {
			list timed out: dead [angband_player is_dead] tomb [winfo exists .tomb]
		} "retired"
		finish
	}]
	set row [lsearch -inline -index 3 [angband_commands] "Retire*"]
	angband_command [lindex $row 0] [lindex $row 2]
	death_pass_more
}

# Past the -more- that retiring ends with.
proc death_pass_more {} {
	if {![catch {angband_player is_dead} dead] && $dead} {
		angband_key 0 0 " "
		return
	}
	after 200 death_pass_more
}

proc death_pass_tomb {} {
	check "the death screen asks the Tomb window, with the game's menu" {
		lassign [angband_death] how actions
		list $how [join [lmap a $actions {lindex $a 0}] ""] \
			[winfo exists .tomb] [stock_widgets_only .tomb]
	} "retired imfvxhsnq 1 {classic {}}"

	check "the tombstone carries the character's portrait" {
		llength [.tomb.f.top.c find withtag portrait]
	} 1

	# A retirement is not scored, so there is nothing to keep a record
	# against -- but a record kept for a game that was is its dump, its
	# messages and a photo of the last thing seen, found again by its score.
	check "a retired character is not scored, and offered no record" {
		dict get [angband_death entry] scored
	} 0

	check "a Character Record keeps what it is asked to, and finds its score" {
		set entry [dict replace [angband_death entry] scored 1]
		if {"record_last_sight" ni [image names]} {
			image create photo record_last_sight
		}
		set ::record(hasphoto) [angband_death photo record_last_sight]
		set ::record(dump) 1
		set ::record(messages) 1
		set ::record(photo) 1
		record_keep $entry
		set dir [record_for $entry]
		list [expr {$dir ne ""}] [lsort [record_contents $dir]] \
			[expr {[file size [file join $dir dump.txt]] > 1000}]
	} "1 {{character dump} {message log} photo} 1"

	check "the tombstone is the one the game drew" {
		set tomb [angband_death tomb]
		list [expr {[lsearch -glob $tomb "*[angband_player name]*"] >= 0}] \
			[expr {[lsearch -glob $tomb "*Retired on Level*"] >= 0}]
	} "1 1"

	# The game is waiting on the window and nothing else, so a dump is safe.
	check "the character can be dumped from the death screen" {
		set fd [file tempfile path]
		close $fd
		set r [list [catch {angband_dump write $path} e] $e \
			[expr {[file size $path] > 1000}]]
		file delete $path
		set r
	} "0 {} 1"

	# New Game, confirmed as the window confirms it.
	proc ::tk_messageBox {args} { return yes }
	tomb_act n
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
