# The Display page: how the main window draws.
#
# T9: the front end's own options, on the Options window beside the game's --
# the tile set and its size, the fonts of the panes, and the colours -- kept in
# the front end's settings, so they are the same whichever character is played,
# and put back at the start of every session.  Every change shows at once in
# the main window, which is the only preview that cannot mislead.
#
# What each does to the game is the game's own mechanism: the tile size is
# tile_width and tile_height, which the game's other front ends multiply the
# same way; a pane's font is its cell size, so a font is a layout and changing
# one measures the panes again (angband_refont); and a palette is the game's
# colour table, which its pref files' color: lines change too.
#
# The palettes besides the game's are in palettes.txt, which says how they are
# checked.  A tile set keeps its own colours: they are in its sheet.

set display(dir) [file dirname [info script]]

# The fixed-width face the panes start in: the first of these the machine has.
# Menlo is the Mac's, Consolas Windows', DejaVu Sans Mono most Linux desktops'.
# Named, because Tk hands a family it does not have to the platform to choose
# for, and on Windows and X11 the platform chose a proportional one: the map in
# letters of different widths.  TkFixedFont, last, is Tk's own and always there.
proc display_face {} {
    set have [font families]
    foreach family {Menlo Consolas {DejaVu Sans Mono} {Liberation Mono}
            {Noto Sans Mono}} {
        if {$family in $have} { return $family }
    }
    return [font actual TkFixedFont -family]
}
set display(face) [display_face]

# The panes' fonts: what each is called, what it is for, and its default.
set display(fonts) [list \
    map      termfont    "Map"           [list $display(face) 13] \
    panes    panefont    "Text panes"    [list $display(face) 13] \
    overhead minimapfont "Overhead map"  [list $display(face) 7]]
set display(sizes) {
    map {8 32}  panes {8 32}  overhead {3 16}
}

# Small, Medium and Large.
set display(scales) {1 Small 2 Medium 3 Large}

# The front end's own colours: the health bar as the character is whole,
# hurt or in danger, and the other bars.  The game's palette keeps these.
set display(roles,game) {
    good #38a038  fair #c8a000  poor #c03030
    mana #3050d0  food #a04848  exp  #c09020
    flaghas   #0b5e0b  flaghastint   #d6ecd6
    flagimm   #063f06  flagimmtint   #a9d6a9
    flagtimed #1a3a96  flagtimedtint #dbe3f7
    flagbad   #9c1414  flagbadtint   #f5d6d6
    flagunk   #6b4f00  flagunktint   #f7ecc4
    flagnone  #9a9a9a  flagempty     #e4e4e4  flagrune #6e6e6e
}

# --- fonts ------------------------------------------------------------------------

# The fonts, as the settings have them.  Called by main.tcl before anything is
# measured from them, so the panes are made at the player's sizes.
proc display_fonts_create {} {
    global display
    foreach {key name label default} $display(fonts) {
        lassign [settings_get display,font,$key $default] family size
        lassign $default dfamily dsize
        if {![string is integer -strict $size]} { set size $dsize }
        lassign [dict get $display(sizes) $key] lo hi
        set size [expr {max($lo, min($hi, $size))}]
        # A family this machine does not have -- Menlo, in settings carried
        # over from a Mac -- is the default, for the reason display_face gives.
        if {$family eq "" || $family ni [font families]} { set family $dfamily }
        if {$name in [font names]} {
            font configure $name -family $family -size $size
        } else {
            font create $name -family $family -size $size
        }
    }
}

# The installed families whose letters are all one width.  A pane is a grid,
# so nothing else will do.  Asked once: a Mac has hundreds of families.
proc display_mono_families {} {
    global display
    if {[info exists display(mono)]} { return $display(mono) }
    set mono {}
    foreach family [lsort -dictionary -unique [font families]] {
        if {[string match {[.@]*} $family]} continue
        if {![catch {font metrics [list $family 12] -fixed} fixed] && $fixed} {
            lappend mono $family
        }
    }
    set display(mono) $mono
    return $mono
}

# A pane's font changed: measure the panes again, and remember it.
proc display_font {key family size} {
    global display angband
    set name ""
    foreach {k n label default} $display(fonts) {
        if {$k eq $key} { set name $n }
    }
    lassign [dict get $display(sizes) $key] lo hi
    if {![string is integer -strict $size] || $size < $lo || $size > $hi} {
        return -code error "a size from $lo to $hi"
    }
    font configure $name -family $family -size $size
    settings_set display,font,$key [list $family $size]
    catch angband_refont
    set angband(cellw) [font measure termfont "W"]
    set angband(cellh) [font metrics termfont -linespace]
    wm minsize . [expr {80 * $angband(cellw)}] [expr {24 * $angband(cellh)}]
    event generate . <<FontsChanged>> -when tail
}

# --- the windows' own text -------------------------------------------------------
#
# Everything in the windows that is not a pane -- the labels, lists and buttons,
# the Character window's sheet, the shops' prices, the manual in the Help
# window -- is one size on the Display page: the size of ordinary text in them,
# 13 by default, as macOS has it.  Every other size in the windows is that one
# scaled, so a heading written as 18 beside text of 13 stays half as large
# again whatever the setting.  No window names a size outright: it asks uifont
# for the size it wants at 13 and gets a named font, which changes with the
# setting and takes every widget and canvas item using it along.  Tk's own
# named fonts, which a widget with no font of its own uses, scale the same way,
# and a Treeview style registered with ui_treeview has its rows made tall
# enough again.

set display(uibase) 13
set display(uisize) 13
set display(uirange) {9 24}
set display(uistyles) {}

# A size as it is at the current setting, from the size it is at 13.
proc ui_scaled {size} {
    global display
    expr {max(6, round($size * $display(uisize) / double($display(uibase))))}
}

# The family a window's font asks for, as this machine draws it.
#
# On X11, Helvetica and Courier are names fontconfig resolves, and an Ubuntu
# desktop resolves them to Nimbus Sans and Nimbus Mono PS, Ghostscript's copies.
# Those keep far more room under the line than the real ones, and Tk centres
# the whole line, so the text in every button and field sat a few pixels high:
# measured in a button, 3 pixels at the natural size and 5 at twice it, and
# worse for Courier.  Liberation Sans and Mono are drawn to Arial's and Courier
# New's widths and sit within a pixel or two of the middle; DejaVu, which every
# desktop has, is the fallback.  The Mac has the real Helvetica, and Tk on
# Windows maps it to Arial, so only X11 is changed.
proc ui_family {family} {
    global display
    if {[tk windowingsystem] ne "x11"} { return $family }
    if {[info exists display(x11family,$family)]} {
        return $display(x11family,$family)
    }
    set use $family
    set choices [dict create \
        Helvetica {{Liberation Sans} {DejaVu Sans}} \
        Courier {{Liberation Mono} {DejaVu Sans Mono}}]
    if {[dict exists $choices $family]} {
        set have [font families]
        foreach f [dict get $choices $family] {
            if {$f in $have} { set use $f ; break }
        }
    }
    set display(x11family,$family) $use
    return $use
}

# A named font that follows the setting: ui_font_create name family size
# ?bold? ?italic?.  The Help window's fonts are made this way under their own
# names; uifont makes the rest.
proc ui_font_create {name family size args} {
    global display
    set spec [list -family [ui_family $family] -size [ui_scaled $size] \
        -weight [expr {"bold" in $args ? "bold" : "normal"}] \
        -slant [expr {"italic" in $args ? "italic" : "roman"}]]
    if {$name in [font names]} {
        font configure $name {*}$spec
    } else {
        font create $name {*}$spec
    }
    set display(ui,$name) [list $family $size $args]
    return $name
}

# The font for text of a size, at 13, in the windows: uifont ?family? size
# ?bold? ?italic?, Helvetica unless another family is named.  The same request
# is the same font, made once.
proc uifont {args} {
    global display
    set family Helvetica
    if {![string is integer -strict [lindex $args 0]]} {
        set args [lassign $args family]
    }
    set args [lassign $args size]
    set name [string tolower "ui-$family-$size[join [lmap a $args {string cat - $a}] {}]"]
    if {![info exists display(ui,$name)]} {
        ui_font_create $name $family $size {*}$args
    }
    return $name
}

# A Treeview style in a window's font, its rows tall enough for it -- now and
# whenever the size changes.
proc ui_treeview {style font args} {
    global display
    ttk::style configure $style -font $font \
        -rowheight [expr {[font metrics $font -linespace] + 6}] {*}$args
    dict set display(uistyles) $style $font
}

# Change the size of the windows' text, and keep it.
proc display_uisize {size} {
    global display
    lassign $display(uirange) lo hi
    if {![string is integer -strict $size] || $size < $lo || $size > $hi} {
        return -code error "a size from $lo to $hi"
    }
    display_uisize_apply $size
    settings_set display,font,windows $size
    event generate . <<WindowTextChanged>> -when tail
}

proc display_uisize_apply {size} {
    global display
    set display(uisize) $size
    foreach key [array names display ui,*] {
        set name [string range $key 3 end]
        lassign $display($key) family base styles
        ui_font_create $name $family $base {*}$styles
    }
    # Tk's own, from the sizes they had before anything here touched them.
    foreach f {TkDefaultFont TkTextFont TkHeadingFont TkCaptionFont
            TkSmallCaptionFont TkIconFont TkTooltipFont TkFixedFont} {
        if {$f ni [font names]} continue
        if {![info exists display(tk,$f)]} {
            set display(tk,$f) [font configure $f -size]
        }
        set was $display(tk,$f)
        set now [ui_scaled [expr {abs($was)}]]
        font configure $f -size [expr {$was < 0 ? -$now : $now}]
    }
    dict for {style font} $display(uistyles) {
        ttk::style configure $style \
            -rowheight [expr {[font metrics $font -linespace] + 6}]
    }
}

# The size kept last time, before any window is built.
proc display_uisize_start {} {
    global display
    lassign $display(uirange) lo hi
    set size [settings_get display,font,windows $display(uibase)]
    if {![string is integer -strict $size]} { set size $display(uibase) }
    display_uisize_apply [expr {max($lo, min($hi, $size))}]
}

# --- tiles --------------------------------------------------------------------------

proc display_tilescale {n} {
    if {[catch {angband_tilescale $n} err]} { return -code error $err }
    settings_set display,tilescale $n
}

# The tile scale for someone who has never chosen one: the one Tk is drawing
# the text at.  A Linux desktop scaled to twice the size has Tk double every
# font, and the map, left at 1, came up half the size of its own text.  1 on
# the Mac and anywhere at its natural size, where ScalingPct is 100; rounded to
# the nearest whole step, as the SDL2 front end's scale is, so 150% is 2.
proc display_tilescale_default {} {
    if {[catch {::tk::ScalingPct} pct]} { return 1 }
    return [expr {max(1, min(4, ($pct + 50) / 100))}]
}

# --- colours -------------------------------------------------------------------------

# The palettes in palettes.txt: key -> {label colours roles}.
proc display_palettes {} {
    global display
    if {[info exists display(palettes)]} { return $display(palettes) }
    set palettes [dict create]
    if {![catch {open [file join $display(dir) palettes.txt] r} f]} {
        fconfigure $f -encoding utf-8
        set key ""
        foreach line [split [read $f] \n] {
            if {$line eq "" || [string index $line 0] eq "#"} continue
            set fields [split $line \t]
            switch -- [lindex $fields 0] {
                palette {
                    set key [lindex $fields 1]
                    dict set palettes $key [list [lindex $fields 2] {} {}]
                }
                colour - role {
                    if {$key eq ""} continue
                    set i [expr {[lindex $fields 0] eq "colour" ? 1 : 2}]
                    set entry [dict get $palettes $key]
                    lset entry $i [list {*}[lindex $entry $i] \
                        [lindex $fields 1] [lindex $fields 2]]
                    dict set palettes $key $entry
                }
            }
        }
        close $f
    }
    set display(palettes) $palettes
    return $palettes
}

# The palettes to offer, as {key label}: the game's first, then the file's,
# then the player's own if there is one.
proc display_palette_choices {} {
    set choices [list game "The game's"]
    dict for {key entry} [display_palettes] {
        lappend choices $key [lindex $entry 0]
    }
    lappend choices custom "My own"
    return $choices
}

# The game's colours as they were before any palette was applied: taken once,
# at the start, so "The game's" can always be gone back to.
proc display_palette_snapshot {} {
    global display
    if {[info exists display(game)]} return
    set display(game) {}
    if {![catch {angband_palette} rows]} {
        foreach row $rows { lappend display(game) {*}$row }
    }
}

# A palette's colours, every one of the game's named colours given a value.
proc display_palette_colours {key} {
    global display
    display_palette_snapshot
    set colours $display(game)
    if {$key eq "custom"} {
        set colours [dict merge $colours [settings_get display,custom {}]]
    } elseif {[dict exists [display_palettes] $key]} {
        set colours [dict merge $colours \
            [lindex [dict get [display_palettes] $key] 1]]
    }
    return $colours
}

# Use a palette, and remember it.  Returns what else it changed, in words,
# or "".
#
# A palette cannot reach a tile's colours, which are in the sheet -- so the
# colour-blind palette, with the map in Neon, chooses Neon's colour-blind sheet
# with it: the same tiles, in colours chosen for the purpose (mode 8, made by
# build-neon.py from palette.txt's cbhue lines).  Leaving that palette for
# another of the named ones goes back to Neon, but only if it was the palette
# that left it: a set chosen by hand is the player's.  A palette of the
# player's own begins as a copy of the one in use, so it keeps the tiles too.
proc display_palette {key} {
    global display
    if {[catch {angband_palette set [display_palette_colours $key]} err]} {
        return -code error $err
    }
    set display(palette) $key
    settings_set display,palette $key
    set said ""
    if {$key eq "colourblind" && [angband_tileset] == 7} {
        if {[angband_choose_tileset 8]} {
            settings_set display,pairedtiles 1
            set said "The map's tiles are now Neon's colour-blind set."
        }
    } elseif {$key ni {colourblind custom}
            && [settings_get display,pairedtiles 0] && [angband_tileset] == 8} {
        if {[angband_choose_tileset 7]} {
            set said "The map's tiles are Neon's own again."
        }
    }
    event generate . <<PaletteChanged>> -when tail
    return $said
}

# One colour changed by hand: the player's own palette, starting from the one
# in use.
proc display_colour {name hex} {
    global display
    set own [settings_get display,custom {}]
    if {$display(palette) ne "custom"} {
        set own [display_palette_colours $display(palette)]
    }
    dict set own $name $hex
    settings_set display,custom $own
    display_palette custom
}

# The colour of one of the front end's own roles in the palette in use.
proc display_role {role} {
    global display
    set key [expr {[info exists display(palette)] ? $display(palette) : "game"}]
    set roles $display(roles,game)
    if {[dict exists [display_palettes] $key]} {
        set roles [dict merge $roles [lindex [dict get [display_palettes] $key] 2]]
    }
    if {[dict exists $roles $role]} { return [dict get $roles $role] }
    return white
}

# --- starting, and going back to the defaults ------------------------------------------

# The palette chosen last time.  At the start, before anything is drawn, and
# again once a character is in play: a character's own pref files can carry
# color: lines, and the palette the player chose here wins over them.
proc display_palette_start {} {
    global display
    display_palette_snapshot
    set key [settings_get display,palette game]
    set known [dict keys [display_palette_choices]]
    if {$key ni $known} { set key game }
    set display(palette) $key
    if {$key ne "game"} {
        catch {angband_palette set [display_palette_colours $key]}
    }
}

proc display_defaults {} {
    global display angband
    foreach {key name label default} $display(fonts) {
        lassign $default family size
        font configure $name -family $family -size $size
        settings_set display,font,$key $default
    }
    catch angband_refont
    set angband(cellw) [font measure termfont "W"]
    set angband(cellh) [font metrics termfont -linespace]
    wm minsize . [expr {80 * $angband(cellw)}] [expr {24 * $angband(cellh)}]
    event generate . <<FontsChanged>> -when tail
    catch {display_uisize $display(uibase)}
    catch {display_tilescale [display_tilescale_default]}
    catch {display_palette game}
    if {[angband_tileset] != 7} { angband_choose_tileset 7 }
    display_page_refresh
}

bind . <<Angband_ENTER_GAME>> {+
    if {[info exists display(palette)] && $display(palette) ne "game"} {
        catch {angband_palette set [display_palette_colours $display(palette)]}
    }
}

# --- the page ---------------------------------------------------------------------------

# The Display tab of the Options window: the tile set and its size on the
# left, the fonts and the colours on the right.
proc display_page {p} {
    global display angband

    set left [ttk::frame $p.left]
    set right [ttk::frame $p.right]
    grid $left -row 0 -column 0 -sticky nsew -padx {0 12}
    grid $right -row 0 -column 1 -sticky nsew
    grid columnconfigure $p 1 -weight 1

    ttk::labelframe $left.tiles -text "Tile set" -padding 8
    pack $left.tiles -fill x
    set i 0
    foreach pair [angband_tilesets] {
        lassign $pair id name
        ttk::radiobutton $left.tiles.t$i -text $name -variable angband(tileset) \
            -value $id -command [list angband_choose_tileset $id]
        pack $left.tiles.t$i -anchor w -pady 1
        incr i
    }

    ttk::labelframe $left.size -text "Tile size" -padding 8
    pack $left.size -fill x -pady {10 0}
    set display(scale) [angband_tilescale]
    foreach {n label} $display(scales) {
        ttk::radiobutton $left.size.s$n -text $label -variable display(scale) \
            -value $n -command [list display_scale_chosen $n]
        pack $left.size.s$n -anchor w -pady 1
    }
    ttk::label $left.size.note -wraplength 180 -justify left \
        -font [uifont 11] -text "The map's tiles, and the overhead\
            map's with them."
    pack $left.size.note -anchor w -pady {4 0}

    ttk::labelframe $right.fonts -text "Fonts" -padding 8
    pack $right.fonts -fill x
    set families [display_mono_families]
    set r 0
    foreach {key name label default} $display(fonts) {
        lassign [dict get $display(sizes) $key] lo hi
        set display(family,$key) [font configure $name -family]
        set display(size,$key) [font configure $name -size]
        ttk::label $right.fonts.l$key -text "$label:"
        # A menu button and not a combo box: the classic set of decision 18,
        # as the Knowledge window's Sort is.
        ttk::menubutton $right.fonts.f$key -textvariable display(family,$key) \
            -menu $right.fonts.f$key.m -width 18
        menu $right.fonts.f$key.m -tearoff 0
        foreach family $families {
            $right.fonts.f$key.m add radiobutton -label $family \
                -variable display(family,$key) -value $family \
                -command [list display_font_chosen $key]
        }
        ttk::spinbox $right.fonts.s$key -from $lo -to $hi -width 4 \
            -textvariable display(size,$key) \
            -command [list display_font_chosen $key]
        bind $right.fonts.s$key <Return> [list display_font_chosen $key]
        bind $right.fonts.s$key <FocusOut> [list display_font_chosen $key]
        grid $right.fonts.l$key -row $r -column 0 -sticky e -padx {0 6} -pady 2
        grid $right.fonts.f$key -row $r -column 1 -sticky w -pady 2
        grid $right.fonts.s$key -row $r -column 2 -sticky w -padx {6 0} -pady 2
        incr r
    }
    # And the windows' own text, which is a size and no more: the system's
    # family, as the rest of a Mac's windows have it.
    lassign $display(uirange) lo hi
    set display(size,windows) $display(uisize)
    ttk::label $right.fonts.lwindows -text "Window text:"
    ttk::label $right.fonts.fwindows -text "Every window but the map's" \
        -font [uifont 11]
    ttk::spinbox $right.fonts.swindows -from $lo -to $hi -width 4 \
        -textvariable display(size,windows) -command display_uisize_chosen
    bind $right.fonts.swindows <Return> display_uisize_chosen
    bind $right.fonts.swindows <FocusOut> display_uisize_chosen
    grid $right.fonts.lwindows -row $r -column 0 -sticky e -padx {0 6} -pady 2
    grid $right.fonts.fwindows -row $r -column 1 -sticky w -pady 2
    grid $right.fonts.swindows -row $r -column 2 -sticky w -padx {6 0} -pady 2

    ttk::labelframe $right.colours -text "Colours" -padding 8
    pack $right.colours -fill x -pady {10 0}
    set c $right.colours
    set i 0
    foreach {key label} [display_palette_choices] {
        ttk::radiobutton $c.p$i -text $label -variable display(palette) \
            -value $key -command [list display_palette_chosen $key]
        grid $c.p$i -row 0 -column $i -sticky w -padx {0 10}
        incr i
    }
    canvas $c.swatches -height 44 -background black -highlightthickness 0 \
        -relief sunken -borderwidth 1
    grid $c.swatches -row 1 -column 0 -columnspan $i -sticky ew -pady {8 0}
    ttk::label $c.note -wraplength 380 -justify left -font [uifont 11] \
        -text "The game's colours: the map drawn in letters, the sidebar, the\
            messages and the lists.  A tile set keeps its own -- but with Neon,\
            the colour-blind palette chooses Neon's colour-blind tiles too.\
            Click a colour to change it."
    grid $c.note -row 2 -column 0 -columnspan $i -sticky w -pady {6 0}
    bind $c.swatches <Configure> display_swatches

    ttk::button $p.defaults -text "Display Defaults" -command display_defaults
    grid $p.defaults -row 1 -column 0 -columnspan 2 -sticky w -pady {12 0}

    display_page_refresh
}

proc display_page_refresh {} {
    global display angband
    set display(scale) [angband_tilescale]
    foreach {key name label default} $display(fonts) {
        set display(family,$key) [font configure $name -family]
        set display(size,$key) [font configure $name -size]
    }
    set display(size,windows) $display(uisize)
    set angband(tileset) [angband_tileset]
    display_swatches
}

# The colours in use, as the map would draw them on black: one swatch for
# each of the game's colours, its name on hovering.
proc display_swatches {} {
    global display
    set c .options.f.nb.display.right.colours.swatches
    if {![winfo exists $c]} return
    $c delete all
    if {[catch {angband_palette} rows]} return
    set w [winfo width $c]
    set n [llength $rows]
    set per [expr {($n + 1) / 2}]
    set sw [expr {max(10, ($w - 8) / $per)}]
    set i 0
    foreach row $rows {
        lassign $row name hex
        set x [expr {4 + ($i % $per) * $sw}]
        set y [expr {4 + ($i / $per) * 19}]
        set id [$c create rectangle $x $y [expr {$x + $sw - 3}] [expr {$y + 16}] \
            -fill $hex -outline #303030 -tags [list swatch s$i]]
        $c bind $id <Enter> [list set ::options(msg) "$name  $hex"]
        $c bind $id <Leave> [list set ::options(msg) ""]
        $c bind $id <ButtonRelease-1> [list display_swatch_pick $name $hex]
        incr i
    }
}

proc display_swatch_pick {name hex} {
    set picked [tk_chooseColor -parent .options -initialcolor $hex \
        -title "$name"]
    if {$picked eq ""} return
    display_colour $name [string tolower $picked]
}

# --- the page's controls ----------------------------------------------------------------

proc display_scale_chosen {n} {
    global display options
    if {[catch {display_tilescale $n} err]} {
        set options(msg) $err
        set display(scale) [angband_tilescale]
        return
    }
    set options(msg) "Tiles drawn [lindex $display(scales) [expr {2 * $n - 1}]]."
}

proc display_uisize_chosen {} {
    global display options
    if {$display(size,windows) == $display(uisize)} return
    if {[catch {display_uisize $display(size,windows)} err]} {
        set options(msg) "The window text takes $err."
        set display(size,windows) $display(uisize)
        return
    }
    set options(msg) ""
}

proc display_font_chosen {key} {
    global display options
    if {[catch {display_font $key $display(family,$key) $display(size,$key)} err]} {
        set options(msg) "The [string tolower [lindex [lsearch -inline \
            -stride 4 $display(fonts) $key] 2]] font takes $err."
        display_page_refresh
        return
    }
    set options(msg) ""
}

proc display_palette_chosen {key} {
    global display options
    if {[catch {display_palette $key} said]} {
        set options(msg) $said
        return
    }
    set options(msg) $said
    display_swatches
}
