# The Choose Game window.
#
# T9: which character to play, as the original's Choose Game window had it --
# every savefile in the save folder by name, the living in bold and the dead in
# plain type, what is known of each beside it, and the file the one chosen is
# kept in underneath -- with the New button the original kept on its startup
# window beside Open.
#
# The choosing is the game's.  play_game() asks which savefile to play when
# the application starts and again after New Game, and select_savefile() in
# ui-game.c decides what is offered, which is chosen to begin with, what a new
# character is saved as, and what happens to a savefile that will not load.
# This window is that menu's view, through angband_savefile: it opens when the
# game asks and is gone once it has an answer.  A dead character can still be
# opened, and the game makes a new one in its place with the quick start --
# so the window says so rather than leave it a surprise.
#
# Built from Tk's own widgets in the classic theme (decision 18).

set choose(sort) name
set choose(rows) {}
set choose(file) ""
set choose(msg) ""
set choose(new) -1
set choose(typed) ""
set choose(typedat) 0
# Set by the Tomb window's New Game: the character just finished is the
# game's default, but a new one is what was asked for.
set choose(fresh) 0

# The game is asking: show the window.
proc choose_show {} {
    global choose

    if {[catch {angband_savefile} here] || ![llength $here]} return
    lassign $here cursor retry
    if {![winfo exists .choose]} choose_window

    set choose(rows) [angband_savefile list]
    set choose(new) -1
    foreach row $choose(rows) {
        if {[lindex $row 3] eq "new"} { set choose(new) [lindex $row 0] }
    }
    choose_fill

    set tv .choose.f.list.tv
    set b .choose.f.btns
    $b.new state [expr {$choose(new) >= 0 ? "!disabled" : "disabled"}]
    if {$choose(fresh) || ![$tv exists c$cursor]} {
        # Nothing to open first: a new character, if one can be made.
        $tv selection set {}
        $b.open configure -default normal
        $b.new configure -default active
        set first [lindex [$tv children {}] 0]
        if {$first ne ""} { $tv focus $first }
    } else {
        $tv selection set c$cursor
        $tv focus c$cursor
        $tv see c$cursor
        $b.open configure -default active
        $b.new configure -default normal
    }
    set choose(fresh) 0
    choose_selected
    if {$retry} {
        set choose(msg) "The savefile chosen could not be opened. Choose\
            another, or make a new character."
        bell
    }

    if {[wm state .choose] eq "withdrawn"} { wm deiconify .choose }
    raise .choose
    focus $tv
}

# What the game knows of a character, as the original put it: level, race and
# class, and how deep -- or how it ended.
proc choose_describe {state level race class depth killer} {
    switch -- $state {
        alive {
            set where [expr {$depth > 0 ? "Depth $depth" : "on the surface"}]
            return "[string trim "Level $level $race $class"], $where"
        }
        dead {
            if {$killer eq "Retiring"} { return "Retired" }
            return "Killed by $killer"
        }
        default { return "" }
    }
}

# The savefiles, sorted as the headings say.  A file that is not a savefile is
# left out, as the original left it out; there is nothing in it to open.
proc choose_fill {} {
    global choose

    set tv .choose.f.list.tv
    set keep [lindex [$tv selection] 0]
    $tv delete [$tv children {}]

    set rows {}
    foreach row $choose(rows) {
        lassign $row n file path state name level race class depth killer
        if {$state in {new invalid}} continue
        if {$name eq ""} { set name $file }
        if {[catch {file mtime $path} saved]} { set saved 0 }
        lappend rows [list $n $name \
            [choose_describe $state $level $race $class $depth $killer] \
            $saved $state $level]
    }
    switch -- $choose(sort) {
        name  { set rows [lsort -dictionary -index 1 $rows] }
        desc  {
            # Living before dead, and by level within them: what sorting the
            # original's Description column came to.
            set rows [lsort -integer -decreasing -index 5 \
                [lsort -dictionary -index 1 $rows]]
            set rows [lsort -command choose_alive_first $rows]
        }
        saved { set rows [lsort -integer -decreasing -index 3 $rows] }
    }
    foreach row $rows {
        lassign $row n name desc saved state
        set when [expr {$saved ? [clock format $saved -format "%d %b %Y  %H:%M"] : ""}]
        $tv insert {} end -id c$n -values [list $name $desc $when] -tags $state
    }
    if {$keep ne "" && [$tv exists $keep]} {
        $tv selection set $keep
        $tv see $keep
    }

    # The sorted column's heading says so.
    foreach {col title} {name Name desc Description saved Saved} {
        $tv heading $col -text [expr {$col eq $choose(sort) ? "$title ▾" : $title}]
    }
}

# lsort is stable, so this keeps the order within each.
proc choose_alive_first {a b} {
    expr {([lindex $a 4] ne "alive") - ([lindex $b 4] ne "alive")}
}

proc choose_sort {col} {
    global choose
    set choose(sort) $col
    choose_fill
}

# The file the character is kept in, and what opening a dead one does.
proc choose_selected {} {
    global choose

    set tv .choose.f.list.tv
    set id [lindex [$tv selection] 0]
    set b .choose.f.btns
    $b.open state [expr {$id eq "" ? "disabled" : "!disabled"}]
    set choose(msg) ""
    if {$id eq ""} {
        set choose(file) ""
        return
    }
    set row [lsearch -inline -index 0 $choose(rows) [string range $id 1 end]]
    lassign $row n file path state name
    set choose(file) "Selected file: $file"
    if {$state eq "dead"} {
        set choose(msg) "[expr {$name ne {} ? $name : $file}] is dead.\
            Opening the savefile makes a new character in its place, with\
            the quick start offered."
    }
}

# Typing a name's first letters goes to it, as in the original.
proc choose_typed {char} {
    global choose

    if {![string is alnum -strict $char]} return
    set now [clock milliseconds]
    if {$now - $choose(typedat) > 700} { set choose(typed) "" }
    set choose(typedat) $now
    append choose(typed) [string tolower $char]

    set tv .choose.f.list.tv
    foreach id [$tv children {}] {
        if {[string match "$choose(typed)*" [string tolower [$tv set $id name]]]} {
            $tv selection set $id
            $tv focus $id
            $tv see $id
            return
        }
    }
    bell
}

# --- answering ------------------------------------------------------------------

proc choose_answer {n} {
    # The game first, the window afterwards, and only if the game took it.
    #
    # angband_savefile answer can refuse, and the game is sitting in a
    # Tcl_DoOneEvent loop waiting for this answer: destroying the chooser
    # first means a refusal leaves nothing on screen to answer with, and
    # nothing will ever arrive, so the game waits for ever behind no window.
    # Answering first costs nothing -- the answer only sets a variable and
    # returns, the game does not resume until this binding does -- and a
    # refusal now leaves the chooser up to try again.
    if {[catch {angband_savefile answer $n}]} { bell; return }
    destroy .choose
}

proc choose_open {} {
    set id [lindex [.choose.f.list.tv selection] 0]
    if {$id eq ""} { bell; return }
    choose_answer [string range $id 1 end]
}

proc choose_new {} {
    global choose
    if {$choose(new) < 0} { bell; return }
    choose_answer $choose(new)
}

# Nothing is lost by quitting here: no character has been opened.
proc choose_quit {} {
    choose_answer -1
}

# Return opens what is selected, or makes a new character when nothing is.
proc choose_default {} {
    if {[.choose.f.btns.open cget -default] eq "active"
            && ![.choose.f.btns.open instate disabled]} {
        choose_open
    } else {
        choose_new
    }
}

# --- the window ----------------------------------------------------------------

proc choose_window {} {
    toplevel .choose
    wm title .choose "Zangband: Choose Game"
    wm protocol .choose WM_DELETE_WINDOW choose_quit
    wm minsize .choose 600 360

    set field [ttk::style lookup TEntry -fieldbackground {} white]
    ttk::style configure Choose.Treeview -font {Helvetica 12} \
        -rowheight [expr {[font metrics {Helvetica 12} -linespace] + 6}] \
        -fieldbackground $field -background $field
    ttk::style configure Choose.Treeview.Heading -font {Helvetica 12 bold}

    set w .choose.f
    ttk::frame $w
    pack $w -fill both -expand 1

    ttk::label $w.prompt -wraplength 640 -justify left -text "Choose a\
        character to play from the savefiles in the save folder, or make\
        a new one."
    pack $w.prompt -anchor w -padx 10 -pady {10 6}

    # --- status, buttons and the rule over them, packed before the list so
    # that they keep their room when the window is made smaller
    ttk::label $w.status -textvariable choose(msg) -font {Helvetica 12} \
        -relief sunken -borderwidth 1 -anchor w -padding {4 1}
    pack $w.status -side bottom -fill x -padx 2 -pady 2

    set b $w.btns
    ttk::frame $b
    pack $b -side bottom -fill x -padx 10 -pady 10
    ttk::label $b.file -textvariable choose(file) -font {Helvetica 12}
    ttk::button $b.open -text "Open" -underline 0 -width 9 -command choose_open
    ttk::button $b.new -text "New" -underline 0 -width 9 -command choose_new
    ttk::button $b.quit -text "Quit" -underline 0 -width 9 -command choose_quit
    pack $b.file -side left
    pack $b.quit -side right
    pack $b.new -side right -padx 6
    pack $b.open -side right

    ttk::separator $w.sep -orient horizontal
    pack $w.sep -side bottom -fill x

    set f $w.list
    ttk::frame $f
    pack $f -fill both -expand 1 -padx 10
    set tv $f.tv
    ttk::treeview $tv -style Choose.Treeview -show headings -selectmode browse \
        -height 12 -columns {name desc saved} -yscrollcommand [list $f.sb set]
    ttk::scrollbar $f.sb -orient vertical -command [list $tv yview]
    foreach {col width stretch} {name 140 0  desc 380 1  saved 150 0} {
        $tv heading $col -anchor w -command [list choose_sort $col]
        $tv column $col -anchor w -width $width -stretch $stretch
    }
    $tv tag configure alive -font {Helvetica 12 bold}
    $tv tag configure dead -font {Helvetica 12}
    grid $tv -row 0 -column 0 -sticky nsew
    grid $f.sb -row 0 -column 1 -sticky ns
    grid rowconfigure $f 0 -weight 1
    grid columnconfigure $f 0 -weight 1

    bind $tv <<TreeviewSelect>> choose_selected
    bind $tv <Double-1> {
        if {[%W identify region %x %y] eq "cell"} { choose_open }
    }
    bind $tv <KeyPress> { choose_typed %A }
    bind .choose <Return> { choose_default ; break }
    bind .choose <F1> { help_window [help_first start] ; break }
    accel_bind .choose o { choose_open ; break }
    accel_bind .choose n { choose_new ; break }
    accel_bind .choose q { choose_quit ; break }
}

bind . <<Angband_CHOOSEGAME>> {+ choose_show }
