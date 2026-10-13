# Character portraits.
#
# T9: the Character window, the tombstone and the Character Record show a
# portrait of the character where they used to show its map tile.  Every race,
# every class, a man and a woman: 784, made by scripts/portraits/build-portraits.py
# from the text grids in scripts/portraits/art.py into one sheet,
# lib/tcl/portraits/portraits.png, with an index beside it.  The figures are on
# nothing and each class's backdrop is a cell of its own, so a window can set
# the figure on its backdrop or, like the tombstone, on nothing at all.
#
# The game has no sex for a character -- 4.2 took it out -- and a portrait
# needs one.  It is taken from the age, height and weight the character was
# born with: random, as birth rolls them, and never changed afterwards, so it is
# the same portrait every time and nothing has to be saved for it.

set portrait(dir) [file join [file dirname [info script]] portraits]

# Read the index and load the sheet, once.  0 if either is missing, and then
# the windows fall back on the character's map tile.
proc portrait_load {} {
    global portrait

    if {[info exists portrait(size)]} { return 1 }
    if {[catch {open [file join $portrait(dir) portraits.txt] r} f]} { return 0 }
    fconfigure $f -encoding utf-8
    set races {}
    set classes {}
    set size 0
    foreach line [split [read $f] \n] {
        lassign [split $line \t] key value
        switch -- $key {
            size  { set size $value }
            race  { lappend races $value }
            class { lappend classes $value }
        }
    }
    close $f
    if {!$size || ![llength $races] || ![llength $classes]} { return 0 }
    if {"portrait_sheet" ni [image names] && [catch {
        image create photo portrait_sheet \
            -file [file join $portrait(dir) portraits.png]
    }]} {
        return 0
    }
    set portrait(races) $races
    set portrait(classes) $classes
    set portrait(size) $size
    return 1
}

# M or F, from what birth rolled.
proc portrait_sex {} {
    if {[catch {
        expr {([angband_player age] + [angband_player height]
            + [angband_player weight]) % 2}
    } odd]} {
        return M
    }
    return [expr {$odd ? "F" : "M"}]
}

# The character's portrait, into a photo: at the largest whole multiple of
# its size that fits in size pixels, on its class's backdrop or, with ground
# 0, on nothing.  1 if there was one to draw.
proc portrait_put {photo size {ground 1}} {
    if {[catch {angband_player race} race]
            || [catch {angband_player class} class]} {
        return 0
    }
    return [portrait_put_for $photo $size $race $class [portrait_sex] $ground]
}

# Any race and class's, as a man (M) or a woman (F) -- for the Birth window,
# which shows the one under the cursor before it is chosen.
proc portrait_put_for {photo size race class sex {ground 1}} {
    global portrait

    if {![portrait_load]} { return 0 }
    set col [lsearch -exact -nocase $portrait(races) $race]
    set row [lsearch -exact -nocase $portrait(classes) $class]
    if {$col < 0 || $row < 0} { return 0 }

    set s $portrait(size)
    set zoom [expr {max(1, $size / $s)}]
    set fx [expr {$col * $s}]
    set fy [expr {(2 * $row + ($sex eq "F")) * $s}]

    $photo blank
    $photo configure -width [expr {$s * $zoom}] -height [expr {$s * $zoom}]
    if {$ground} {
        set bx [expr {$row * $s}]
        set by [expr {2 * [llength $portrait(classes)] * $s}]
        $photo copy portrait_sheet -from $bx $by [expr {$bx + $s}] \
            [expr {$by + $s}] -zoom $zoom -compositingrule set
    }
    $photo copy portrait_sheet -from $fx $fy [expr {$fx + $s}] \
        [expr {$fy + $s}] -zoom $zoom
    return 1
}

# The portrait, or the character's map tile where there is no portrait to
# draw -- a race or class added to the game before its art.  1 if either was.
proc portrait_draw {photo size {ground 1}} {
    if {[portrait_put $photo $size $ground]} { return 1 }
    # A portrait drawn into the photo before fixed its size; the tile sizes it
    # afresh.
    $photo configure -width 0 -height 0
    return [expr {![catch {angband_tile player $photo $size} ok] && $ok}]
}
