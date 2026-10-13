# Sound: the Options window's Sound tab, and the volume kept between sessions.
#
# The game plays its own sounds.  When the character's use_sound option is
# on, message.c signals each message's sound and sound-core.c plays whatever
# lib/customize/sound.prf binds to it: Angband 4.2's Dubtrain pack, in
# lib/sounds.  On the Mac the playing is done by the Cocoa front end's module,
# snd-cocoa.m, built into this one (TCL_Frontend.cmake).
#
# So the switch here is that option -- the same one as on the Interface tab
# and Other > Use Sound, kept with each character and off for a new one, as in
# 4.2 -- and the volume is the front end's, kept in tk-settings.txt beside the
# Display page's settings.  That is all there is: which sound goes with which
# message is sound.prf, a text file, and there is no editor for it (decision
# 7, OBS-35).
#
# Built from Tk's own widgets in the classic theme (decision 18).

set sound(volume) [settings_get sound,volume 100]
catch {angband_sound volume $sound(volume)}
set sound(shown) "$sound(volume)%"
set sound(save) ""

# What the Try button plays, one after another: short sounds, and ones a
# player hears often, so the volume is set by what the game will sound like.
set sound(tries) {HIT MISS STORE_ENTER LEVEL HITWALL}
set sound(try) 0

proc sound_page {p} {
    global sound

    ttk::labelframe $p.play -text "Sound" -padding 8
    pack $p.play -fill x
    ttk::checkbutton $p.play.on -text "Play the game's sounds" \
        -variable options(o,use_sound) \
        -command {options_set use_sound}
    ttk::label $p.play.note -wraplength 460 -justify left \
        -font {Helvetica 11} -text "Kept with each character, as the game's\
            other options are, and off for a new one.  Other ▸ Use Sound\
            switches it too."
    pack $p.play.on -anchor w
    pack $p.play.note -anchor w -pady {4 0}

    ttk::labelframe $p.vol -text "Volume" -padding 8
    pack $p.vol -fill x -pady {10 0}
    ttk::scale $p.vol.s -from 0 -to 100 -length 280 \
        -variable sound(volume) -command sound_volume_moved
    ttk::label $p.vol.n -textvariable sound(shown) -width 5 -anchor e
    ttk::button $p.vol.try -text "Try" -width 6 -command sound_try
    grid $p.vol.s -row 0 -column 0 -sticky ew
    grid $p.vol.n -row 0 -column 1 -padx {8 0}
    grid $p.vol.try -row 0 -column 2 -padx {12 0}
    grid columnconfigure $p.vol 0 -weight 1

    ttk::label $p.credit -wraplength 460 -justify left -font {Helvetica 11} \
        -text "The sounds are Angband 4.2's, made by Dubtrain and shared under\
            the Creative Commons Attribution 4.0 licence.  Which sound goes\
            with what is in lib/customize/sound.prf."
    pack $p.credit -anchor w -pady {12 0}

    sound_page_refresh
}

# The switch follows the character, and without one there is nothing to
# switch; the volume and Try work whenever there is sound at all.
proc sound_page_refresh {} {
    set p .options.f.nb.sound
    if {![winfo exists $p]} return
    set character [expr {![catch {angband_option} rows]
        && [llength [lsearch -inline -index 0 $rows use_sound]]}]
    $p.play.on state [expr {$character ? "!disabled" : "disabled"}]
    set on [expr {![catch {angband_sound ready} ready] && $ready}]
    foreach w [list $p.vol.s $p.vol.try] {
        $w state [expr {$on ? "!disabled" : "disabled"}]
    }
}

# The slider: heard at once, saved once it stops moving, since every save
# writes the settings file again.
proc sound_volume_moved {value} {
    global sound
    set v [expr {round($value)}]
    set sound(shown) "$v%"
    catch {angband_sound volume $v}
    after cancel $sound(save)
    set sound(save) [after 400 [list settings_set sound,volume $v]]
}

proc sound_try {} {
    global sound
    set name [lindex $sound(tries) $sound(try)]
    set sound(try) [expr {($sound(try) + 1) % [llength $sound(tries)]}]
    catch {angband_sound play $name}
}
