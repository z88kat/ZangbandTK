# The Birth window: making a character.
#
# T8.  ui-birth.c still runs character creation -- the stages, their order,
# stepping back, the realm question asked only when there is a choice, and
# what each answer pushes -- and asks for each stage's input through its view
# hook.  The view is this window: angband_birth says which stage is asking and
# what it offers, and every button answers the stage in its own terms
# (angband_birth answer), so a click here does exactly what the key would on
# the text screen.  The descriptions are the game's, the words the text screen
# puts beside its menus; nothing here is written from the old message
# catalogues, which is how the original's Moorcock patrons would have come
# back.
#
# Built from Tk's own widgets in the classic theme (decision 18), as the other
# windows are: a page per kind of stage, a row of steps along the top, and
# Back and Next along the bottom.

set birth(stage) ""
set birth(slot) 0
set birth(rows) {}
set birth(question) ""
set birth(step) ""
set birth(left) ""
set birth(rolled) 0
set birth(history) ""
set birth(name) ""
set birth(keep) 0

# The steps along the top, and which of them each stage belongs to.
set birth(steps) {Race Class Realms Stats Name History Confirm}
set birth(stepof) {
    race Race  class Class  realm Realms  method Stats  points Stats
    roller Stats  name Name  history History  confirm Confirm
}

# The menu stages, and what their list is headed.
set birth(menus) {race Race class Class realm Realm method Method}

proc birth_answer {verb args} {
    if {[catch {angband_birth answer $verb {*}$args} err]} { bell }
}

# --- following the game ---------------------------------------------------------

# The game is asking: show the page for the stage.
proc birth_show {} {
    global birth

    if {[catch {angband_birth} here] || ![llength $here]} return
    lassign $here stage slot
    if {![winfo exists .birth]} birth_window

    set changed [expr {$stage ne $birth(stage) || $slot != $birth(slot)}]
    set birth(stage) $stage
    set birth(slot) $slot

    # A new history has gone in; the stage asks again, and the answer is
    # the one already given -- keep it.
    if {$stage eq "history" && $birth(keep)} {
        set birth(keep) 0
        after idle [list birth_answer accept]
        return
    }

    birth_steps
    set w .birth.f
    foreach page {menu points roller name history confirm quickstart} {
        pack forget $w.$page
    }
    switch -- $stage {
        race - class - realm - method { birth_menu_page $changed }
        points     { birth_points_page }
        roller     { birth_roller_page }
        name       { birth_name_page }
        history    { birth_history_page }
        confirm    { birth_confirm_page }
        quickstart { birth_quickstart_page }
    }
    birth_buttons
    if {[wm state .birth] eq "withdrawn"} { wm deiconify .birth }
    raise .birth
}

# The row of steps, the current one in bold and those still to come greyed.
proc birth_steps {} {
    global birth

    set now [expr {[dict exists $birth(stepof) $birth(stage)]
        ? [dict get $birth(stepof) $birth(stage)] : ""}]
    set at [lsearch $birth(steps) $now]
    set i 0
    foreach step $birth(steps) {
        set l .birth.f.steps.s$i
        $l configure -font [expr {$i == $at ? {Helvetica 12 bold} : {Helvetica 12}}] \
            -foreground [expr {$at < 0 || $i > $at ? "#909090" : "black"}]
        incr i
    }
}

proc birth_on {w on} { $w state [expr {$on ? "!disabled" : "disabled"}] }

# Which buttons a stage has, and what Next says.
proc birth_buttons {} {
    global birth

    set b .birth.f.btns
    set menu [dict exists $birth(menus) $birth(stage)]

    # The options are offered where the text screen takes '=', the random
    # choice where it takes '*' -- the race and the class -- and finishing at
    # random where it takes '@'.
    birth_on $b.options [expr {$menu || $birth(stage) eq "quickstart"}]
    birth_on $b.random [expr {$birth(stage) in {race class}}]
    birth_on $b.finish $menu
    $b.back state [expr {$birth(stage) eq "quickstart" ? "disabled" : "!disabled"}]
    $b.next configure -text [expr {$birth(stage) eq "confirm"
        ? "Create Character" : "Next >"}]
    $b.next state [expr {$birth(stage) eq "quickstart" ? "disabled" : "!disabled"}]
}

# --- the menu stages: race, class, realm, and how the stats are made -------------

proc birth_menu_page {changed} {
    global birth

    set p .birth.f.menu
    pack $p -fill both -expand 1 -padx 10 -pady {4 0}

    set birth(question) [angband_birth question]

    # A class with two realms is asked twice, once a realm, and the two
    # questions have to look like two: which one it is comes first, as in
    # the original's two screens, and the second names the first.
    if {$birth(stage) eq "realm"} {
        set hint $birth(question)
        if {$birth(slot) == 0} {
            set birth(question) "Choose your first realm of magic.  $hint"
        } else {
            set first ""
            catch {set first [lindex [dict get [angband_birth character] realms] 0]}
            set birth(question) "Choose your second realm of magic[expr {$first ne ""
                ? ", to study beside [string totitle $first]" : ""}].  $hint"
        }
    }
    lassign [angband_birth choices] cursor rows
    set birth(rows) $rows

    # Nothing to describe a method by, so no box to describe it in.
    if {$birth(stage) eq "method"} {
        pack forget $p.desc
    } elseif {![winfo ismapped $p.desc] && [winfo manager $p.desc] eq ""} {
        pack $p.desc -side left -fill both -expand 1 -padx {12 0}
    }

    set tv $p.list.tv
    $tv heading name -text [dict get $birth(menus) $birth(stage)]
    set keep [lindex [$tv selection] 0]
    $tv delete [$tv children {}]
    foreach row $rows {
        lassign $row n name
        $tv insert {} end -id r$n -values [list [string totitle $name 0 0]]
    }
    # Where the game's own cursor is, unless the player has moved it here.
    set pick [expr {!$changed && $keep ne "" && [$tv exists $keep] ? $keep : "r$cursor"}]
    if {[$tv exists $pick]} {
        $tv selection set $pick
        $tv see $pick
    }
    focus $tv
    birth_describe
}

# The race, class or realm chosen in the list: in words first, as the
# original's window described it, and then by the game's own numbers -- or,
# for a realm, this class's books in it.  How the stats are made has only its
# question.
proc birth_describe {} {
    global birth

    set tv .birth.f.menu.list.tv
    set id [lindex [$tv selection] 0]
    set text ""
    set prose ""
    if {$id ne "" && $birth(stage) in {race class realm}} {
        set n [string range $id 1 end]
        set name [lindex [lsearch -inline -index 0 $birth(rows) $n] 1]
        if {[catch {angband_birth describe $n} text]} { set text "" }
        if {[catch {angband_birth prose $birth(stage) $name} prose]} {
            set prose ""
        }
    }
    set t .birth.f.menu.desc.t
    $t configure -state normal
    $t delete 1.0 end
    if {$id ne ""} {
        $t insert end [lindex [$tv item $id -values] 0] head "\n\n"
    }
    if {$prose ne ""} { $t insert end $prose prose "\n\n" }
    $t insert end [string trimright $text]
    $t configure -state disabled
}

# The birth options are on the Options window's Birth tab, which can change
# them for as long as the character is being made.
proc birth_options {} {
    options_window birth
}

proc birth_choose {} {
    set id [lindex [.birth.f.menu.list.tv selection] 0]
    if {$id eq ""} { bell; return }
    birth_answer choose [string range $id 1 end]
}

# --- the stats ------------------------------------------------------------------

# The stat table both stat pages share: name, base, race and class
# adjustments, and the total the game makes of them.
proc birth_stat_rows {table extra} {
    foreach w [winfo children $table] { destroy $w }
    set col 0
    foreach head [concat {Stat Base Race Class Total} $extra] {
        ttk::label $table.h$col -text $head -anchor e
        grid $table.h$col -row 0 -column $col -sticky ew -padx 6 -pady {0 4}
        incr col
    }
    set r 1
    foreach row [angband_birth stats] {
        lassign $row name self race class best
        set c 0
        foreach v [list $name $self [format %+d $race] [format %+d $class] $best] {
            ttk::label $table.v$r,$c -text $v -anchor e -font {Courier 12}
            grid $table.v$r,$c -row $r -column $c -sticky ew -padx 6 -pady 1
            incr c
        }
        incr r
    }
    grid columnconfigure $table [expr {$col - 1}] -weight 1
}

proc birth_points_page {} {
    global birth

    set p .birth.f.points
    pack $p -fill both -expand 1 -padx 10 -pady {4 0}
    set birth(question) "Spend the points on your stats.  Each costs more\
        the higher it already is."

    set table $p.table
    birth_stat_rows $table {Cost {}}
    lassign [angband_birth points] left rows
    set spent 0
    set r 1
    foreach row $rows {
        lassign $row cost next buy sell
        incr spent $cost
        set stat [expr {$r - 1}]
        ttk::label $table.v$r,5 -text $cost -anchor e -font {Courier 12}
        grid $table.v$r,5 -row $r -column 5 -sticky ew -padx 6
        ttk::frame $table.b$r
        ttk::button $table.b$r.sell -text "−" -width 2 \
            -command [list birth_answer sell $stat]
        ttk::button $table.b$r.buy -text "+" -width 2 \
            -command [list birth_answer buy $stat]
        $table.b$r.sell state [expr {$sell ? "!disabled" : "disabled"}]
        $table.b$r.buy state [expr {$buy ? "!disabled" : "disabled"}]
        pack $table.b$r.sell $table.b$r.buy -side left -padx 1
        grid $table.b$r -row $r -column 6 -sticky w -padx 6
        incr r
    }
    set birth(left) "Points spent: $spent of [expr {$spent + $left}]"
    focus .birth.f.btns.next
}

proc birth_roller_page {} {
    global birth

    set p .birth.f.roller
    pack $p -fill both -expand 1 -padx 10 -pady {4 0}
    set birth(question) "Reroll until you like what you see.  Previous\
        brings back the roll before this one."
    birth_stat_rows $p.table {}
    $p.act.prev state [expr {$birth(rolled) ? "!disabled" : "disabled"}]
    focus .birth.f.btns.next
}

proc birth_reroll {} {
    global birth
    set birth(rolled) 1
    birth_answer reroll
}

# --- name, history, and the character as made -----------------------------------

proc birth_name_page {} {
    global birth

    set p .birth.f.name
    pack $p -fill both -expand 1 -padx 10 -pady {4 0}
    set birth(question) "What is your character called?"
    if {![catch {angband_birth character} c]} {
        set birth(name) [dict get $c name]
    }
    focus $p.e
    $p.e selection range 0 end
    $p.e icursor end
}

proc birth_history_page {} {
    global birth

    set p .birth.f.history
    pack $p -fill both -expand 1 -padx 10 -pady {4 0}
    set birth(question) "Your character's history.  Change it if you like."
    set birth(history) ""
    if {![catch {angband_birth character} c]} {
        set birth(history) [string trim [dict get $c history]]
    }
    set t $p.t
    $t delete 1.0 end
    $t insert end $birth(history)
    focus $t
}

proc birth_confirm_page {} {
    global birth

    set p .birth.f.confirm
    pack $p -fill both -expand 1 -padx 10 -pady {4 0}
    set birth(question) "This is your character.  Create Character to begin,\
        or Start Over."
    birth_summary $p.t
    focus .birth.f.btns.next
}

# The character as it stands: name, race and class and realms, hit points and
# mana, the stats, the patron if there is one, and the history.
proc birth_summary {t} {
    if {[catch {angband_birth character} c]} return

    $t configure -state normal
    $t delete 1.0 end
    $t insert end [dict get $c name] head "\n"
    $t insert end "[dict get $c race] [dict get $c class]"
    set realms [dict get $c realms]
    if {[llength $realms]} {
        $t insert end ", of [join [lmap r $realms {string totitle $r}] { and }]"
    }
    $t insert end "\n\nHit points [dict get $c hitpoints]"
    if {[dict get $c mana]} { $t insert end ",  mana [dict get $c mana]" }
    $t insert end "\n\n"
    foreach row [angband_birth stats] {
        lassign $row name self race class best
        $t insert end [format "%-4s %8s\n" $name $best]
    }
    if {[dict get $c patron] ne ""} {
        $t insert end "\nServing " {} [dict get $c patron] bold
        if {[dict get $c patron_title] ne ""} {
            $t insert end ", [dict get $c patron_title]"
        }
        $t insert end "\n[dict get $c patron_text]\n"
    }
    $t insert end "\n[string trim [dict get $c history]]\n"
    $t configure -state disabled
}

proc birth_quickstart_page {} {
    global birth

    set p .birth.f.quickstart
    pack $p -fill both -expand 1 -padx 10 -pady {4 0}
    set birth(question) "Start again from your last character?  Play Again\
        makes the same character afresh; you can change the name and history,\
        or make a new one."
    birth_summary $p.t
    focus $p.act.use
}

# Next: whatever accepting means at this stage.
# Press the first of these buttons that is on show and can be pressed.
proc birth_press {args} {
    foreach b $args {
        if {[winfo exists $b] && [winfo ismapped $b] && ![$b instate disabled]} {
            $b invoke
            return
        }
    }
    bell
}

proc birth_next {} {
    global birth

    switch -- $birth(stage) {
        race - class - realm - method { birth_choose }
        name {
            set name [string trim $birth(name)]
            if {$name eq ""} { bell; return }
            birth_answer name $name
        }
        history {
            set text [string trim [.birth.f.history.t get 1.0 end]]
            if {$text ne "" && $text ne $birth(history)} {
                set birth(keep) 1
                birth_answer history $text
            } else {
                birth_answer accept
            }
        }
        default { birth_answer accept }
    }
}

# Closing the window is quitting, as Ctrl-X is on the text screen -- asked
# first, because it cannot be taken back.  Unless the game is not asking
# anything, in which case the character is made and the window is merely
# still there: then closing it closes it.
proc birth_close {} {
    if {[catch {angband_birth} here] || ![llength $here]} {
        destroy .birth
        return
    }
    if {[tk_messageBox -parent .birth -icon question -type yesno \
            -title "Quit" -message "Quit without making a character?"] eq "yes"} {
        birth_answer quit
    }
}

# --- the window ----------------------------------------------------------------

# A text with a scrollbar beside it, so that a description longer than the
# box says so -- the wheel alone scrolled it, with nothing to show there was
# more.
proc birth_scrolled {t sb pady} {
    ttk::scrollbar $sb -orient vertical -command [list $t yview]
    $t configure -yscrollcommand [list $sb set]
    pack $sb -side right -fill y -pady $pady
    pack $t -side left -fill both -expand 1 -pady $pady
}

proc birth_window {} {
    global birth

    toplevel .birth
    wm title .birth "Zangband: New Character"
    wm protocol .birth WM_DELETE_WINDOW birth_close
    wm minsize .birth 720 520

    set field [ttk::style lookup TEntry -fieldbackground {} white]
    ttk::style configure Birth.Treeview -font {Helvetica 12} \
        -rowheight [expr {[font metrics {Helvetica 12} -linespace] + 6}] \
        -fieldbackground $field -background $field
    ttk::style configure Birth.Treeview.Heading -font {Helvetica 12 bold}

    set w .birth.f
    ttk::frame $w
    pack $w -fill both -expand 1

    # The steps, and the question.
    ttk::frame $w.steps
    pack $w.steps -fill x -padx 10 -pady {10 4}
    set i 0
    foreach step $birth(steps) {
        if {$i} {
            ttk::label $w.steps.a$i -text "›" -foreground #909090
            pack $w.steps.a$i -side left -padx 4
        }
        ttk::label $w.steps.s$i -text $step
        pack $w.steps.s$i -side left
        incr i
    }
    ttk::separator $w.rule -orient horizontal
    pack $w.rule -fill x
    ttk::label $w.q -textvariable birth(question) -wraplength 680 -justify left
    pack $w.q -anchor w -padx 10 -pady {8 4}

    # The buttons, packed before the pages so that they keep their room.
    set b $w.btns
    ttk::frame $b
    pack $b -side bottom -fill x -padx 10 -pady 10
    ttk::button $b.options -text "Birth Options…" -command birth_options
    ttk::button $b.random -text "Random" -underline 0 -command {birth_answer random}
    ttk::button $b.finish -text "Finish at Random" -underline 0 \
        -command {birth_answer finish}
    ttk::button $b.next -text "Next >" -default active -command birth_next
    ttk::button $b.back -text "< Back" -command {birth_answer back}
    pack $b.options $b.random $b.finish -side left -padx {0 6}
    pack $b.next -side right
    pack $b.back -side right -padx {0 6}
    ttk::separator $w.rule2 -orient horizontal
    pack $w.rule2 -side bottom -fill x

    # The menu stages: the list, and the game's description of the choice.
    set p $w.menu
    ttk::frame $p
    ttk::frame $p.list
    pack $p.list -side left -fill y
    set tv $p.list.tv
    ttk::treeview $tv -style Birth.Treeview -show headings -selectmode browse \
        -columns {name} -height 14 -yscrollcommand [list $p.list.sb set]
    $tv column name -width 220 -anchor w
    ttk::scrollbar $p.list.sb -orient vertical -command [list $tv yview]
    pack $p.list.sb -side right -fill y
    pack $tv -side left -fill y
    bind $tv <<TreeviewSelect>> birth_describe
    bind $tv <Double-1> {
        if {[%W identify region %x %y] eq "cell"} { birth_choose }
    }
    ttk::labelframe $p.desc -text Description -padding 6
    pack $p.desc -side left -fill both -expand 1 -padx {12 0}
    text $p.desc.t -width 44 -height 14 -wrap word -font {Courier 12} \
        -background $field -foreground black -relief sunken -borderwidth 2 \
        -highlightthickness 0 -padx 6 -pady 6 -state disabled
    $p.desc.t tag configure head -font {Courier 12 bold}
    $p.desc.t tag configure prose -font {Helvetica 13} -spacing3 2
    birth_scrolled $p.desc.t $p.desc.sb 0

    # Point-based stats.
    set p $w.points
    ttk::frame $p
    ttk::frame $p.table
    pack $p.table -anchor nw -pady {6 0}
    ttk::frame $p.act
    pack $p.act -anchor w -pady {12 0}
    ttk::label $p.act.left -textvariable birth(left)
    ttk::button $p.act.reset -text "Reset" -command {birth_answer reset}
    pack $p.act.left -side left
    pack $p.act.reset -side left -padx 12

    # The roller.
    set p $w.roller
    ttk::frame $p
    ttk::frame $p.table
    pack $p.table -anchor nw -pady {6 0}
    ttk::frame $p.act
    pack $p.act -anchor w -pady {12 0}
    ttk::button $p.act.roll -text "Reroll" -underline 0 -command birth_reroll
    ttk::button $p.act.prev -text "Previous" -underline 0 \
        -command {birth_answer previous}
    pack $p.act.roll $p.act.prev -side left -padx {0 6}

    # The name.
    set p $w.name
    ttk::frame $p
    ttk::entry $p.e -textvariable birth(name) -width 32 -font {Helvetica 14}
    ttk::button $p.random -text "Random Name" \
        -command {set birth(name) [angband_birth random_name]}
    grid $p.e $p.random -sticky w -padx {0 8} -pady 8
    bind $p.e <Return> { birth_next ; break }

    # The history.
    set p $w.history
    ttk::frame $p
    text $p.t -width 60 -height 8 -wrap word -font {Helvetica 13} \
        -background $field -foreground black -relief sunken -borderwidth 2 \
        -highlightthickness 1 -padx 6 -pady 6 -undo 1
    birth_scrolled $p.t $p.sb {6 0}

    # The character as made.
    set p $w.confirm
    ttk::frame $p
    text $p.t -width 60 -height 16 -wrap word -font {Courier 12} \
        -background $field -foreground black -relief sunken -borderwidth 2 \
        -highlightthickness 0 -padx 8 -pady 8 -state disabled
    $p.t tag configure head -font {Courier 14 bold}
    $p.t tag configure bold -font {Courier 12 bold}
    ttk::button $p.over -text "Start Over" -command {birth_answer startover}
    pack $p.over -side bottom -anchor w -pady {8 0}
    birth_scrolled $p.t $p.sb {6 0}

    # The quick start: the last character, and what to do with it.
    set p $w.quickstart
    ttk::frame $p
    ttk::frame $p.act
    pack $p.act -side bottom -anchor w -pady {8 0}
    text $p.t -width 60 -height 14 -wrap word -font {Courier 12} \
        -background $field -foreground black -relief sunken -borderwidth 2 \
        -highlightthickness 0 -padx 8 -pady 8 -state disabled
    $p.t tag configure head -font {Courier 14 bold}
    $p.t tag configure bold -font {Courier 12 bold}
    birth_scrolled $p.t $p.sb {6 0}
    ttk::button $p.act.use -text "Play Again" -default active \
        -command {birth_answer use}
    ttk::button $p.act.change -text "Change Name and History" \
        -command {birth_answer change}
    ttk::button $p.act.redo -text "New Character" -command {birth_answer redo}
    pack $p.act.use $p.act.change $p.act.redo -side left -padx {0 8}

    # Return is Next and Escape is Back, as Enter and ESC are on the text
    # screen; the history keeps its Return for new lines.  A button with the
    # focus is pressed -- which on most pages is Next itself, and on the quick
    # start is Play Again.  It used to be left alone, on the understanding that
    # a focused button takes Return for itself; a ttk button does not, so on
    # four pages Return did nothing at all.
    bind .birth <Return> {
        switch -- [winfo class %W] {
            Text {}
            TButton { if {![%W instate disabled]} { %W invoke } ; break }
            default { birth_next ; break }
        }
    }

    # The underlined letters.  Reroll and Random share R, and only one of them
    # is ever there to press.
    accel_bind .birth r {
        birth_press .birth.f.roller.act.roll .birth.f.btns.random ; break
    }
    accel_bind .birth f { birth_press .birth.f.btns.finish ; break }
    accel_bind .birth p { birth_press .birth.f.roller.act.prev ; break }
    bind .birth <Escape> {
        if {$birth(stage) ne "quickstart"} { birth_answer back }
        break
    }
}

bind . <<Angband_BIRTHVIEW>> {+ birth_show }
foreach event {LEAVE_BIRTH ENTER_GAME} {
    bind . <<Angband_$event>> {+ if {[winfo exists .birth]} { destroy .birth } }
}
