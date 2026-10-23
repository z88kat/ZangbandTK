# Ignoring: the Options window's Ignoring tab.
#
# The game's own ignoring setup, the options screen's "Item ignoring setup", as
# a page: how good each kind of gear has to be to be kept, the ego items the
# character has seen, and the things ignored by kind, a group at a time.  The
# list on the left is the game's menu, in its order.  What each section shows
# is angband_ignore's, read from the game's own tables, and each change is the
# one the game's menu makes, so the page can ignore nothing the game's screen
# would not.  A change takes effect at once: what is now ignored is hidden, and
# dropped from the pack, as when the game's screen is closed.
#
# Built from Tk's own widgets in the classic theme (decision 18).  The quality
# settings are menubuttons rather than comboboxes, because a combobox changes
# its value when the wheel turns over it, and this page scrolls.

set ignore(rows) {}
set ignore(key) quality
set ignore(note) ""

# The game's names for the quality settings, as the page shows them.
proc ignore_level_name {name} {
    if {$name eq "no ignore"} { return "No ignoring" }
    return [string toupper $name 0 0]
}

proc ignore_page {p} {
    global ignore

    set ground [ttk::style lookup . -background]
    set field  [ttk::style lookup TEntry -fieldbackground {} white]
    set chosen [ttk::style lookup . -selectbackground]

    ttk::label $p.note -textvariable ignore(note) -wraplength 520 \
        -justify left -font [uifont 11]
    pack $p.note -side bottom -anchor w -pady {8 0}

    set l [ttk::frame $p.list]
    listbox $l.lb -width 18 -height 16 -font [uifont 12] \
        -selectmode browse -exportselection 0 -activestyle none \
        -background $field -foreground black \
        -selectbackground $chosen -selectforeground black -selectborderwidth 1 \
        -relief sunken -borderwidth 2 -highlightthickness 1 \
        -highlightcolor black -highlightbackground $ground \
        -yscrollcommand [list $l.sb set]
    ttk::scrollbar $l.sb -orient vertical -command [list $l.lb yview]
    pack $l.sb -side right -fill y
    pack $l.lb -side left -fill both -expand 1
    pack $l -side left -fill y -padx {0 10}
    bind $l.lb <<ListboxSelect>> ignore_choose

    # Named $p.scroll, so the Options window's wheel scrolls it.
    set ignore(inner) [options_scrolled $p]

    ignore_page_refresh
}

# The list, keeping what is chosen, and the section shown again: the game's
# `k` changes the same settings while the window is open.
proc ignore_page_refresh {} {
    global ignore

    set lb .options.f.nb.ignore.list.lb
    if {![winfo exists $lb]} return

    set rows [list {quality "Quality" 1} {egos "Ego items" 1}]
    if {![catch {angband_ignore groups} groups]} {
        foreach g $groups { lappend rows $g }
    }
    set ignore(rows) $rows
    $lb delete 0 end
    set i 0
    foreach row $rows {
        lassign $row key label seen
        $lb insert end $label
        # A group nothing of which has been seen is greyed, as the game's
        # menu greys it, and still chosen, since unknown kinds are listed by
        # their look.
        if {!$seen} { $lb itemconfigure $i -foreground #909090 }
        if {$key eq $ignore(key)} { $lb selection set $i; $lb see $i }
        incr i
    }
    if {![llength [$lb curselection]]} {
        set ignore(key) quality
        $lb selection set 0
    }
    ignore_show
}

proc ignore_choose {} {
    global ignore
    set sel [.options.f.nb.ignore.list.lb curselection]
    if {$sel eq ""} return
    set ignore(key) [lindex $ignore(rows) $sel 0]
    ignore_show
}

# One section, built again whole: they are short, and what they hold changes
# as the character sees more.  Back at the top only for another section.
proc ignore_show {} {
    global ignore

    set in $ignore(inner)
    destroy {*}[winfo children $in]
    if {![info exists ignore(shown)] || $ignore(shown) ne $ignore(key)} {
        [winfo parent $in] yview moveto 0
    }
    set ignore(shown) $ignore(key)

    if {[catch {angband_ignore quality} quality]} {
        ttk::label $in.none -text "Ignoring belongs to a character, and there\
            is not one yet." -font [uifont 12]
        grid $in.none -sticky w
        set ignore(note) ""
        return
    }

    switch -- $ignore(key) {
        quality { ignore_show_quality $in $quality }
        egos    { ignore_show_egos $in }
        default { ignore_show_kinds $in $ignore(key) }
    }
}

proc ignore_show_quality {in rows} {
    global ignore

    set ignore(note) "Gear of each kind is ignored at the quality set here and\
        below, once you know how good it is: Average ignores the bad and the\
        average, and Non-artifact keeps only artifacts."
    set r 0
    foreach row $rows {
        lassign $row itype name level levels
        set ignore(q,$itype) $level
        set ignore(levels,$itype) $levels
        ttk::label $in.l$itype -text $name
        set mb [ttk::menubutton $in.m$itype -width 14 \
            -text [ignore_level_name [lindex $levels $level]]]
        menu $mb.m -tearoff 0
        set i 0
        foreach lv $levels {
            $mb.m add radiobutton -label [ignore_level_name $lv] \
                -variable ignore(q,$itype) -value $i \
                -command [list ignore_quality $itype $i]
            incr i
        }
        $mb configure -menu $mb.m
        grid $in.l$itype -row $r -column 0 -sticky w -padx {0 12} -pady 1
        grid $mb -row $r -column 1 -sticky w -pady 1
        incr r
    }
}

proc ignore_quality {itype level} {
    global ignore
    if {[catch {angband_ignore quality $itype $level} err]} {
        set ignore(note) [string toupper $err 0 0]
        bell
        ignore_show
        return
    }
    $ignore(inner).m$itype configure \
        -text [ignore_level_name [lindex $ignore(levels,$itype) $level]]
}

# Ego names begin "of" or "(", and the game's menu sorts them on what follows.
proc ignore_ego_sortkey {name} {
    regsub {^(of (the )?|\()} $name {} key
    return [string tolower $key]
}

proc ignore_show_egos {in} {
    global ignore

    set ignore(note) "An ego item you have seen can be ignored on each kind of\
        gear it comes on, once you know it is that ego."
    set rows {}
    foreach row [angband_ignore egos] {
        lappend rows [linsert $row 0 [ignore_ego_sortkey [lindex $row 2]]]
    }
    if {![llength $rows]} {
        ttk::label $in.none -text "You haven't seen any ego items yet." \
            -font [uifont 12]
        grid $in.none -sticky w
        return
    }
    set r 0
    foreach row [lsort -dictionary -index 0 $rows] {
        lassign $row - eidx itype name type ignored
        set ignore(e,$eidx,$itype) $ignored
        ttk::checkbutton $in.e$r -text "$type $name" \
            -variable ignore(e,$eidx,$itype) \
            -command [list ignore_ego $eidx $itype]
        grid $in.e$r -row $r -column 0 -sticky w -pady 1
        incr r
    }
}

proc ignore_ego {eidx itype} {
    global ignore
    if {[catch {angband_ignore ego $eidx $itype $ignore(e,$eidx,$itype)} err]} {
        set ignore(note) [string toupper $err 0 0]
        bell
        ignore_show
    }
}

# The kinds the character knows by name, then those still known only by their
# look, which the game's menu lists in that order too.  Ticking a kind by name
# ignores it whatever it looks like, even before you know which look it has.
proc ignore_show_kinds {in tval} {
    global ignore

    set label [lindex [lsearch -inline -index 0 $ignore(rows) $tval] 1]
    set ignore(note) "A ticked kind is hidden and dropped wherever it turns\
        up.  One ticked by name is ignored whatever it looks like, even before\
        you know which look is which."
    set rows [angband_ignore kinds $tval]
    if {![llength $rows]} {
        ttk::label $in.none -text "No [string tolower $label] are known yet." \
            -font [uifont 12]
        grid $in.none -sticky w
        return
    }
    set r 0
    foreach aware {1 0} head {"By name" "By look, not yet identified"} {
        set some [lsearch -all -inline -index 1 $rows $aware]
        if {![llength $some]} continue
        ttk::label $in.h$aware -text $head -font [uifont 12 bold]
        grid $in.h$aware -row $r -column 0 -sticky w \
            -pady [expr {$r ? {10 2} : {0 2}}]
        incr r
        foreach row $some {
            lassign $row kidx - name ignored
            set how [expr {$aware ? "aware" : "unaware"}]
            set ignore(k,$kidx,$how) $ignored
            ttk::checkbutton $in.k$r -text $name \
                -variable ignore(k,$kidx,$how) \
                -command [list ignore_kind $kidx $how]
            grid $in.k$r -row $r -column 0 -sticky w -padx {12 0} -pady 1
            incr r
        }
    }
}

proc ignore_kind {kidx how} {
    global ignore
    if {[catch {angband_ignore kind $kidx $how $ignore(k,$kidx,$how)} err]} {
        set ignore(note) [string toupper $err 0 0]
        bell
        ignore_show
    }
}
