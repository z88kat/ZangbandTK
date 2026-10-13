# Character Records.
#
# T9 (OBS-46): the original's memorial.  When a character's game is over, the
# Character Record dialog offers to keep its character dump, its message log
# and a photo of the last thing it saw, and the Hall of Fame shows them against
# its score -- "More..." beside the entry, and a window to read them in.  4.2
# keeps nothing of the kind: a score is a line in a table.
#
# A record is a folder under records in the player's own folder: record.txt,
# the score's fields as the game has them (angband_death entry), and whichever
# of dump.txt, messages.txt and photo.png were kept.  It is matched back to its
# score by who, the turn, the points and how the character died, which between
# them name one game.  Only a game that made the table is offered a record:
# without a score there is nowhere to show it.

set record(dump) [settings_get record,dump 1]
set record(messages) [settings_get record,messages 1]
set record(photo) [settings_get record,photo 1]
set record(keys) {who turns pts how}

proc record_root {} {
    return [file join [angband_path user] records]
}

# Every record kept, as {folder fields ...}; read again only after one is
# written, since the Hall of Fame asks once for every entry it shows.
proc record_listing {} {
    global record

    if {[info exists record(listing)]} { return $record(listing) }
    set record(listing) {}
    foreach dir [glob -nocomplain -type d [file join [record_root] *]] {
        if {[catch {open [file join $dir record.txt] r} f]} continue
        fconfigure $f -encoding utf-8
        set fields [dict create]
        foreach line [split [read $f] \n] {
            if {[catch {llength $line} n] || $n != 2} continue
            dict set fields {*}$line
        }
        close $f
        lappend record(listing) $dir $fields
    }
    return $record(listing)
}

# The record kept for a score, or "".
proc record_for {row} {
    global record

    foreach {dir fields} [record_listing] {
        set match 1
        foreach key $record(keys) {
            if {![dict exists $fields $key] || ![dict exists $row $key]
                    || [dict get $fields $key] ne [dict get $row $key]} {
                set match 0
                break
            }
        }
        if {$match} { return $dir }
    }
    return ""
}

# What a record holds, in words.
proc record_contents {dir} {
    set have {}
    foreach {file words} {dump.txt "character dump" messages.txt "message log"
            photo.png photo} {
        if {[file exists [file join $dir $file]]} { lappend have $words }
    }
    return $have
}

# --- keeping one -----------------------------------------------------------------

# The Character Record dialog, as the game's over: what to keep, then kept.
# Returns once it is answered.
proc record_offer {} {
    global record

    if {[catch {angband_death entry} entry] || ![dict get $entry scored]} return
    if {[winfo exists .record]} { destroy .record }

    if {"record_last_sight" ni [image names]} { image create photo record_last_sight }
    set record(hasphoto) [expr {![catch {angband_death photo record_last_sight} ok]
        && $ok}]
    set record(answer) ""
    set parent [expr {[winfo exists .tomb] ? ".tomb" : "."}]

    toplevel .record
    wm title .record "Zangband: Character Record"
    wm transient .record $parent
    wm resizable .record 0 0
    wm protocol .record WM_DELETE_WINDOW { set record(answer) cancel }

    set w .record.f
    ttk::frame $w -padding 10
    pack $w -fill both -expand 1

    # The character's portrait beside the question, as the original had its
    # icon.
    ttk::frame $w.top
    pack $w.top -fill x
    if {"record_tile" ni [image names]} { image create photo record_tile }
    if {[portrait_draw record_tile 32]} {
        label $w.top.tile -image record_tile -background black \
            -relief sunken -borderwidth 1
        pack $w.top.tile -side left -anchor n -padx {0 10}
    }
    ttk::label $w.top.q -wraplength 300 -justify left -text "Choose the\
        information you wish to be kept for your character. The information\
        can be viewed in the Hall of Fame."
    pack $w.top.q -side left -anchor n

    ttk::labelframe $w.opts -text Options -padding 8
    pack $w.opts -fill x -pady 10
    ttk::checkbutton $w.opts.dump -text "Character dump" -variable record(dump)
    ttk::checkbutton $w.opts.messages -text "Message log" \
        -variable record(messages)
    ttk::checkbutton $w.opts.photo -variable record(photo) \
        -text [expr {$record(hasphoto) ? "Photo"
            : "Photo (only of a game played with tiles)"}]
    if {!$record(hasphoto)} { $w.opts.photo state disabled }
    pack $w.opts.dump $w.opts.messages $w.opts.photo -anchor w -pady 1

    ttk::frame $w.btns
    pack $w.btns -fill x
    ttk::button $w.btns.cancel -text Cancel -width 10 \
        -command { set record(answer) cancel }
    ttk::button $w.btns.ok -text OK -width 10 -default active \
        -command { set record(answer) ok }
    pack $w.btns.cancel -side right
    pack $w.btns.ok -side right -padx 6

    bind .record <Return> { set record(answer) ok }
    bind .record <Escape> { set record(answer) cancel }

    focus $w.btns.ok
    catch { grab .record }
    vwait record(answer)
    catch { grab release .record }
    destroy .record

    if {$record(answer) ne "ok"} return
    foreach part {dump messages photo} {
        settings_set record,$part $record($part)
    }
    if {[catch {record_keep $entry} err]} {
        tk_messageBox -parent $parent -icon error -title "Character Record" \
            -message "The record could not be kept." -detail $err
    }
}

proc record_keep {entry} {
    global record

    set who [regsub -all {[^A-Za-z0-9]+} [dict get $entry who] _]
    set dir [file join [record_root] "$who-[dict get $entry turns]"]
    file mkdir $dir

    set f [open [file join $dir record.txt] w]
    fconfigure $f -encoding utf-8
    puts $f "# ZangbandTK Character Record: the score it belongs to."
    dict for {key value} $entry {
        if {$key ni {rank you scored}} { puts $f [list $key $value] }
    }
    close $f

    if {$record(dump)} {
        angband_dump write [file join $dir dump.txt]
    }
    if {$record(messages)} {
        set f [open [file join $dir messages.txt] w]
        fconfigure $f -encoding utf-8
        puts $f [join [angband_messages] \n]
        close $f
    }
    if {$record(photo) && $record(hasphoto)} {
        record_last_sight write [file join $dir photo.png] -format png
    }
    unset -nocomplain record(listing)
}

# --- reading one -----------------------------------------------------------------

proc record_window {dir} {
    global record

    if {[winfo exists .recordview]} { destroy .recordview }
    set fields [dict create]
    foreach {d f} [record_listing] { if {$d eq $dir} { set fields $f } }

    toplevel .recordview
    set who [expr {[dict exists $fields who] ? [dict get $fields who] : "?"}]
    wm title .recordview "Zangband: Character Record of $who"
    wm minsize .recordview 640 480

    set w .recordview.f
    ttk::frame $w -padding 10
    pack $w -fill both -expand 1

    ttk::frame $w.btns
    pack $w.btns -side bottom -fill x -pady {10 0}
    ttk::button $w.btns.close -text Close -underline 0 -width 10 \
        -command { destroy .recordview }
    pack $w.btns.close -side right

    set nb $w.nb
    ttk::notebook $nb
    pack $nb -fill both -expand 1
    foreach {file title} {dump.txt Character messages.txt Messages} {
        set path [file join $dir $file]
        if {![file exists $path]} continue
        set p [ttk::frame $nb.[file rootname $file]]
        $nb add $p -text $title
        text $p.t -font termfont -wrap none -background black \
            -foreground white -relief sunken -borderwidth 2 \
            -highlightthickness 0 -yscrollcommand [list $p.sb set]
        ttk::scrollbar $p.sb -orient vertical -command [list $p.t yview]
        pack $p.sb -side right -fill y
        pack $p.t -side left -fill both -expand 1
        set f [open $path r]
        fconfigure $f -encoding utf-8
        $p.t insert end [read $f]
        close $f
        $p.t configure -state disabled
        # The log is read from its end: the last messages are the death.
        if {$file eq "messages.txt"} { $p.t see end }
    }
    set path [file join $dir photo.png]
    if {[file exists $path]} {
        set p [ttk::frame $nb.photo]
        $nb add $p -text Photo
        if {"record_view_photo" in [image names]} { image delete record_view_photo }
        image create photo record_view_photo -file $path
        canvas $p.c -background black -highlightthickness 0 \
            -xscrollcommand [list $p.sx set] -yscrollcommand [list $p.sy set] \
            -scrollregion [list 0 0 [image width record_view_photo] \
                [image height record_view_photo]]
        ttk::scrollbar $p.sx -orient horizontal -command [list $p.c xview]
        ttk::scrollbar $p.sy -orient vertical -command [list $p.c yview]
        $p.c create image 0 0 -image record_view_photo -anchor nw
        grid $p.c -row 0 -column 0 -sticky nsew
        grid $p.sy -row 0 -column 1 -sticky ns
        grid $p.sx -row 1 -column 0 -sticky ew
        grid rowconfigure $p 0 -weight 1
        grid columnconfigure $p 0 -weight 1
    }

    bind .recordview <Escape> { destroy .recordview }
    bind .recordview <F1> {
        help_window [help_first dungeon#upon-death-and-dying dungeon] ; break
    }
    accel_bind .recordview c { destroy .recordview ; break }
}
