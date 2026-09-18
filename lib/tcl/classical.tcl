# The Classical design system, in Tk.
#
# Every window this front end opens is drawn with these: the colours, the type
# and the six components below.  It exists so that the character sheet and the
# knowledge browsers and whatever comes after them look like one program rather
# than like six people's afternoons.
#
# The values are from .claude/plans/design_handoff_character_sheet, which is
# authoritative.  Where Tk cannot do what the design asks, the compromise is
# written down beside it rather than left for the next person to rediscover.
#
# Classic Tk widgets throughout, not Ttk.  Ttk widgets are themed by the
# platform, which is the right default and exactly wrong here: this design
# specifies every colour, and fighting a theme for control of a background is
# more work than not having one.  Ttk stays for anything that should look like
# the operating system; this is for anything that should look like the game.

namespace eval classical {}

# --- colour -----------------------------------------------------------------
#
# The neutral ramp is warm grey and the accent is old gold.  Body text on the
# accent always uses 700: the bare accent is for strokes and large type only,
# and reads as illegible mustard at 13px.

array set classical::colour {
    bg          #f3f2f2
    surface     #eae9e9
    text        #201f1d
    accent      #b68235
    divider     #cbc9c8

    neutral-100 #f8f4f4
    neutral-200 #eae7e7
    neutral-300 #d7d3d3
    neutral-400 #bab6b6
    neutral-500 #9b9797
    neutral-600 #7d7979
    neutral-700 #605d5d
    neutral-800 #444141
    neutral-900 #2d2b2b

    accent-100  #fff3e4
    accent-200  #ffe3bf
    accent-300  #facb8d
    accent-400  #e1ad66
    accent-500  #c28d41
    accent-600  #a06f24
    accent-700  #7d5411
    accent-800  #5a3b0a
    accent-900  #3a270d
}

# The design's divider is rgba(32,31,29,.16) over #f3f2f2.  Tk has no alpha, so
# it is flattened once, here, rather than guessed at in six places.
proc classical::c {name} {
    variable colour
    return $colour($name)
}

# --- spacing ----------------------------------------------------------------
#
# A 1.15x density scale.  Named rather than sprinkled, so the rhythm survives
# somebody adding a row in a hurry.

array set classical::space {1 5  2 9  3 14  4 18  6 28  8 37}

proc classical::sp {n} {
    variable space
    return $space($n)
}

# --- type -------------------------------------------------------------------
#
# Three faces, all SIL Open Font Licence and all shipped in lib/fonts:
# Cormorant Garamond for headings, Lora for body, Courier Prime for the
# terminal register -- the sigil, kickers, numerals, history and footer.
#
# main-tcl.c registers them with the platform at startup, because Tk has no way
# to load a font file: it asks for a family by name and takes what it is given.
# When that fails -- another platform, a missing file -- these fall back, and a
# sheet set in Baskerville is a lesser thing than one set in Cormorant, not a
# broken one.
#
# Sizes are negative, which is Tk for "pixels".  The design's numbers are CSS
# pixels, so they transfer directly.

proc classical::pick {wanted} {
    set have [font families]
    foreach family $wanted {
        if {$family in $have} { return $family }
    }
    return [lindex $wanted end]
}

proc classical::init_fonts {} {
    variable face

    set face(head) [classical::pick {"Cormorant Garamond" "Baskerville" "Hoefler Text" \
                          "Georgia" "Times New Roman" Times}]
    set face(body) [classical::pick {"Lora" "Charter" "Georgia" "Times New Roman" Times}]
    set face(mono) [classical::pick {"Courier Prime" "Courier New" Courier Menlo}]

    # {name face size weight slant}
    foreach spec {
        {display    head -62 normal roman}
        {title      head -46 normal roman}
        {sigil      mono -46 normal roman}
        {subhead    body -17 normal italic}
        {value      head -20 normal roman}
        {strip      head -14 normal roman}
        {history    mono -14 normal roman}
        {section    head -13 bold   roman}
        {label      body -13 normal roman}
        {note       body -13 normal italic}
        {footer     mono -12 normal roman}
        {kicker     mono -11 normal roman}
        {numeral    mono -11 normal roman}
        {figure     mono -11 normal roman}
        {tiny       mono -10 normal roman}
    } {
        lassign $spec name which size weight slant
        if {[lsearch -exact [font names] cl_$name] >= 0} continue
        font create cl_$name -family $face($which) -size $size \
            -weight $weight -slant $slant
    }
}

proc classical::f {name} { return cl_$name }

# Letter-spacing has no Tk equivalent, and the design leans on it for every
# small uppercase label.  A thin space between characters is the approximation
# the handoff suggests; at .2em it is close, and without it the kickers set
# tight and lose the engraved look that is half the point.
proc classical::tracked {text} {
    return [join [split $text ""] " "]
}

# --- components -------------------------------------------------------------

# A hairline.  1px frames, never -relief: a groove or a ridge reads as 3D and
# this design has no depth in it anywhere.
proc classical::hairline {path {colour divider} {thickness 1}} {
    frame $path -height $thickness -bg [classical::c $colour] -bd 0 -highlightthickness 0
    return $path
}

# A window.
#
# The design draws a card floating on a desk, with a margin of ground around
# it, a rounded corner and a drop shadow.  None of the three survives the move
# to Tk, and they should not: the margin is a web page's way of saying "this is
# a sheet of paper", and a native window already says that with its own frame
# and title bar.  So the card fills the window, and what is left of the idea is
# the colour and the rule under the title strip.
#
# Returns the card, which is what everything else is packed into.
proc classical::window {path title} {
    toplevel $path -bg [classical::c bg]
    wm title $path $title

    set card $path.card
    frame $card -bg [classical::c bg]
    pack $card -fill both -expand 1

    return $card
}

# The title strip: a word on the left, a hairline across the middle, a word on
# the right.  The hairline is what makes it read as a printed masthead rather
# than as two labels that happen to share a row.
proc classical::titlestrip {parent left right} {
    set f $parent.strip
    frame $f -bg [classical::c neutral-100]
    pack $f -fill x

    set row $f.row
    frame $row -bg [classical::c neutral-100]
    pack $row -fill x -padx [classical::sp 4] -pady [classical::sp 2]

    label $row.l -text [classical::tracked $left] -font [classical::f kicker] \
        -bg [classical::c neutral-100] -fg [classical::c neutral-600]
    label $row.r -text [classical::tracked $right] -font [classical::f strip] \
        -bg [classical::c neutral-100] -fg [classical::c accent-700]
    classical::hairline $row.rule

    pack $row.l -side left
    pack $row.r -side right
    pack $row.rule -side left -fill x -expand 1 -padx [classical::sp 3]

    pack [classical::hairline $f.under neutral-400] -fill x
    return $f
}

# A section head: a gold-ruled title with a roman numeral flush right.  The
# numerals are the design's way of saying "read these in order", so a caller
# that invents its own numbering is missing the point.
proc classical::sectionhead {parent name title numeral} {
    set f $parent.h$name
    frame $f -bg [classical::c bg]

    set row $f.row
    frame $row -bg [classical::c bg]
    pack $row -fill x -pady {0 4}

    label $row.t -text $title -font [classical::f section] -bg [classical::c bg] -fg [classical::c accent-700]
    label $row.n -text $numeral -font [classical::f numeral] -bg [classical::c bg] -fg [classical::c neutral-600]
    pack $row.t -side left
    pack $row.n -side right

    pack [classical::hairline $f.rule accent] -fill x
    return $f
}

# One label-and-value row.  `muted` is the design's rule that a zero or absent
# value drops to neutral-500, so a sheet full of noughts reads as "none" rather
# than as data -- which is the difference between a character with no mana and
# a character whose mana the window failed to fetch.
proc classical::statrow {parent name label var {last 0}} {
    set f $parent.r$name
    frame $f -bg [classical::c bg]
    pack $f -fill x

    set row $f.row
    frame $row -bg [classical::c bg]
    pack $row -fill x -pady 4

    label $row.l -text $label -font [classical::f label] -bg [classical::c bg] -fg [classical::c neutral-700] \
        -anchor w
    label $row.v -textvariable $var -font [classical::f value] -bg [classical::c bg] -fg [classical::c text] \
        -anchor e
    pack $row.l -side left
    pack $row.v -side right

    if {!$last} { pack [classical::hairline $f.rule] -fill x }
    return $row.v
}

# A meter: a label row over a 7px track.  A resource whose maximum is zero
# draws as an empty grey-bordered track and never as a full bar -- the rule is
# in the handoff because dividing by it is the obvious bug.
proc classical::meter {parent name label var} {
    set f $parent.m$name
    frame $f -bg [classical::c bg]

    set row $f.row
    frame $row -bg [classical::c bg]
    pack $row -fill x -pady {0 5}
    label $row.l -text [classical::tracked $label] -font [classical::f kicker] \
        -bg [classical::c bg] -fg [classical::c neutral-700] -anchor w
    label $row.v -textvariable $var -font [classical::f figure] \
        -bg [classical::c bg] -fg [classical::c text] -anchor e
    pack $row.l -side left
    pack $row.v -side right

    frame $f.track -height 7 -bg [classical::c bg] -highlightthickness 1 \
        -highlightbackground [classical::c neutral-400] -bd 0
    pack $f.track -fill x
    frame $f.track.fill -bg [classical::c accent-400] -bd 0 -highlightthickness 0

    return $f
}

proc classical::meter_set {f ratio} {
    if {$ratio <= 0} {
        place forget $f.track.fill
        $f.track configure -highlightbackground [classical::c neutral-400]
        return
    }
    if {$ratio > 1.0} { set ratio 1.0 }
    $f.track configure -highlightbackground [classical::c accent]
    place $f.track.fill -x 0 -y 0 -relwidth $ratio -relheight 1.0
}

# A read-only prose panel: the history, and anything else the game hands us as
# a paragraph.  A text widget because it has to wrap, disabled because a sheet
# you can type into is a sheet that lies.
proc classical::prose {parent name {height 4}} {
    set f $parent.p$name
    frame $f -bg [classical::c neutral-100] -highlightthickness 1 \
        -highlightbackground [classical::c divider] -bd 0
    text $f.t -height $height -wrap word -relief flat -borderwidth 0 \
        -highlightthickness 0 -bg [classical::c neutral-100] -fg [classical::c neutral-800] \
        -font [classical::f history] -padx [classical::sp 4] -pady [classical::sp 4] -spacing1 2 -spacing3 6 \
        -cursor arrow
    pack $f.t -fill both -expand 1
    $f.t configure -state disabled
    return $f
}

proc classical::prose_set {f text} {
    $f.t configure -state normal
    $f.t delete 1.0 end
    # The engine wraps its own prose and doubles its spaces.  The panel wraps,
    # so the engine's breaks are noise; flatten them.
    $f.t insert end [regsub -all {\s+} [string trim $text] " "]
    $f.t configure -state disabled
}

# The footer: bracketed keys that are hints, not buttons, and a status flush
# right.  Bracketed keys take the accent; everything else is quiet.
proc classical::footer {parent items statusvar} {
    set f $parent.footer
    frame $f -bg [classical::c neutral-100]
    pack $f -side bottom -fill x
    pack [classical::hairline $f.over neutral-400] -fill x -side top

    set row $f.row
    frame $row -bg [classical::c neutral-100]
    pack $row -fill x -padx [classical::sp 4] -pady [classical::sp 3]

    set i 0
    foreach {key text} $items {
        label $row.k$i -text "\[$key\]" -font [classical::f footer] \
            -bg [classical::c neutral-100] -fg [classical::c accent-700]
        label $row.t$i -text $text -font [classical::f footer] \
            -bg [classical::c neutral-100] -fg [classical::c neutral-700]
        pack $row.k$i -side left
        pack $row.t$i -side left -padx [list 4 [classical::sp 3]]
        incr i
    }

    label $row.status -textvariable $statusvar -font [classical::f footer] \
        -bg [classical::c neutral-100] -fg [classical::c neutral-600]
    pack $row.status -side right

    return $f
}

# Reflow a row of equal columns as the window narrows: three, then two, then
# one.  The design asks for CSS auto-fit with a minimum track width, and this
# is the same rule said in Tk -- bound to <Configure>, deferred to idle so a
# drag does not re-grid on every pixel.
proc classical::columns {container children minwidth} {
    variable lastfit
    set n [llength $children]

    # The container's own width is meaningless until it has been mapped, and
    # the toplevel's is what actually decides the layout, so ask that.
    set w [winfo width [winfo toplevel $container]]
    if {$w <= 1} { set w [winfo reqwidth [winfo toplevel $container]] }

    set fit [expr {$w / $minwidth}]
    if {$fit < 1} { set fit 1 }
    if {$fit > $n} { set fit $n }

    # Nothing to do unless the answer changed, and this is not an optimisation.
    # Re-gridding inside a <Configure> handler resizes the container, which
    # fires <Configure> again -- so without this the window spends seconds
    # churning through idle handlers before it settles, which is exactly what
    # it looked like: an empty card, then the content some seconds later.
    if {[info exists lastfit($container)] && $lastfit($container) eq $fit} {
        return
    }
    set lastfit($container) $fit

    set i 0
    foreach child $children {
        grid $child -row [expr {$i / $fit}] -column [expr {$i % $fit}] \
            -sticky new -padx [expr {[classical::sp 6] / 2}] -pady [list 0 [classical::sp 4]]
        incr i
    }
    for {set col 0} {$col < $n} {incr col} {
        grid columnconfigure $container $col -weight [expr {$col < $fit}] \
            -uniform [expr {$col < $fit ? "cl" : ""}]
    }
}

proc classical::responsive {container children minwidth} {
    # Bound on the toplevel, not the container: it is the window being dragged
    # that decides how many columns fit, and a binding on the container hears
    # its own re-grid as well as the drag.
    bind [winfo toplevel $container] <Configure> [list \
        classical::reflow_soon $container $children $minwidth]
    classical::columns $container $children $minwidth
}

proc classical::reflow_soon {container children minwidth} {
    variable pending
    if {[info exists pending($container)]} return
    set pending($container) 1
    after idle [list classical::reflow_now $container $children $minwidth]
}

proc classical::reflow_now {container children minwidth} {
    variable pending
    variable lastfit
    unset -nocomplain pending($container)
    if {[winfo exists $container]} {
        classical::columns $container $children $minwidth
    } else {
        unset -nocomplain lastfit($container)
    }
}

classical::init_fonts
