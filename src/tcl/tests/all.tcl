# The Tcl side's test runner.
#
# Run it with scripts/run-tcl-tests, which points tclsh at the toolchain
# scripts/build-tcltk produced.  Every *.test file beside this one is picked up.
#
# These are not the C unit tests.  Those live in src/tests, are programs linked
# against angband.o, and have their own runner; do not try to merge the two --
# see section 9 of .claude/plans/phase3-tcl-tk-frontend.md.
#
# Two suites are intended here, and only the first exists yet:
#
#   *.test      headless, plain tclsh, no display.  Runs everywhere.
#   *.wtest     needs Tk and a window.  Whether a CI runner can open one is
#               still an open question; until it is answered these stay a
#               local gate rather than a CI one.

package require tcltest 2.5
namespace import ::tcltest::*

# Where the things under test are, as absolute paths, so a test never depends
# on the directory it was started from.
set here [file normalize [file dirname [info script]]]
set ::LIB_TCL [file normalize [file join $here .. .. .. lib tcl]]

# Deliberately no `package require Tk` here, not even behind a constraint.
# Loading Tk from a headless tclsh takes over the interpreter and this runner
# then produces no output at all and exits 0 -- a suite that silently runs
# nothing, which is worse than one that fails.  Tk-dependent tests get their
# own runner when there is somewhere to run them.
::tcltest::configure -testdir $here -singleproc 1

runAllTests
