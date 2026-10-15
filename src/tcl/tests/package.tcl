# Sourced by a packaged ZangbandTK -- the Mac bundle, the Linux AppImage, the
# Windows folder -- through ZANGBAND_TCL_SCRIPT, by scripts/run-tcl-package-test.
#
# The bridge test runs the game in its build tree, where Tcl and Tk and the
# front end's scripts are all on the build machine.  A package that quietly
# reached back for any of them would pass there and fail on every other
# machine, with an error naming a path its player has never seen.  So this
# asks the packaged game where each came from, and that each is inside the
# package: ZANGBAND_PACKAGE_ROOT.
#
# The report goes to ZANGBAND_TCL_RESULT, its first line the number of
# failures, as bridge.tcl's does, because Tk has closed stdout by now.

set pkgcheck(failed) 0
set pkgcheck(report) {}

proc package_inside {what path} {
    global pkgcheck
    set root [file normalize $::env(ZANGBAND_PACKAGE_ROOT)]
    set path [file normalize $path]
    if {[string first "$root/" "$path/"] == 0} {
        lappend pkgcheck(report) "ok   $what is the package's own ($path)"
    } else {
        lappend pkgcheck(report) "FAIL $what is not inside $root: $path"
        incr pkgcheck(failed)
    }
}

proc package_check {what script} {
    global pkgcheck
    if {[catch {uplevel #0 $script} err]} {
        lappend pkgcheck(report) "FAIL $what: $err"
        incr pkgcheck(failed)
    } else {
        lappend pkgcheck(report) "ok   $what ($err)"
    }
}

package_inside "Tcl's script library" [info library]
package_inside "Tk's script library" $tk_library
package_inside "the front end's script folder" $display(dir)
package_check "a Tcl module from tcl9/ loads" { package require msgcat }
package_check "the panes have a fixed-width face" {
    if {![font metrics termfont -fixed]} { error "[font actual termfont -family] is proportional" }
    font actual termfont -family
}

set f [open $env(ZANGBAND_TCL_RESULT) w]
fconfigure $f -encoding utf-8
puts $f $pkgcheck(failed)
puts $f [join $pkgcheck(report) \n]
close $f

after 100 { exit 0 }
