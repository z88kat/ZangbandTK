# The Options window.
#
# T9's options: the game's own, from list-options.h by way of angband_option --
# so an option added to the game appears here without anyone editing this
# file -- and the front end's beside them, so that the player never has to
# know which layer owns a setting.  A tab a page: Interface, with the three
# numeric settings under it; Birth, which can be changed only while a
# character is being made; Cheats; Display, the front end's own, which is
# display.tcl's: the tile set and its size, the panes' fonts and the colours;
# and Sound, sound.tcl's: the game's use_sound beside the front end's volume.
#
# Every change is made through angband_option and so through the game's own
# rules: a birth option is refused once a character exists, a cheat marks the
# character, and the four cheats that are things done rather than settings --
# gold, hit points, a pet, the map of the world -- do it once and switch
# themselves back off, as on the game's options screen.
#
# Built from Tk's own widgets in the classic theme (decision 18).

set options(msg) ""
set options(score) ""
set options(gold) 5000
set options(hp) 999
set options(pet) soldier

# The cheats that are done rather than set, with what each asks for.
set options(actions) {
    cheat_gold   "Gold to take:"  gold  "Take"
    cheat_hp     "Hit points:"    hp    "Set"
    cheat_pet    "Pet to gain:"   pet   "Gain"
    cheat_places "Map the world:" {}    "Reveal"
}

proc options_window {{tab ""}} {
    if {![winfo exists .options]} options_build
    options_refresh
    if {$tab ne ""} { .options.f.nb select .options.f.nb.$tab }
    wm deiconify .options
    raise .options
    focus .options.f.nb
}

# --- reading the game ----------------------------------------------------------

proc options_refresh {} {
    global options

    if {![winfo exists .options]} return
    if {[catch {angband_option} rows]} {
        set options(msg) "The options belong to a character, and there is not\
            one yet."
        return
    }
    foreach row $rows {
        lassign $row name type desc value changeable
        set options(o,$name) $value
        if {[winfo exists [options_widget $name]]} {
            [options_widget $name] state [expr {$changeable ? "!disabled" : "disabled"}]
        }
    }

    # Birth options are the character's from the moment it exists.
    set making [expr {[llength [lsearch -inline -index 0 $rows birth_no_selling]]
        && [lindex [lsearch -inline -index 0 $rows birth_no_selling] 4]}]
    set options(birthnote) [expr {$making
        ? "These shape the character being made, and are fixed once it is."
        : "Fixed for this character: birth options are chosen when a character\
            is made, and these are the ones this one was made with."}]
    foreach w {restore reset} {
        .options.f.nb.birth.act.$w state [expr {$making ? "!disabled" : "disabled"}]
    }

    # What the game will say at the end, said now.
    if {![catch {angband_option scoring} scoring]} {
        lassign $scoring cheated wizard eligible
        set options(msg) [expr {$cheated ? "Cheats have been used."
            : ($wizard ? "Wizard or debug mode has been used."
                : "No cheats have been used.")}]
        set options(score) "High score: [expr {$eligible ? "eligible" : "not eligible"}]"
    }
    sound_page_refresh
}

# The checkbutton or spinbox standing for an option, wherever it is.
proc options_widget {name} {
    global options
    return [expr {[info exists options(w,$name)] ? $options(w,$name) : ""}]
}

# --- changing it -----------------------------------------------------------------

proc options_set {name} {
    global options

    set value $options(o,$name)

    # A cheat marks the character for good, so it is asked first, as a
    # thing that cannot be taken back.
    if {[string match cheat_* $name] && $value} {
        if {![options_cheat_ok $name]} {
            set options(o,$name) 0
            return
        }
    }
    if {[catch {angband_option $name $value} err]} {
        set options(msg) [string toupper $err 0 0]
        bell
        options_refresh
        return
    }
    set options(msg) ""
}

proc options_cheat_ok {name} {
    set desc [lindex [lsearch -inline -index 0 [angband_option] $name] 2]
    regsub {^Cheat: } $desc {} desc
    expr {[tk_messageBox -parent .options -icon warning -type yesno \
        -title "Cheat" -message "Cheating marks this character." \
        -detail "Once any cheat has been used, the character can no longer\
            go on the high-score list.\n\n[string trim $desc]?"] eq "yes"}
}

# A cheat that is done once: what it asks for is answered from the field
# beside it, through the prompt the game asks it with.
proc options_act {name field} {
    global options

    if {![options_cheat_ok $name]} return
    if {$field ne ""} {
        angband_hook string [list apply {{answer prompt current} {list $answer}} \
            $options($field)]
    }
    set failed [catch {angband_option $name 1} err]
    if {$field ne ""} { angband_hook string {} }
    options_refresh
    if {$failed} {
        set options(msg) [string toupper $err 0 0]
        bell
    } else {
        set options(msg) "Done -- see the messages."
    }
}

# A page's defaults: saved for new characters, restored, or the game's own.
proc options_defaults {page how} {
    global options

    if {[catch {angband_option $how $page} err]} {
        set options(msg) [string toupper $err 0 0]
        bell
        return
    }
    set options(msg) [dict get {
        save    "Saved as the defaults for new characters."
        restore "Restored from your saved defaults."
        reset   "Back to the game's own defaults."
    } $how]
    options_refresh
}

proc options_value {name} {
    global options

    if {![string is integer -strict $options(o,$name)]} return
    if {[catch {angband_option $name $options(o,$name)} err]} {
        set options(msg) [string toupper $err 0 0]
        bell
        options_refresh
    } else {
        set options(msg) ""
    }
}

# --- the window ----------------------------------------------------------------

# A page that scrolls: a canvas holding a frame, which is how Tk does it.
proc options_scrolled {parent} {
    set f [ttk::frame $parent.scroll]
    set c [canvas $f.c -highlightthickness 0 -borderwidth 0 \
        -background [ttk::style lookup . -background] \
        -yscrollincrement 20 -yscrollcommand [list $f.sb set]]
    ttk::scrollbar $f.sb -orient vertical -command [list $c yview]
    set inner [ttk::frame $c.in]
    $c create window 0 0 -window $inner -anchor nw
    bind $inner <Configure> [list apply {{c} {
        $c configure -scrollregion [$c bbox all]
    }} $c]
    pack $f.sb -side right -fill y
    pack $c -side left -fill both -expand 1
    pack $f -fill both -expand 1
    return $inner
}

# A checkbutton an option, in the game's order, with the game's description.
proc options_checks {inner type rows} {
    global options

    set r 0
    foreach row $rows {
        lassign $row name rtype desc
        if {$rtype ne $type} continue
        if {$name in [lmap {n l f v} $options(actions) {set n}]} continue
        set w $inner.o$r
        regsub {^Cheat: } $desc {} desc
        ttk::checkbutton $w -text $desc -variable options(o,$name) \
            -command [list options_set $name]
        grid $w -row $r -column 0 -sticky w -pady 1
        set options(w,$name) $w
        incr r
    }
    return $r
}

proc options_build {} {
    global options angband

    toplevel .options
    wm title .options "Zangband: Options"
    wm minsize .options 560 520

    set mb .options.mb
    menu $mb
    .options configure -menu $mb
    menu $mb.file
    $mb add cascade -label File -underline 0 -menu $mb.file
    $mb.file add command -label "Close" -underline 0 -accelerator Esc \
        -command {destroy .options}
    # Not $mb.help: on macOS Tk treats a menu with that name as the platform's
    # own Help menu and fills it with things this window does not have.
    menu $mb.about
    $mb add cascade -label Help -underline 0 -menu $mb.about
    $mb.about add command -label "On Options..." -underline 0 -accelerator F1 \
        -command options_help

    set w .options.f
    ttk::frame $w
    pack $w -fill both -expand 1

    # Status, and the button to close, before the tabs keep their room.
    ttk::frame $w.status
    pack $w.status -side bottom -fill x -padx 2 -pady 2
    ttk::label $w.status.msg -textvariable options(msg) -font [uifont 12] \
        -relief sunken -borderwidth 1 -anchor w -padding {4 1}
    ttk::label $w.status.score -textvariable options(score) -font [uifont 12] \
        -relief sunken -borderwidth 1 -anchor center -width -20 -padding {4 1}
    pack $w.status.score -side right -padx {2 0}
    pack $w.status.msg -side left -fill x -expand 1
    ttk::frame $w.btns
    pack $w.btns -side bottom -fill x -padx 10 -pady 10
    ttk::button $w.btns.close -text "Close" -underline 0 -default active \
        -command {destroy .options}
    pack $w.btns.close -side right

    set nb $w.nb
    ttk::notebook $nb
    pack $nb -fill both -expand 1 -padx 10 -pady {10 0}

    if {[catch {angband_option} rows]} { set rows {} }

    # Interface: the game's own, and the three numbers under them.
    set p [ttk::frame $nb.interface -padding 8]
    $nb add $p -text "Interface" -underline 0
    options_page_buttons $p interface
    set inner [options_scrolled $p]
    set r [options_checks $inner interface $rows]
    ttk::separator $inner.sep -orient horizontal
    grid $inner.sep -row $r -column 0 -sticky ew -pady 8
    incr r
    foreach row $rows {
        lassign $row name type desc
        if {$type ne "value"} continue
        set f [ttk::frame $inner.v$name]
        ttk::spinbox $f.s -from 0 -to 9 -width 3 -font [uifont Courier 12] \
            -textvariable options(o,$name) -command [list options_value $name]
        bind $f.s <Return> [list options_value $name]
        bind $f.s <FocusOut> [list options_value $name]
        ttk::label $f.l -text $desc
        pack $f.s -side left
        pack $f.l -side left -padx 8
        grid $f -row $r -column 0 -sticky w -pady 2
        set options(w,$name) $f.s
        incr r
    }

    # Birth.
    set p [ttk::frame $nb.birth -padding 8]
    $nb add $p -text "Birth" -underline 0
    ttk::label $p.note -textvariable options(birthnote) -wraplength 500 \
        -justify left -font [uifont 12]
    pack $p.note -anchor w -pady {0 6}
    options_page_buttons $p birth
    set inner [options_scrolled $p]
    options_checks $inner birth $rows

    # Cheats: a warning, the settings, then the things done once.  Short
    # enough not to scroll.
    set p [ttk::frame $nb.cheat -padding 10]
    $nb add $p -text "Cheats" -underline 1
    set warn [ttk::frame $p.warn]
    ttk::label $warn.icon -text "!" -font [uifont 30 bold]
    ttk::label $warn.text -font [uifont 12] -wraplength 460 -justify left \
        -text "Using any cheat marks this character.  A marked character can\
            no longer be entered on the high-score list."
    pack $warn.icon -side left -padx {6 14}
    pack $warn.text -side left -fill x -expand 1
    pack $warn -fill x -pady {0 10}

    ttk::labelframe $p.opts -text "Cheat Options" -padding {10 6}
    pack $p.opts -fill x
    options_checks $p.opts cheat $rows

    set a [ttk::labelframe $p.acts -text "One-Time Actions" -padding {10 6}]
    pack $a -fill x -pady {12 0}
    ttk::label $a.note -font [uifont 12] \
        -text "Each takes effect once, when its button is pressed."
    grid $a.note -row 0 -column 0 -columnspan 3 -sticky w -pady {0 8}
    set r 1
    foreach {name label field verb} $options(actions) {
        ttk::label $a.l$r -text $label
        grid $a.l$r -row $r -column 0 -sticky e -padx {0 8} -pady 3
        if {$field ne ""} {
            ttk::entry $a.e$r -textvariable options($field) -width 16 \
                -font [uifont Courier 12]
            grid $a.e$r -row $r -column 1 -sticky w -pady 3
        }
        ttk::button $a.b$r -text $verb -width 9 \
            -command [list options_act $name $field]
        grid $a.b$r -row $r -column 2 -sticky w -padx {10 0} -pady 3
        incr r
    }

    # Display: the front end's, beside the game's -- display.tcl's.
    set p [ttk::frame $nb.display -padding 8]
    $nb add $p -text "Display" -underline 0
    display_page $p

    # Sound: sound.tcl's.
    set p [ttk::frame $nb.sound -padding 8]
    $nb add $p -text "Sound" -underline 0
    sound_page $p

    bind .options <Escape> {destroy .options}
    bind .options <F1> {options_help; break}
    accel_bind .options c {destroy .options}
    # The tabs answer their underlined letters too.  Cheats is h, because C
    # is Close.
    foreach {letter page} {i interface b birth h cheat d display s sound} {
        accel_bind .options $letter \
            [list .options.f.nb select .options.f.nb.$page]
    }
    # The wheel scrolls the page showing, wherever the pointer is in the
    # window -- not only over the gaps between the options.  Three rows a
    # notch, as Tk's own lists scroll: Tk 9 reports a notch as 120, and this
    # used to scroll by that many units, which is the whole page at once.
    bind .options <MouseWheel> {
        if {[winfo class %W] ni {TSpinbox TEntry}} {
            set c "[.options.f.nb select].scroll.c"
            if {[winfo exists $c]} { tk::MouseWheel $c y %D -40.0 }
        }
    }
    bind .options <FocusIn> {if {"%W" eq ".options"} options_refresh}
}

# F1: the manual where it covers the page showing -- the game's options by
# their kind, and the front end's own, Display and Sound, in its section of
# the customising chapter.
proc options_help {} {
    set page [winfo name [.options.f.nb select]]
    help_window [help_first [dict get {
        interface option#user-interface-options
        birth option#birth-options
        cheat option#cheating-options
        display customize#tcl-tk
        sound customize#tcl-tk
    } $page] option]
}

# Save, restore and reset, for a page of the game's options.
proc options_page_buttons {p page} {
    set a [ttk::frame $p.act]
    pack $a -side bottom -fill x -pady {8 0}
    ttk::button $a.save -text "Save as Defaults" \
        -command [list options_defaults $page save]
    ttk::button $a.restore -text "Restore Defaults" \
        -command [list options_defaults $page restore]
    ttk::button $a.reset -text "Game Defaults" \
        -command [list options_defaults $page reset]
    pack $a.save $a.restore $a.reset -side left -padx {0 6}
}

# Preferences in the application menu, where a Mac keeps them.  Elsewhere
# there is no application menu, and no ::tk::mac namespace to define this in;
# Window > Options opens the same window.
if {[tk windowingsystem] eq "aqua"} {
    proc ::tk::mac::ShowPreferences {} { options_window }
}
