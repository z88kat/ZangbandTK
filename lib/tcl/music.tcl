# The player's own music: the Sound page's lower half, and the playlist.
#
# OBS-33.  Nothing ships with the game -- so nothing has to be licensed -- and
# the player points it at a folder of their own.  music.m plays one track at a
# time (angband_music); this decides which, in a shuffled order, and moves on
# when one ends, which it asks about once a second: soon enough that the gap
# between tracks is not worth the name, and nothing the rest of the time.
#
# Whether it plays, the folder and the volume are the front end's, kept in
# tk-settings.txt, and off to begin with.  The folder is read with the folders
# inside it, as music is usually filed by album.
#
# Built from Tk's own widgets in the classic theme (decision 18).

set music(on) [settings_get music,on 0]
# file home, not ~: Tcl 9 no longer expands a tilde, so ~/Music was a folder
# called "~" in the directory the game started in, on every platform.
set music(dir) [settings_get music,dir [file join [file home] Music]]
set music(volume) [settings_get music,volume 60]
set music(list) {}
set music(at) -1
set music(now) ""
set music(shown) "$music(volume)%"
set music(paused) 0
set music(tick) ""
set music(save) ""

# What the player plays: Core Audio's formats on the Mac (music.m), SDL_mixer's
# on Windows and Linux (music-sdl.c), which has no AAC.  Not MIDI, and not the
# tracker modules the original's player was for: those want a player of their
# own.
if {[tk windowingsystem] eq "aqua"} {
    set music(types) {.mp3 .m4a .m4b .aac .wav .aif .aiff .aifc .caf .flac
        .ogg .oga .opus}
    set music(said) "MP3, AAC, FLAC, Ogg, Opus, WAV and AIFF"
} else {
    set music(types) {.mp3 .wav .aif .aiff .flac .ogg .oga .opus}
    set music(said) "MP3, FLAC, Ogg, Opus, WAV and AIFF"
}

# The tracks in a folder and the folders in it, as far as three deep.
proc music_files {dir {depth 0}} {
    global music
    set found {}
    if {$depth > 3 || ![file isdirectory $dir]} { return $found }
    foreach f [lsort -dictionary [glob -nocomplain -directory $dir *]] {
        if {[file isdirectory $f]} {
            lappend found {*}[music_files $f [expr {$depth + 1}]]
        } elseif {[string tolower [file extension $f]] in $music(types)} {
            lappend found $f
        }
    }
    return $found
}

proc music_shuffle {list} {
    set n [llength $list]
    for {set i [expr {$n - 1}]} {$i > 0} {incr i -1} {
        set j [expr {int(rand() * ($i + 1))}]
        set t [lindex $list $i]
        lset list $i [lindex $list $j]
        lset list $j $t
    }
    return $list
}

# Start from the folder: read it, shuffle it, play the first.
proc music_start {} {
    global music
    music_stop
    if {!$music(on) || ![angband_music here]} return
    set music(list) [music_shuffle [music_files $music(dir)]]
    if {![llength $music(list)]} {
        set music(now) "No music in that folder."
        return
    }
    music_play_at 0
    music_tick
}

proc music_stop {} {
    global music
    after cancel $music(tick)
    catch {angband_music stop}
    set music(at) -1
    set music(paused) 0
    set music(now) ""
    music_buttons
}

# Play the track at a place in the order, or the next that will play: a file
# Core Audio cannot read is passed over rather than stopping the music.
proc music_play_at {i} {
    global music
    set n [llength $music(list)]
    for {set tries 0} {$tries < $n} {incr tries} {
        set i [expr {($i % $n + $n) % $n}]
        set f [lindex $music(list) $i]
        if {![catch {angband_music play $f $music(volume)} ok] && $ok} {
            set music(at) $i
            set music(paused) 0
            music_show
            music_buttons
            return 1
        }
        incr i
    }
    set music(now) "None of the music in that folder will play."
    return 0
}

proc music_next {} {
    global music
    if {$music(at) < 0} { music_start ; return }
    music_play_at [expr {$music(at) + 1}]
}

proc music_previous {} {
    global music
    if {$music(at) < 0} { music_start ; return }
    music_play_at [expr {$music(at) - 1}]
}

proc music_pause {} {
    global music
    if {$music(at) < 0} return
    set music(paused) [expr {!$music(paused)}]
    catch {angband_music [expr {$music(paused) ? "pause" : "resume"}]}
    music_buttons
}

# Once a second while there is music: the next track when one has ended, and
# where the one playing has got to.
proc music_tick {} {
    global music
    after cancel $music(tick)
    if {!$music(on) || $music(at) < 0} return
    lassign [angband_music state] state
    if {$state eq "ended" || $state eq "stopped"} {
        if {![music_play_at [expr {$music(at) + 1}]]} return
    }
    music_show
    set music(tick) [after 1000 music_tick]
}

proc music_clock {seconds} {
    set s [expr {int($seconds)}]
    format "%d:%02d" [expr {$s / 60}] [expr {$s % 60}]
}

proc music_show {} {
    global music
    if {$music(at) < 0} return
    lassign [angband_music state] state position length
    set name [file rootname [file tail [lindex $music(list) $music(at)]]]
    # A length of 0 is one the player cannot tell -- SDL_mixer before 2.6,
    # which the Linux build has -- and then the time played stands alone.
    if {$length > 0} {
        set position [expr {min($position, $length)}]
        set music(now) "$name  —  [music_clock $position] of [music_clock $length]"
    } else {
        set music(now) "$name  —  [music_clock $position]"
    }
}

# --- the Sound page's lower half -------------------------------------------------

proc music_section {p} {
    global music
    ttk::labelframe $p.music -text "Your music" -padding 8
    set m $p.music
    ttk::checkbutton $m.on -text "Play music from a folder of your own" \
        -variable music(on) -command music_switched
    ttk::label $m.dir -textvariable music(dir) -font [uifont 11] \
        -relief sunken -borderwidth 1 -padding {4 1}
    ttk::button $m.choose -text "Choose…" -command music_choose
    ttk::scale $m.vol -from 0 -to 100 -length 200 -variable music(volume) \
        -command music_volume_moved
    ttk::label $m.voln -textvariable music(shown) -width 5 -anchor e
    set b [ttk::frame $m.btns]
    ttk::button $b.prev -text "Previous" -command music_previous
    ttk::button $b.pause -text "Pause" -width 7 -command music_pause
    ttk::button $b.next -text "Next" -command music_next
    pack $b.prev $b.pause $b.next -side left -padx {0 6}
    ttk::label $m.now -textvariable music(now) -font [uifont 11]
    ttk::label $m.note -wraplength 460 -justify left -font [uifont 11] \
        -text "None comes with the game: choose a folder of your own.\
            $music(said) play, from the folders inside it too, in a shuffled\
            order.  Kept for every character."

    grid $m.on -row 0 -column 0 -columnspan 3 -sticky w
    grid $m.dir -row 1 -column 0 -columnspan 2 -sticky ew -pady {6 0}
    grid $m.choose -row 1 -column 2 -sticky w -padx {8 0} -pady {6 0}
    grid $m.vol -row 2 -column 0 -sticky ew -pady {6 0}
    grid $m.voln -row 2 -column 1 -sticky w -padx {8 0} -pady {6 0}
    grid $m.btns -row 3 -column 0 -columnspan 3 -sticky w -pady {8 0}
    grid $m.now -row 4 -column 0 -columnspan 3 -sticky w -pady {6 0}
    grid $m.note -row 5 -column 0 -columnspan 3 -sticky w -pady {6 0}
    grid columnconfigure $m 0 -weight 1

    if {![angband_music here]} {
        $m.on state disabled
        set music(now) "This build of the game plays no music."
    }
    music_buttons
    return $m
}

proc music_buttons {} {
    global music
    set b .options.f.nb.sound.music.btns
    if {![winfo exists $b]} return
    set live [expr {$music(on) && $music(at) >= 0}]
    foreach w [list $b.prev $b.pause $b.next] {
        $w state [expr {$live ? "!disabled" : "disabled"}]
    }
    $b.pause configure -text [expr {$music(paused) ? "Play" : "Pause"}]
}

proc music_switched {} {
    global music
    settings_set music,on $music(on)
    if {$music(on)} { music_start } else { music_stop }
}

proc music_choose {} {
    global music
    set dir [tk_chooseDirectory -parent .options -title "Your music" \
        -initialdir $music(dir) -mustexist 1]
    if {$dir eq ""} return
    set music(dir) $dir
    settings_set music,dir $dir
    if {$music(on)} music_start
}

# The slider: heard at once, kept once it stops moving.
proc music_volume_moved {value} {
    global music
    set v [expr {round($value)}]
    set music(volume) $v
    set music(shown) "$v%"
    catch {angband_music volume $v}
    after cancel $music(save)
    set music(save) [after 400 [list settings_set music,volume $v]]
}

# Music from the start, if it was playing when the game was last left.
after 1500 { if {$music(on)} music_start }
