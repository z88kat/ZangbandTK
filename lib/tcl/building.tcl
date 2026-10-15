# The Building window.
#
# T7's building window: one of the town's service buildings -- the healer, the
# inn, the magetower, the magesmith, the recharger, the Chaos Tower -- while it
# is asking the character something.  The building's question at the top, what
# it offers beneath with the prices in a column of their own, Choose and Leave,
# and gold in the status bar.
#
# A visit is the game's.  Walking in runs the building, which asks what it
# asks -- a menu of services, yes or no to a price, which item to work on, and
# in a building that hands out work, whether you will take it on -- and the
# visit is over when it stops asking.  This window is a second way to answer,
# as the Choice pane is: the keyboard still works, and what it shows and how a
# click answers are angband_choice's.  So it opens on the building's first
# question rather than at the door -- the inn by day says its piece and asks
# nothing -- and closes when the building has nothing more to ask.
#
# Built from Tk's own widgets in the classic theme (decision 18), as the Store
# window is.

set building(where) ""
set building(ask) ""
set building(gold) ""
set building(msg) ""
set building(kind) ""
set building(return) ""

# What the game calls the building, as a name: "chaos tower" is the Chaos
# Tower.
proc building_title {name} {
    set words {}
    foreach word [split $name] { lappend words [string totitle $word] }
    return [join $words]
}

# --- opening and closing --------------------------------------------------------

# Open on a building's question, close when it has none.
proc building_check {} {
    if {[catch {angband_service} here]} { set here {} }
    if {[catch {angband_choice} state]} { set state {0 {} {}} }
    if {[llength $here] && [lindex $state 0]} {
        if {![winfo exists .building]} { building_window }
        building_refresh
    } elseif {[winfo exists .building]} {
        building_close
    }
}

# Back to wherever the keys were going before the building asked.
proc building_close {} {
    global building

    destroy .building
    if {$building(return) ne "" && [winfo exists $building(return)]} {
        focus $building(return)
    }
    set building(return) ""
}

# Leaving is the game's: Escape declines whatever the building is asking, as
# from the keyboard, and the visit ends.
proc building_leave {} {
    angband_key 0xff1b 0 "\x1b"
}

# --- reading the game ----------------------------------------------------------

# A row of the game's as the table shows it: {n key text price valid}.
#
# A menu entry is a line the game has laid out in columns with spaces --
# "Bind your wounds  ...  120 au", and the magetower's name, kind and fare --
# so it is split where the game left two spaces or more, and a last column in
# gold becomes the price.  The rest is the entry, with any middle column after
# it in brackets: "Kashfa (town)".
proc building_row {kind row} {
    switch -- $kind {
        item {
            lassign $row n view label name weight
            if {$view ne "inventory"} { append name " ($view)" }
            return [list $n $label $name \
                [format "%d.%d lb" [expr {$weight / 10}] [expr {$weight % 10}]] 1]
        }
        spell {
            lassign $row n label name level mana fail comment colour valid
            return [list $n $label $name "" $valid]
        }
        default {
            lassign $row n label text valid
            set fields [regexp -all -inline {\S+(?: \S+)*} $text]
            set price ""
            if {[llength $fields] > 1
                    && [regexp {^(\d+) au$} [lindex $fields end] -> price]} {
                set fields [lrange $fields 0 end-1]
            }
            set text [lindex $fields 0]
            if {[llength $fields] > 1} {
                append text " ([join [lrange $fields 1 end] {, }])"
            }
            return [list $n $label $text $price $valid]
        }
    }
}

proc building_refresh {} {
    global building

    if {![winfo exists .building]} return
    if {[catch {angband_service} here] || ![llength $here]} return
    if {[catch {angband_choice} state]} return
    lassign $here service name town gold
    lassign $state open prompt kind

    set title [building_title $name]
    wm title .building "Zangband: $title"
    set building(where) [expr {$town ne "" ? "The $title of $town." : "The $title."}]
    set building(gold) "Gold: $gold"
    if {$prompt ne $building(ask)} { set building(msg) "" }
    set building(ask) $prompt
    set building(kind) $kind

    set tv .building.f.list.tv
    $tv heading text -text [building_word {item Item check Answer} $kind Choice]
    $tv heading price -text [building_word {item Weight} $kind Price]
    # A yes or no has nothing to put in the last column.
    $tv configure -displaycolumns [expr {$kind eq "check" ? {key text} : {key text price}}]

    set keep [lindex [$tv selection] 0]
    $tv delete [$tv children {}]
    set first ""
    foreach row [expr {$open ? [angband_choice list] : {}}] {
        lassign [building_row $kind $row] n key text price valid
        $tv insert {} end -id r$n -values [list $key $text $price] \
            -tags [expr {$valid ? {} : {off}}]
        if {$valid && $first eq ""} { set first r$n }
    }
    if {$keep ne "" && [$tv exists $keep]} {
        $tv selection set $keep
    } elseif {$first ne ""} {
        $tv selection set $first
    }
    $tv see [lindex [$tv selection] 0]
    building_pick
}

# The column headings follow what is being asked.
proc building_word {words kind default} {
    return [expr {[dict exists $words $kind] ? [dict get $words $kind] : $default}]
}

# Choose is offered for a row the game would accept.
proc building_pick {} {
    set tv .building.f.list.tv
    set id [lindex [$tv selection] 0]
    set ok [expr {$id ne "" && "off" ni [$tv item $id -tags]}]
    .building.f.act.do state [expr {$ok ? "!disabled" : "disabled"}]
}

# --- answering -----------------------------------------------------------------

proc building_choose {} {
    global building

    set id [lindex [.building.f.list.tv selection] 0]
    if {$id eq "" || [.building.f.act.do instate disabled]} { bell; return }
    if {[catch {angband_choice pick [string range $id 1 end]} err]} {
        set building(msg) [string toupper $err 0 0]
        bell
    }
}

# What the window uses itself stays here: the keys that move about the table.
# Everything else goes to the game, whose own prompt is still listening -- so
# the letters in the first column choose, as they do on the main screen.
proc building_key {w keysym n s a} {
    if {[winfo class $w] eq "Treeview"
            && $keysym in {Up Down Prior Next Home End Return}} return
    angband_key $n $s $a
}

# --- the window ----------------------------------------------------------------

# F1: the manual on the town's services, which this window is.
proc building_help {} {
    help_window [help_first towns#services towns]
}

proc building_window {} {
    global building

    set building(return) [focus]
    set building(ask) ""
    set building(msg) ""

    toplevel .building
    wm title .building "Zangband"
    wm protocol .building WM_DELETE_WINDOW building_leave
    wm minsize .building 600 420

    set mb .building.mb
    menu $mb
    .building configure -menu $mb
    menu $mb.file
    $mb add cascade -label File -underline 0 -menu $mb.file
    $mb.file add command -label "Leave" -underline 0 -accelerator Esc \
        -command building_leave
    # Not $mb.help, for the reason store.tcl gives.
    menu $mb.about
    $mb add cascade -label Help -underline 0 -menu $mb.about
    $mb.about add command -label "On Services..." -underline 0 -accelerator F1 \
        -command building_help

    set w .building.f
    ttk::frame $w
    pack $w -fill both -expand 1

    set field [ttk::style lookup TEntry -fieldbackground {} white]
    ui_treeview Building.Treeview [uifont 12] \
                -fieldbackground $field -background $field
    ttk::style configure Building.Treeview.Heading -font [uifont 12 bold]

    ttk::label $w.where -textvariable building(where) -font [uifont 12]
    pack $w.where -anchor w -padx 10 -pady {10 2}
    ttk::label $w.ask -textvariable building(ask) -wraplength 560 -justify left
    pack $w.ask -anchor w -padx 10 -pady {2 6}

    # --- status, buttons and the rule over them, packed before the table so
    # that they keep their room when the window is made smaller
    ttk::frame $w.status
    pack $w.status -side bottom -fill x -padx 2 -pady 2
    ttk::label $w.status.msg -textvariable building(msg) -font [uifont 12] \
        -relief sunken -borderwidth 1 -anchor w -padding {4 1}
    ttk::label $w.status.gold -textvariable building(gold) -font [uifont 12] \
        -relief sunken -borderwidth 1 -anchor center -width -16 -padding {4 1}
    pack $w.status.gold -side right -padx {2 0}
    pack $w.status.msg -side left -fill x -expand 1

    ttk::frame $w.btns
    pack $w.btns -side bottom -fill x -padx 10 -pady 10
    ttk::button $w.btns.leave -text "Leave" -underline 0 -command building_leave
    pack $w.btns.leave -side right

    ttk::separator $w.sep -orient horizontal
    pack $w.sep -side bottom -fill x

    ttk::frame $w.act
    pack $w.act -side bottom -fill x -padx 10 -pady {8 10}
    ttk::button $w.act.do -text "Choose" -underline 0 -width 8 -default active \
        -command building_choose
    pack $w.act.do -side left

    set f $w.list
    ttk::frame $f
    pack $f -fill both -expand 1 -padx 10
    set tv $f.tv
    ttk::treeview $tv -style Building.Treeview -show headings -selectmode browse \
        -height 8 -columns {key text price} -yscrollcommand [list $f.sb set]
    ttk::scrollbar $f.sb -orient vertical -command [list $tv yview]
    foreach {col text anchor width stretch} {
        key {} center 36 0  text Choice w 380 1  price Price e 90 0
    } {
        $tv heading $col -text $text -anchor $anchor
        $tv column $col -anchor $anchor -width $width -stretch $stretch
    }
    $tv tag configure off -foreground #909090
    grid $tv -row 0 -column 0 -sticky nsew
    grid $f.sb -row 0 -column 1 -sticky ns
    grid rowconfigure $f 0 -weight 1
    grid columnconfigure $f 0 -weight 1

    bind $tv <<TreeviewSelect>> building_pick
    bind $tv <Double-1> {
        if {[%W identify region %x %y] eq "cell"} { building_choose }
    }
    bind $tv <Return> { building_choose ; break }

    bind .building <Key> { building_key %W %K [key_sym %N %k] %s %A }
    bind .building <Escape> { building_leave ; break }
    bind .building <F1> { building_help ; break }
    accel_bind .building c { building_choose ; break }
    accel_bind .building l { building_leave ; break }

    focus $tv
}

foreach event {CHOOSE LEAVE_SERVICE} {
    bind . <<Angband_$event>> {+ building_check }
}
bind . <<Angband_GOLD>> {+ building_refresh }
