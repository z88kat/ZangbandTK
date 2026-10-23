# Pointing at the map: what is there, in the line under it.
#
# The original's Main Window did this as the pointer moved -- interface.html's
# hover rule, "Look: describes the grid; over a monster, shows the health bar
# and recall".  Its <Motion> binding asked `angband cave examine` and put the
# answer in the status bar; here it is angband_look, which says what the look
# command would say of the square, and makes the Recall pane follow the
# pointer as it follows the look command.
#
# Only while the game is waiting for a command.  In a prompt, while running or
# resting, or with the pointer off the map, angband_look answers empty and the
# line is cleared.

set look(x) -1
set look(y) -1
set look(pending) 0

proc look_canvas {} { return .pw.top.map.c }
proc look_line {} { return .pw.top.map.look }

proc look_say {text} {
    set w [look_line]
    if {[$w cget -text] ne $text} { $w configure -text $text }
}

proc look_motion {x y} {
    global look
    set c [look_canvas]
    set look(x) [$c canvasx $x]
    set look(y) [$c canvasy $y]
    look_say [angband_look $look(x) $look(y) recall]
}

proc look_leave {} {
    global look
    set look(x) -1
    set look(y) -1
    look_say ""
}

# Things move under a pointer that is standing still: a monster walks off the
# square, the character steps and the map scrolls under it, or a turn ends a
# run.  So the line is asked for again whenever the game has drawn, once per
# burst of drawing, for the words only.  The Recall pane is left alone: it
# follows the pointer when the pointer moves, and a monster wandering under a
# mouse nobody is touching should not take it from whatever was being read.
proc look_again {} {
    global look
    if {$look(pending) || $look(x) < 0} return
    set look(pending) 1
    after idle look_again_now
}

proc look_again_now {} {
    global look
    set look(pending) 0
    if {$look(x) < 0} return
    look_say [angband_look $look(x) $look(y)]
}

bind [look_canvas] <Motion> {+ look_motion %x %y}
bind [look_canvas] <Leave> {+ look_leave}
bind . <<Angband_REFRESH>> {+ look_again}
