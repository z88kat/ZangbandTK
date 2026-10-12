# The front end's own settings, kept from one session to the next.
#
# The game's options are the game's: angband_option reads and writes them, and
# they travel with the character.  These are the front end's -- whether the
# Tips window opens at the start, which tip it was showing -- and are the same
# whichever character is played, so they are kept beside the game's pref files
# in the player's own folder (angband_path user) rather than in a savefile.
#
# One file, tk-settings.txt, a key and its value on each line as a Tcl list.
# A line that does not read as one is passed over, and a missing file is a
# first start: every setting asked for has its default until it is set.

set settings(values) [dict create]
set settings(file) ""

proc settings_load {} {
    global settings

    set settings(values) [dict create]
    if {[catch {file join [angband_path user] tk-settings.txt} settings(file)]} {
        set settings(file) ""
        return
    }
    if {[catch {open $settings(file) r} f]} return
    fconfigure $f -encoding utf-8
    foreach line [split [read $f] \n] {
        set line [string trim $line]
        if {$line eq "" || [string index $line 0] eq "#"} continue
        if {[catch {llength $line} n] || $n != 2} continue
        dict set settings(values) {*}$line
    }
    close $f
}

proc settings_get {key default} {
    global settings
    if {[dict exists $settings(values) $key]} {
        return [dict get $settings(values) $key]
    }
    return $default
}

# Set one and write them all.  Written beside the old file and renamed over
# it, so a session that ends part-way through leaves the last good copy.
proc settings_set {key value} {
    global settings

    dict set settings(values) $key $value
    if {$settings(file) eq ""} return
    set tmp $settings(file).new
    if {[catch {
        set f [open $tmp w]
        fconfigure $f -encoding utf-8
        puts $f "# ZangbandTK's front end settings, written by the game."
        dict for {k v} $settings(values) { puts $f [list $k $v] }
        close $f
        file rename -force $tmp $settings(file)
    } err]} {
        catch {close $f}
        catch {file delete $tmp}
        errorsink::record settings "could not save the front end's settings: $err"
    }
}

settings_load
