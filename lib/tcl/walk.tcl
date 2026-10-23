# Clicking on the map: interface.html's first mouse rules.
#
# A click takes one step the way of the pointer: the eighth of the compass it
# is in, seen from the character, wherever on the map it is.  Shift and a click
# runs that way.  A double click goes to the square itself by the game's own
# pathfinder, from wherever the first click's step left the character.  A click
# while running, resting or repeating a command stops it, and one at -more-
# goes on to the next message, as a key would.
#
# angband_walk does all of it, and nothing in a prompt or with the option
# "Allow mouse clicks to move the player" off.  When there is no way to the
# square at all, the line under the map says so until the pointer moves.

proc walk_click {x y how} {
    set c [look_canvas]
    if {[angband_walk [$c canvasx $x] [$c canvasy $y] $how] eq "nowhere"} {
        look_say "You know of no way there."
    }
}

bind [look_canvas] <ButtonPress-1> {+ walk_click %x %y step}
bind [look_canvas] <Shift-ButtonPress-1> {+ walk_click %x %y run}
bind [look_canvas] <Double-ButtonPress-1> {+ walk_click %x %y travel}
