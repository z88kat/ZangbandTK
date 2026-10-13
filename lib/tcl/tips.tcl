# The Tips window.
#
# T9: the original's -- "Welcome to" over a tip at a time on pale yellow under
# "Did you know...", Back and Next, which tip of how many, and a box for
# whether it opens every time the game starts.  It opens once a session, when
# the first game is under way, and from Help > Tips; and it carries on where it
# left off, each time showing the tip after the last one seen.  Both are kept
# in the front end's settings.
#
# The tips are lib/tcl/tips.txt, a paragraph each.  The original's 32 are not
# carried over as they were: most of them name things this front end does
# differently or not at all -- the Micro Map's popup menus, the Setup button,
# a Recall window that grows under the pointer.  A tip has to be true of the
# front end and the game as they are, so the file changes when they do.
#
# Built from Tk's own widgets in the classic theme (decision 18).

set tips(file) [file join [file dirname [info script]] tips.txt]
set tips(list) {}
set tips(at) 0
set tips(of) ""
set tips(started) 0
set tips(show) [settings_get tips,show 1]

# One tip to a paragraph; lines starting with # are notes for whoever edits
# the file.
proc tips_read {} {
    global tips

    set tips(list) {}
    if {[catch {open $tips(file) r} f]} return
    fconfigure $f -encoding utf-8
    set para {}
    foreach line [split [read $f] \n] {
        set line [string trim $line]
        if {[string index $line 0] eq "#"} continue
        if {$line ne ""} {
            lappend para $line
        } elseif {[llength $para]} {
            lappend tips(list) [join $para " "]
            set para {}
        }
    }
    if {[llength $para]} { lappend tips(list) [join $para " "] }
    close $f
}

proc tips_show {at} {
    global tips

    set n [llength $tips(list)]
    set tips(at) [expr {(($at % $n) + $n) % $n}]
    set tips(of) "[expr {$tips(at) + 1}]/$n"
    settings_set tips,last $tips(at)

    set t .tips.f.tip
    $t delete 1.0 end
    $t insert end "Did you know...\n" head [lindex $tips(list) $tips(at)] body
}

proc tips_next {} { global tips; tips_show [expr {$tips(at) + 1}] }
proc tips_back {} { global tips; tips_show [expr {$tips(at) - 1}] }

proc tips_close {} {
    destroy .tips
    focus .
}

# Once a session, when the first game is under way -- as the original opened
# it once the main window was up.
proc tips_started {} {
    global tips

    if {$tips(started)} return
    set tips(started) 1
    if {$tips(show)} { after 500 tips_window }
}

proc tips_window {} {
    global tips

    if {[winfo exists .tips]} {
        wm deiconify .tips
        raise .tips
        focus .tips
        return
    }
    if {![llength $tips(list)]} tips_read
    if {![llength $tips(list)]} { bell; return }

    toplevel .tips
    wm title .tips "Zangband: Tips"
    wm transient .tips .
    wm protocol .tips WM_DELETE_WINDOW tips_close

    set w .tips.f
    ttk::frame $w -padding {10 6 10 0}
    pack $w -fill both -expand 1

    ttk::label $w.head -text "Welcome to ZangbandTK" -font {Times 24 bold}
    pack $w.head -anchor w -pady {0 6}

    # The tip, on the original's pale yellow.  Only the window's own bindings:
    # it is something to read, not to type in or select from.
    set t $w.tip
    text $t -width 52 -height 9 -wrap word -font {Helvetica 12} \
        -background #FFFFE0 -foreground black -relief sunken -borderwidth 1 \
        -highlightthickness 0 -cursor {} -takefocus 0 -padx 0 -pady 0
    $t tag configure head -font {Helvetica 12 bold} -lmargin1 20 \
        -spacing1 16 -spacing3 12
    $t tag configure body -lmargin1 20 -lmargin2 20 -rmargin 20 -spacing2 2
    bindtags $t [list $t .tips all]
    pack $t -fill both -expand 1

    ttk::frame $w.misc
    pack $w.misc -fill x -pady 6
    ttk::checkbutton $w.misc.show -variable tips(show) \
        -text "Show this window every time the game starts" \
        -command {settings_set tips,show $tips(show)}
    ttk::label $w.misc.of -textvariable tips(of)
    pack $w.misc.show -side left
    pack $w.misc.of -side right

    ttk::separator $w.sep -orient horizontal
    pack $w.sep -fill x

    set b $w.btns
    ttk::frame $b
    pack $b -fill x -pady 8
    ttk::button $b.close -text "Close" -width 10 -command tips_close
    ttk::button $b.next -text "Next >" -underline 0 -width 10 \
        -default active -command tips_next
    ttk::button $b.back -text "< Back" -underline 2 -width 10 \
        -command tips_back
    pack $b.close -side right
    pack $b.next -side right -padx 6
    pack $b.back -side right

    foreach key {Left b less} { bind .tips <KeyPress-$key> tips_back }
    foreach key {Right n greater Return} { bind .tips <KeyPress-$key> tips_next }
    bind .tips <Escape> tips_close
    bind .tips <F1> { help_window [help_first start] ; break }

    tips_show [expr {[settings_get tips,last -1] + 1}]
    focus .tips
}

bind . <<Angband_ENTER_GAME>> {+ tips_started }
