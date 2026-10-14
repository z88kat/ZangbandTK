# The Spell Book window.
#
# T7's Book window: the spells of a book as a table that sorts by any column,
# the game's own columns -- name, level, mana, failure and the information the
# game shows once a spell has worked -- each spell coloured by its state, its
# description beneath, and Cast and Study for the one chosen.  Built from Tk's
# own widgets in the classic theme, as decision 18 asks: the table is a
# ttk::treeview, which sorts and selects without anything being drawn by hand.
#
# What it shows is angband_book's, which reads the game's own tables, and what
# it does is the game's commands with their arguments filled in, so the turn,
# the "attempt it anyway?" question and the aiming are the game's, exactly as
# from the keyboard.  Like the other windows it is passive, opens before there
# is a character, and lets keys it does not use itself go on to the game.

set book(n) ""
set book(rows) {}
set book(sel) ""
set book(sort) ""
set book(down) 0
set book(msg) ""
set book(mana) ""
set book(learn) ""

# The game's six states, in the game's order, and a colour for each that reads
# on a white field.  The game's own are for a black screen -- its "known" is
# white -- so these are the same hues made dark enough to read.
array set book {
    colour,known     black
    colour,untried   #1a7a1a
    colour,unknown   #2050b0
    colour,difficult #b03030
    colour,forgotten #a07800
    colour,illegible #909090
}

set book(events) {
    STUDYSTATUS MANA PLAYERLEVEL STATS INVENTORY ENTER_GAME LEAVE_BIRTH
    ENTER_WORLD
}

proc book_tree {} { return .book.f.body.tv }

# --- reading the game ----------------------------------------------------------

proc book_refresh {} {
    global book

    if {![winfo exists .book]} return
    set w .book.f

    if {[catch {angband_book books} books]} { set books {} }

    # The row of books, rebuilt only when the books carried change.
    if {![info exists book(books)] || $book(books) ne $books} {
        set book(books) $books
        destroy {*}[winfo children $w.books.list]
        foreach row $books {
            lassign $row n name realm
            regexp {\[(.*)\]} $name -> short
            if {![info exists short]} { set short $name }
            ttk::radiobutton $w.books.list.b$n -text $short \
                -variable book(n) -value $n -command book_show
            pack $w.books.list.b$n -side left -padx {16 0}
            unset short
        }
        if {![llength $books]} {
            ttk::label $w.books.list.none -font [uifont 12] \
                -text [expr {[catch {angband_player name}]
                    ? "" : "You carry no book you can read."}]
            pack $w.books.list.none -side left -padx {16 0}
        }
    }

    # Keep the book that was open if it is still carried.
    set ns {}
    foreach row $books { lappend ns [lindex $row 0] }
    if {$book(n) ni $ns} { set book(n) [lindex $ns 0] }

    if {[catch {angband_player mana} mana]} {
        set book(mana) ""
        set book(learn) ""
    } else {
        lassign $mana now most
        set book(mana) "Mana $now / $most"
        set learn [angband_player new_spells]
        set book(learn) [expr {$learn == 1 ? "1 to learn" : "$learn to learn"}]
    }
    book_show
}

# The open book's spells into the table, in the chosen order.
proc book_show {} {
    global book

    set w .book.f
    set tv [book_tree]
    $tv delete [$tv children {}]
    set book(rows) {}

    if {$book(n) eq "" || [catch {angband_book spells $book(n)} rows]} {
        $w.body configure -text "Spells"
        book_select
        return
    }
    foreach row $book(books) {
        if {[lindex $row 0] == $book(n)} {
            lassign $row n name realm
            $w.body configure -text "[string totitle $realm]: $name"
        }
    }

    set book(rows) $rows
    if {$book(sort) ne ""} {
        set i [lsearch {name level mana fail info} $book(sort)]
        set type [expr {$i in {1 2 3} ? "-integer" : "-dictionary"}]
        set rows [lsort $type -index [expr {$i + 1}] \
            {*}[expr {$book(down) ? "-decreasing" : ""}] $rows]
    }
    foreach row $rows {
        lassign $row sidx name level mana fail info state castable learnable
        $tv insert {} end -id s$sidx -tags $state \
            -values [list $name $level $mana "$fail%" \
                [expr {$info ne "" ? $info : $state}]]
    }

    if {$book(sel) ne "" && [$tv exists s$book(sel)]} {
        $tv selection set s$book(sel)
        $tv see s$book(sel)
    } elseif {[llength $rows]} {
        $tv selection set [lindex [$tv children {}] 0]
    }
    book_select
}

# The chosen spell: its description, and which of Cast and Study apply.
proc book_select {} {
    global book

    set w .book.f
    set tv [book_tree]
    set id [lindex [$tv selection] 0]
    set row {}
    if {$id ne ""} {
        set sidx [string range $id 1 end]
        set book(sel) $sidx
        set row [lsearch -inline -index 0 $book(rows) $sidx]
    }

    set t $w.desc.t
    $t configure -state normal
    $t delete 1.0 end
    if {[llength $row]} {
        lassign $row sidx name level mana fail info state castable learnable
        $t insert end $name head "\n" {} [angband_book text $sidx]
    }
    $t configure -state disabled

    lassign [book_can $row] cast study
    $w.btns.cast state [expr {$cast ? "!disabled" : "disabled"}]
    $w.btns.study state [expr {$study ? "!disabled" : "disabled"}]
    .book.mb.spells entryconfigure Cast -state [expr {$cast ? "normal" : "disabled"}]
    .book.mb.spells entryconfigure Study -state [expr {$study ? "normal" : "disabled"}]
}

# What the chosen row allows: {cast study}.  A class that learns at random
# studies the book rather than the spell, so for it Study is open whenever
# anything in the book can be learned.
proc book_can {row} {
    global book
    if {![llength $row]} { return {0 0} }
    lassign $row sidx name level mana fail info state castable learnable
    if {[catch {angband_book chooses} chooses]} { return {0 0} }
    if {!$chooses} {
        set learnable 0
        foreach r $book(rows) { if {[lindex $r 8]} { set learnable 1 } }
    }
    return [list $castable $learnable]
}

# --- the commands --------------------------------------------------------------

proc book_cast {} {
    global book
    set row [lsearch -inline -index 0 $book(rows) $book(sel)]
    if {![lindex [book_can $row] 0]} return
    if {[catch {angband_book cast $book(sel)} err]} {
        set book(msg) $err
    } else {
        set book(msg) "Casting [lindex $row 1]."
    }
}

proc book_study {} {
    global book
    set row [lsearch -inline -index 0 $book(rows) $book(sel)]
    if {![lindex [book_can $row] 1]} return
    if {[catch {angband_book study $book(n) $book(sel)} err]} {
        set book(msg) $err
    } elseif {[angband_book chooses]} {
        set book(msg) "Learning [lindex $row 1]."
    } else {
        set book(msg) "Learning from the book."
    }
}

# Double-click or Return: cast what can be cast, learn what can be learned.
proc book_use {} {
    global book
    set row [lsearch -inline -index 0 $book(rows) $book(sel)]
    lassign [book_can $row] cast study
    if {$cast} { book_cast } elseif {$study} { book_study }
}

proc book_sort {column} {
    global book
    if {$book(sort) eq $column} {
        set book(down) [expr {!$book(down)}]
    } else {
        set book(sort) $column
        set book(down) 0
    }
    book_show
}

# F1: the manual on the realm of the book being read -- or its chapter on the
# realms, before there is a book -- and how to use the window in the status
# line.
proc book_help {} {
    global book
    set realm ""
    if {[info exists book(books)]} {
        foreach row $book(books) {
            if {[lindex $row 0] eq $book(n)} {
                set realm [string tolower [lindex $row 2]]
            }
        }
    }
    help_window [help_first [expr {$realm ne "" ? "realms#$realm" : ""}] \
        [expr {$realm ne "" ? "birth#$realm" : ""}] realms]
    set book(msg) "Choose a book and a spell; double-click to cast it, or to\
        learn it if it is new.  Click a heading to sort."
}

# What the window uses itself stays here: the keys that move about the table.
# Everything else goes to the game, so the window can be left open while you
# play.
proc book_key {w keysym n s a} {
    if {[winfo class $w] eq "Treeview"
            && $keysym in {Up Down Prior Next Home End Return}} return
    angband_key $n $s $a
}

# --- the window ----------------------------------------------------------------

proc book_menus {} {
    set mb .book.mb
    menu $mb
    .book configure -menu $mb

    menu $mb.file
    $mb add cascade -label File -underline 0 -menu $mb.file
    $mb.file add command -label Close -underline 0 -accelerator Esc \
        -command {destroy .book}

    menu $mb.spells
    $mb add cascade -label Spells -underline 0 -menu $mb.spells
    $mb.spells add command -label Cast -underline 0 -command book_cast
    $mb.spells add command -label Study -underline 0 -command book_study

    # Not $mb.help: on macOS Tk treats a menu with that name as the platform's
    # own Help menu and fills it with things this window does not have.
    menu $mb.about
    $mb add cascade -label Help -underline 0 -menu $mb.about
    $mb.about add command -label "On the Spell Book..." -underline 0 \
        -accelerator F1 -command book_help
}

proc book_window {} {
    global book

    if {[winfo exists .book]} {
        wm deiconify .book
        raise .book
        return
    }

    unset -nocomplain book(books)
    toplevel .book
    wm title .book "Zangband: Spell Book"
    wm protocol .book WM_DELETE_WINDOW { destroy .book }
    wm minsize .book 640 520

    book_menus

    # One Ttk frame under everything, because that is what carries the theme's
    # grey: the toplevel itself is drawn by the platform.
    set w .book.f
    ttk::frame $w
    pack $w -fill both -expand 1

    set ground [ttk::style lookup . -background]
    set field  [ttk::style lookup TEntry -fieldbackground {} white]

    # --- the books
    ttk::frame $w.books
    pack $w.books -fill x -padx 10 -pady {10 4}
    ttk::label $w.books.l -text "Book:"
    ttk::frame $w.books.list
    pack $w.books.l -side left
    pack $w.books.list -side left -fill x

    # --- status, buttons and the rule over them, packed before the table so
    # that they keep their room when the window is made smaller
    ttk::frame $w.status
    pack $w.status -side bottom -fill x -padx 2 -pady 2
    foreach {name var width} {msg msg 0 mana mana -14 learn learn -12} {
        ttk::label $w.status.$name -textvariable book($var) \
            -font [uifont 12] -relief sunken -borderwidth 1 -padding {4 1} \
            -anchor [expr {$name eq "msg" ? "w" : "center"}]
        if {$width} { $w.status.$name configure -width $width }
    }
    pack $w.status.learn $w.status.mana -side right -padx {2 0}
    pack $w.status.msg -side left -fill x -expand 1

    ttk::frame $w.btns
    pack $w.btns -side bottom -fill x -padx 10 -pady 10
    ttk::button $w.btns.cast -text Cast -underline 0 -width 10 -command book_cast
    ttk::button $w.btns.study -text Study -underline 0 -width 10 \
        -command book_study
    ttk::button $w.btns.close -text Close -width 10 -default active \
        -command {destroy .book}
    pack $w.btns.cast -side left
    pack $w.btns.study -side left -padx {10 0}
    pack $w.btns.close -side right

    ttk::separator $w.sep -orient horizontal
    pack $w.sep -side bottom -fill x

    # --- the description
    ttk::labelframe $w.desc -text Description -padding 6
    pack $w.desc -side bottom -fill x -padx 10 -pady {6 0}
    text $w.desc.t -height 4 -width 60 -wrap word -font [uifont Courier 12] \
        -background $field -foreground black -relief sunken -borderwidth 2 \
        -highlightthickness 1 -highlightcolor black \
        -highlightbackground $ground -padx 4 -pady 4 -state disabled
    $w.desc.t tag configure head -font [uifont Courier 12 bold]
    pack $w.desc.t -fill x

    # --- the table
    set body $w.body
    ttk::labelframe $body -text Spells -padding 6
    pack $body -fill both -expand 1 -padx 10 -pady {6 0}

    ui_treeview Book.Treeview [uifont 12] \
                -fieldbackground $field -background $field
    ttk::style configure Book.Treeview.Heading -font [uifont 12 bold]

    set tv $body.tv
    ttk::treeview $tv -style Book.Treeview -show headings -selectmode browse \
        -height 10 -columns {name level mana fail info} \
        -yscrollcommand [list $body.sb set]
    ttk::scrollbar $body.sb -orient vertical -command [list $tv yview]
    foreach {col text anchor width} {
        name  Name w 220   level Lv e 40   mana Mana e 56
        fail  Fail e 56    info  Info w 160
    } {
        $tv heading $col -text $text -anchor $anchor \
            -command [list book_sort $col]
        $tv column $col -anchor $anchor -width $width \
            -stretch [expr {$col in {name info}}]
    }
    foreach state {known untried unknown difficult forgotten illegible} {
        $tv tag configure $state -foreground $book(colour,$state)
    }
    pack $body.sb -side right -fill y
    pack $tv -side left -fill both -expand 1

    bind $tv <<TreeviewSelect>> { book_select }
    bind $tv <Double-1> {
        if {[%W identify region %x %y] eq "cell"} { book_use }
    }
    bind $tv <Return> { book_use }

    bind .book <Key> { book_key %W %K %N %s %A }
    bind .book <Escape> { destroy .book ; break }
    bind .book <F1> { book_help ; break }
    accel_bind .book c { book_cast ; break }
    accel_bind .book s { book_study ; break }

    book_refresh
    focus $tv
}

# Up to date whether or not anyone is looking: the bindings live on "." and
# cost nothing while the window is closed, because book_refresh returns at once
# when there is none.
foreach event $book(events) {
    bind . <<Angband_$event>> {+ book_refresh }
}
