# Character portraits.
#
# T9: the Character window, the tombstone and the Character Record show a
# portrait of the character where they used to show its map tile.  Every race,
# every class, a man and a woman: 784, drawn in November 2026 and made by
# scripts/portraits/build-portraits.py into lib/tcl/portraits/portraits.png,
# with an index beside it.  The figures are on nothing and each class's
# backdrop is a cell of its own, so a window can set the figure on its backdrop
# or, like the tombstone, on nothing at all.
#
# They are drawn at 64, the size the Character and Birth windows show them.
# The Record and the tombstone show them smaller, and zooming only goes up, so
# the index names a second sheet of the same layout at 32, shrunk from the
# first.  A window gets whichever sheet suits the size it asks for.
#
# The game has no sex for a character -- 4.2 took it out -- and a portrait
# needs one.  It is taken from the age, height and weight the character was
# born with: random, as birth rolls them, and never changed afterwards, so it is
# the same portrait every time and nothing has to be saved for it.

set portrait(dir) [file join [file dirname [info script]] portraits]

# Read the index, once, and check the sheets are there.  0 if any is missing,
# and then the windows fall back on the character's map tile.
#
# A sheet is not loaded whole.  Tk keeps the outline of every opaque pixel in
# a photo, and for 784 figures on nothing that took 2.6 seconds on a Mac, and
# about five on a Linux VM, all of it spent before the Birth window could open:
# New Character sat there doing nothing.  At 64 it takes 17 seconds.  A cell
# read on its own, by portrait_cell, takes about 50 ms from the 64 sheet and 15
# from the 32 on a Mac, decoding the file included, and is kept once read.
proc portrait_load {} {
    global portrait

    if {[info exists portrait(size)]} { return 1 }
    if {[catch {open [file join $portrait(dir) portraits.txt] r} f]} { return 0 }
    fconfigure $f -encoding utf-8
    set races {}
    set classes {}
    set size 0
    set sheets {}
    foreach line [split [read $f] \n] {
        set fields [split $line \t]
        lassign $fields key value
        switch -- $key {
            size  { set size $value }
            sheet { lappend sheets [lrange $fields 1 2] }
            race  { lappend races $value }
            class { lappend classes $value }
        }
    }
    close $f
    if {!$size || ![llength $races] || ![llength $classes]} { return 0 }
    lappend sheets [list $size portraits.png]
    foreach sheet $sheets {
        lassign $sheet s name
        if {![string is integer -strict $s] || $s <= 0
                || ![file readable [file join $portrait(dir) $name]]} {
            return 0
        }
    }
    set portrait(races) $races
    set portrait(classes) $classes
    set portrait(sheets) [lsort -integer -index 0 $sheets]
    set portrait(size) $size
    return 1
}

# The sheet to draw a portrait size pixels across from, as {size file}: the
# largest that fits, or the smallest if none does.
proc portrait_sheet {size} {
    global portrait

    set best [lindex $portrait(sheets) 0]
    foreach sheet $portrait(sheets) {
        if {[lindex $sheet 0] <= $size} { set best $sheet }
    }
    return $best
}

# The cell of a sheet whose top left is x, y, as a photo of its own: read the
# first time it is wanted and kept, so a window that redraws, or the Birth
# window going up and down the list, reads each one once.  "" if it can't be.
proc portrait_cell {sheet x y} {
    global portrait

    lassign $sheet s file
    set name portrait_cell_${s}_${x}_$y
    if {$name in [image names]} { return $name }
    image create photo $name
    if {[catch {
        $name read [file join $portrait(dir) $file] -format png \
            -from $x $y [expr {$x + $s}] [expr {$y + $s}]
    }]} {
        image delete $name
        return ""
    }
    return $name
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
# its sheet's size that fits in size pixels, on its class's backdrop or, with
# ground 0, on nothing.  1 if there was one to draw.
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

    set sheet [portrait_sheet $size]
    set s [lindex $sheet 0]
    set zoom [expr {max(1, $size / $s)}]
    set figure [portrait_cell $sheet [expr {$col * $s}] \
        [expr {(2 * $row + ($sex eq "F")) * $s}]]
    set backdrop ""
    if {$ground} {
        set backdrop [portrait_cell $sheet [expr {$row * $s}] \
            [expr {2 * [llength $portrait(classes)] * $s}]]
    }
    if {$figure eq "" || ($ground && $backdrop eq "")} { return 0 }

    $photo blank
    $photo configure -width [expr {$s * $zoom}] -height [expr {$s * $zoom}]
    if {$ground} {
        $photo copy $backdrop -zoom $zoom -compositingrule set
    }
    $photo copy $figure -zoom $zoom
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
