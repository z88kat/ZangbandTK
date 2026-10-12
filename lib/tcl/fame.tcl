# The Hall of Fame window.
#
# T9: the original's (OBS-23) -- the score table as three lines an entry in a
# fixed font on black, the character being played among the dead where its
# score would put it, "killed by nobody (yet!)", a Sort menu, and a status bar
# that says whether the entry under the cursor has a Character Record kept.
# An entry with one says More..., and opening it shows the record.
#
# The table is the game's: angband_scores reads it, and places the living
# character with the game's own prediction.  Sorting is only a way of looking
# at it.  Built from Tk's own widgets in the classic theme (decision 18).

set fame(sort) score
set fame(rows) {}
set fame(msg) ""
set fame(picked) -1

# How an entry reads: the original's three lines.
proc fame_lines {row} {
    dict with row {}
    if {$who eq ""} { set who "(no name)" }
    set first [format "%3d.%10s  %s the %s %s, Level %d" \
        $rank $pts $who $race $class $cur_lev]
    if {$max_lev > $cur_lev} { append first " (Max $max_lev)" }

    set where [expr {$cur_dun > 0 ? "on Dungeon Level $cur_dun" : "on the surface"}]
    set second "                Killed by $how $where"
    if {$max_dun > $cur_dun} { append second " (Max $max_dun)" }

    set third "                ([fame_day $day], Gold $gold, Turn $turns)"
    return [list $first $second $third]
}

# The game writes a date as @YYYYMMDD, and the living character's as TODAY.
proc fame_day {day} {
    if {[regexp {^@(\d{4})(\d\d)(\d\d)$} $day -> y m d]} {
        return "Date $y-$m-$d"
    }
    return "Date $day"
}

proc fame_sorted {} {
    global fame

    set rows $fame(rows)
    set key [dict get {score rank name who race race class class} \
        [expr {$fame(sort) eq "winner" ? "score" : $fame(sort)}]]
    if {$key eq "rank"} {
        set rows [lsort -command [list fame_by rank] $rows]
    } else {
        set rows [lsort -command [list fame_by $key] $rows]
    }
    if {$fame(sort) eq "winner"} {
        set rows [lsort -command fame_winner_first $rows]
    }
    return $rows
}

proc fame_by {key a b} {
    set x [dict get $a $key]
    set y [dict get $b $key]
    if {$key eq "rank"} { return [expr {$x - $y}] }
    set c [string compare -nocase $x $y]
    return [expr {$c ? $c : [dict get $a rank] - [dict get $b rank]}]
}

# A winner died of old age; lsort is stable, so the order within each stays.
proc fame_winner_first {a b} {
    expr {([dict get $a how] ne "Ripe Old Age") - ([dict get $b how] ne "Ripe Old Age")}
}

proc fame_refresh {} {
    global fame

    if {![winfo exists .fame]} return
    if {[catch {angband_scores} fame(rows)]} { set fame(rows) {} }

    set t .fame.f.list.t
    $t configure -state normal
    $t delete 1.0 end
    set fame(entries) {}
    set n 0
    foreach row [fame_sorted] {
        set tags [list e$n [expr {[dict get $row you] ? "you" : "dead"}]]
        lassign [fame_lines $row] first second third
        if {$n} { $t insert end "\n" $tags }
        $t insert end "$first\n$second" $tags
        if {[record_for $row] ne ""} {
            $t insert end "    " $tags "More..." [concat $tags more]
        }
        $t insert end "\n$third\n" $tags
        lappend fame(entries) $row
        incr n
    }
    if {!$n} {
        $t insert end "\n    Nobody has made the Hall of Fame yet.\n" dead
    }
    $t configure -state disabled

    # Where the character being played stands, if anywhere.
    set you [lsearch -exact [lmap r $fame(entries) {dict get $r you}] 1]
    if {$you >= 0} {
        fame_pick $you
        $t see e$you.last
        $t see e$you.first
    } else {
        fame_pick -1
    }
}

# An entry chosen: highlighted, and the status bar says what is kept for it.
proc fame_pick {n} {
    global fame

    set t .fame.f.list.t
    $t tag remove picked 1.0 end
    set fame(picked) $n
    if {$n < 0 || $n >= [llength $fame(entries)]} {
        set fame(msg) ""
        .fame.mb.score entryconfigure "Show Record" -state disabled
        return
    }
    $t tag add picked e$n.first e$n.last
    set row [lindex $fame(entries) $n]
    set dir [record_for $row]
    if {$dir eq ""} {
        if {[dict get $row how] eq "nobody (yet!)"} {
            set fame(msg) "Where [dict get $row who] would stand, dying now."
        } else {
            set fame(msg) "No record available."
        }
        .fame.mb.score entryconfigure "Show Record" -state disabled
    } else {
        set fame(msg) "Record kept: [join [record_contents $dir] {, }]."
        .fame.mb.score entryconfigure "Show Record" -state normal
    }
}

proc fame_click {x y} {
    set t .fame.f.list.t
    foreach tag [$t tag names @$x,$y] {
        if {[regexp {^e(\d+)$} $tag -> n]} {
            fame_pick $n
            if {"more" in [$t tag names @$x,$y]} fame_record
            return
        }
    }
}

proc fame_record {} {
    global fame
    if {$fame(picked) < 0} { bell; return }
    set dir [record_for [lindex $fame(entries) $fame(picked)]]
    if {$dir eq ""} { bell; return }
    record_window $dir
}

proc fame_dump {} {
    set file [tk_getSaveFile -parent .fame -title "Dump Scores" \
        -initialdir [angband_path user] -initialfile scores.txt]
    if {$file eq ""} return
    if {[catch {
        set f [open $file w]
        fconfigure $f -encoding utf-8
        puts -nonewline $f [.fame.f.list.t get 1.0 end]
        close $f
    } err]} {
        set ::fame(msg) "Could not write the scores: $err"
        return
    }
    set ::fame(msg) "Scores written to [file tail $file]."
}

proc fame_window {} {
    global fame

    if {[winfo exists .fame]} {
        fame_refresh
        wm deiconify .fame
        raise .fame
        return
    }

    toplevel .fame
    wm title .fame "Zangband: Hall of Fame"
    wm minsize .fame 560 360

    set mb .fame.mb
    menu $mb
    .fame configure -menu $mb
    menu $mb.score -tearoff 0
    $mb add cascade -label Score -underline 0 -menu $mb.score
    $mb.score add command -label "Dump Scores..." -underline 0 -command fame_dump
    $mb.score add command -label "Show Record" -underline 5 -command fame_record
    $mb.score add separator
    $mb.score add command -label Close -underline 0 -command {destroy .fame}
    menu $mb.sort -tearoff 0
    $mb add cascade -label Sort -underline 1 -menu $mb.sort
    foreach {label value} {Score score Name name Race race Class class Winner winner} {
        $mb.sort add radiobutton -label $label -variable fame(sort) \
            -value $value -command fame_refresh
    }

    set w .fame.f
    ttk::frame $w
    pack $w -fill both -expand 1

    ttk::label $w.status -textvariable fame(msg) -font {Helvetica 12} \
        -relief sunken -borderwidth 1 -anchor w -padding {4 1}
    pack $w.status -side bottom -fill x -padx 2 -pady 2

    set f $w.list
    ttk::frame $f
    pack $f -fill both -expand 1 -padx 10 -pady 10
    text $f.t -width 78 -height 24 -font termfont -wrap none \
        -background black -foreground white -relief sunken -borderwidth 2 \
        -highlightthickness 0 -cursor {} -yscrollcommand [list $f.sb set] \
        -state disabled
    ttk::scrollbar $f.sb -orient vertical -command [list $f.t yview]
    pack $f.sb -side right -fill y
    pack $f.t -side left -fill both -expand 1
    $f.t tag configure you -foreground #60c0ff
    $f.t tag configure picked -background #303060
    $f.t tag configure more -underline 1 -foreground #ffff60
    $f.t tag lower picked
    # Read only: the window's own bindings, the wheel and the click.
    bindtags $f.t [list $f.t .fame all]
    bind $f.t <ButtonPress-1> { fame_click %x %y }
    bind $f.t <Double-ButtonPress-1> { fame_click %x %y ; fame_record }
    bind $f.t <MouseWheel> { tk::MouseWheel %W y %D -40.0 }
    $f.t tag bind more <Enter> { %W configure -cursor hand2 }
    $f.t tag bind more <Leave> { %W configure -cursor {} }

    bind .fame <Escape> { destroy .fame }
    bind .fame <Up> {
        if {$fame(picked) > 0} { fame_pick [expr {$fame(picked) - 1}] ; .fame.f.list.t see e$fame(picked).first }
    }
    bind .fame <Down> {
        if {$fame(picked) < [llength $fame(entries)] - 1} {
            fame_pick [expr {$fame(picked) + 1}] ; .fame.f.list.t see e$fame(picked).last
        }
    }
    bind .fame <Return> fame_record

    fame_refresh
}
