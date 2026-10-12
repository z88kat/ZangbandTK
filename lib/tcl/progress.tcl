# The Progress strip: hit points, spell points, food and experience as bars.
#
# The original's Progress window, under the overhead view as it sat under the
# original's Micro Map: the things a player watches most, each a label, the
# number in the colour the game's own display gives it, and a bar.  Experience
# was not one of the original's three.  Its bar is the way through the current
# level, and clicking it switches the number between the total and what the
# next level still wants, which is what the original's Misc window did with its
# EXP label.
#
# What the numbers are and what colour they are is angband_player meters, so
# the game decides when hit points are low or a character is hungry -- the
# same colours as the sidebar and the status line.  Not a term, because a term
# cannot draw a bar: Tk's own labels and progress bars, on the black of the
# panes around it (decision 18).

set progress(exp) total
set progress(pending) 0

# {meter label bar-colour}: the original's red, blue and dark red, and the
# Character window's gold for experience.
set progress(meters) {
    hp   HP  #d03030
    sp   SP  #3050d0
    food FD  #a04848
    exp  EXP #c09020
}

proc progress_frame {} { return .pw.top.side.progress }

proc progress_build {} {
    global progress

    set f [progress_frame]
    foreach {meter label colour} $progress(meters) {
        set style Progress$meter.Horizontal.TProgressbar
        ttk::style configure $style -background $colour -troughcolor #c0c0c0 \
            -borderwidth 1
        label $f.l$meter -text $label -font termfont -anchor w \
            -background black -foreground white
        label $f.v$meter -font termfont -anchor e -width 8 \
            -background black -foreground white
        ttk::progressbar $f.b$meter -style $style -orient horizontal \
            -mode determinate -length 80
        grid $f.l$meter $f.v$meter $f.b$meter -sticky ew -padx {6 0} -pady 1
        grid configure $f.b$meter -padx 6
    }
    grid columnconfigure $f 2 -weight 1
    # The rows stay together at the top; a taller strip is room below them.
    grid rowconfigure $f 4 -weight 1

    # The experience number is the clickable one, as it was in the original.
    foreach w [list $f.lexp $f.vexp $f.bexp] {
        $w configure -cursor hand2
        bind $w <ButtonPress-1> progress_toggle_exp
    }
}

# Total experience, or what the next level still wants.
proc progress_toggle_exp {} {
    global progress
    set progress(exp) [expr {$progress(exp) eq "total" ? "next" : "total"}]
    progress_refresh
}

# Read again, at most once each time the event loop is idle: the refresh event
# comes every player turn, and resting takes hundreds of those a second.
proc progress_schedule {} {
    global progress
    if {$progress(pending)} return
    set progress(pending) 1
    after idle progress_refresh
}

proc progress_refresh {} {
    global progress

    set progress(pending) 0
    set f [progress_frame]
    if {![winfo exists $f.lhp]} return

    if {[catch {angband_player meters} meters]} {
        foreach {meter label colour} $progress(meters) {
            $f.v$meter configure -text ""
            $f.b$meter configure -value 0
        }
        return
    }

    foreach row $meters {
        lassign $row meter value cur max colour
        if {$meter eq "exp"} {
            # The sidebar's words: NXT for what is wanted, and in lower case
            # while experience is drained.  At the top level there is no next.
            set next [angband_player exp_to_advance]
            set drained [expr {[angband_player exp] < [angband_player max_exp]}]
            set word EXP
            if {$progress(exp) eq "next" && $max > 0} {
                set value $next
                set word NXT
            }
            $f.lexp configure -text [expr {$drained ? [string totitle $word] : $word}]
        }
        $f.v$meter configure -text $value -foreground $colour
        $f.b$meter configure -maximum [expr {max(1, $max)}] \
            -value [expr {max(0, min($cur, $max))}]
    }
}

progress_build
progress_refresh

foreach event {HP MANA EXPERIENCE PLAYERLEVEL STATUS REFRESH
        ENTER_GAME LEAVE_BIRTH ENTER_WORLD} {
    bind . <<Angband_$event>> {+ progress_schedule }
}
