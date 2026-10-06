# The error sink.
#
# Named after the 2001 script that did this job, per the naming rule in §3.3 of
# the Phase 3 plan: the concept survives, so the name does.  The mechanism does
# not survive -- see "What this does not take from 2001" at the bottom.
#
# What it is for.  This front end's characteristic failure is not a crash, it
# is silence.  Tk redirects stdout and stderr to /dev/null for a bundled
# application, which is what hid the whole of T0's launch failure; `plog` goes
# to stderr; and an error inside an <<Angband_*>> binding is reported by C and
# then goes nowhere a person will ever read.  Every bug in the T4, T6 and T7
# notes was found by making the failure visible first and diagnosing it second.
# This is that step, made permanent.
#
# Deliberately plain.  No `classical::` anything: this is the window you look at
# when the other windows are broken, so it depends on nothing that can break,
# and it is a text widget and a scrollbar on purpose rather than for want of
# effort.
#
# It records whether or not it is open.  An error at startup happens long
# before anyone has thought to open a window, so records are buffered and the
# window shows the backlog when it opens.  The 2001 version returned early when
# the window did not exist, which lost exactly the errors worth having.

namespace eval errorsink {
    variable P

    # Everything recorded so far, oldest first, whether or not there is a
    # window.  Bounded for the same reason the widget is: a binding that fails
    # once a turn for an hour must not be able to exhaust memory.
    set P(records) {}

    # How many errors have been recorded.  A test asserts on this; so can you.
    set P(errors) 0

    # Lines waiting to go into the widget, and the idle handler that will put
    # them there.  Batched because a failing event binding fails in bursts --
    # one per redraw -- and inserting per record makes the window the slowest
    # thing in the game at precisely the moment it is needed.
    set P(pending) {}
    set P(flushId) ""

    # The caps.  Lines in the widget, records in the buffer.
    set P(maxlines) 2000
    set P(maxrecords) 500
}

# errorsink::record --
#
#   Put something in the sink.  `context` says where it came from -- the event
#   name, the hook name, "console" -- and `stack` is an errorInfo trace if there
#   is one.  Everything else is optional, so a caller with only a message can
#   still use it.
#
#   This is the one entry point.  C calls it indirectly through the background
#   handler below; the console calls it directly; anything else that wants to
#   say something can call it too.
proc errorsink::record {context message {stack ""}} {
    variable P

    incr P(errors)

    set when [clock format [clock seconds] -format %H:%M:%S]
    set text "$when  $context: $message"
    if {$stack ne "" && $stack ne $message} {
        # Indent the trace so the first line of each record stays findable when
        # several are on screen.  This is the whole formatting budget.
        foreach line [split [string trimright $stack] \n] {
            append text "\n        $line"
        }
    }

    lappend P(records) $text
    if {[llength $P(records)] > $P(maxrecords)} {
        set P(records) [lrange $P(records) end-[expr {$P(maxrecords) - 1}] end]
    }

    errorsink::queue $text
    return
}

# errorsink::note --
#
#   A line that is not an error -- a trace, a marker, a "got here".  The 2001
#   script called this `Debug` and it is the half of that file worth keeping:
#   when something only goes wrong once in twenty turns, a print statement that
#   survives the redirection of stderr is the tool.
proc errorsink::note {line} {
    variable P
    lappend P(records) $line
    if {[llength $P(records)] > $P(maxrecords)} {
        set P(records) [lrange $P(records) end-[expr {$P(maxrecords) - 1}] end]
    }
    errorsink::queue $line
    return
}

# errorsink::background --
#
#   The handler Tcl calls for an error nobody caught: an `after` script, a
#   binding, or -- once main-tcl.c calls Tcl_BackgroundException -- an
#   <<Angband_*>> event binding and a failed input hook.
#
#   Tk's own default puts up a modal dialog with a "Stack Trace" button, which
#   is wrong here twice over: the game is mid-turn behind it, and a binding
#   that fails every redraw produces a dialog every redraw, which is
#   unrecoverable without force-quitting.  A line in a window that is already
#   scrolling is the right shape for a thing that happens repeatedly.
proc errorsink::background {message options} {
    set stack $message
    if {[dict exists $options -errorinfo]} {
        set stack [dict get $options -errorinfo]
    }
    errorsink::record "background error" $message $stack
    return
}

# errorsink::queue, errorsink::flush --
#
#   Hold lines until idle, then insert them all at once and trim the top.  The
#   batching and the 1000-line trim are both the 2001 script's ideas; the cap
#   is larger because a stack trace is many lines and 1000 was a tail of about
#   thirty records.
proc errorsink::queue {text} {
    variable P
    lappend P(pending) $text
    if {$P(flushId) eq ""} {
        set P(flushId) [after idle ::errorsink::flush]
    }
    return
}

proc errorsink::flush {} {
    variable P
    set P(flushId) ""
    if {![winfo exists .errors]} {
        # No window: the records are kept, and opening one shows them.  The
        # pending list is cleared so it cannot double up with the backlog.
        set P(pending) {}
        return
    }
    set t .errors.f.text
    $t configure -state normal
    foreach text $P(pending) {
        $t insert end "$text\n"
    }
    set P(pending) {}

    # Trim from the top.  `end` is one past the last line, hence the -1.
    set lines [expr {int([$t index end]) - 1}]
    if {$lines > $P(maxlines)} {
        $t delete 1.0 [expr {$lines - $P(maxlines)}].0
    }
    $t configure -state disabled
    $t see end
    return
}

# errorsink::count --
#
#   How many errors have been recorded.  Exists so a test can assert that an
#   error arrived without parsing the window, and so you can tell at a glance
#   whether anything has gone wrong since you last looked.
proc errorsink::count {} {
    variable P
    return $P(errors)
}

# errorsink::text --
#
#   Everything recorded, as one string.  The window is a view of this; this is
#   the thing itself, and it is what a test reads.  It works with no window and
#   no display, which is the point -- `scripts/run-tcl-tests` has neither.
proc errorsink::text {} {
    variable P
    return [join $P(records) \n]
}

# errorsink::clear --
#
#   Forget everything.  Mostly for tests, which want to assert on one error
#   rather than on one error and whatever else the session had already
#   accumulated.
proc errorsink::clear {} {
    variable P
    set P(records) {}
    set P(pending) {}
    set P(errors) 0
    if {[winfo exists .errors]} {
        .errors.f.text configure -state normal
        .errors.f.text delete 1.0 end
        .errors.f.text configure -state disabled
    }
    return
}

# errorsink_window --
#
#   Open it, or raise it if it is already open.  Same shape as the other
#   windows in lib/tcl, so the Window menu can treat it the same way.
proc errorsink_window {} {
    if {[winfo exists .errors]} {
        wm deiconify .errors
        raise .errors
        return
    }

    toplevel .errors
    wm title .errors "Errors"
    wm geometry .errors 860x420
    wm minsize .errors 480 200
    wm protocol .errors WM_DELETE_WINDOW { destroy .errors }

    # Escape closes it.  Nothing else is bound: unlike the character and
    # knowledge windows this one does *not* forward keys to the game, because
    # the text widget has to be selectable and copyable -- reading an error out
    # of it and into a commit message is most of what it is for.
    bind .errors <Escape> { destroy .errors ; break }

    ttk::frame .errors.f
    pack .errors.f -fill both -expand 1

    text .errors.f.text -wrap none -undo 0 -state disabled \
        -font errorfont -highlightthickness 0 -borderwidth 0 \
        -yscrollcommand {.errors.f.y set} \
        -xscrollcommand {.errors.f.x set}
    ttk::scrollbar .errors.f.y -orient vertical \
        -command {.errors.f.text yview}
    ttk::scrollbar .errors.f.x -orient horizontal \
        -command {.errors.f.text xview}

    grid .errors.f.text -row 0 -column 0 -sticky news
    grid .errors.f.y    -row 0 -column 1 -sticky ns
    grid .errors.f.x    -row 1 -column 0 -sticky ew
    grid rowconfigure    .errors.f 0 -weight 1
    grid columnconfigure .errors.f 0 -weight 1

    # The backlog: everything recorded before there was anywhere to show it.
    #
    # A flush may already be queued for records that are also in the backlog --
    # open the window between a `record` and the idle handler and both would
    # insert the same line.  Cancelling it here is cheaper than making flush
    # work out what the widget has already seen.
    if {$errorsink::P(flushId) ne ""} {
        after cancel $errorsink::P(flushId)
        set errorsink::P(flushId) ""
    }
    set errorsink::P(pending) {}

    .errors.f.text configure -state normal
    foreach text $errorsink::P(records) {
        .errors.f.text insert end "$text\n"
    }
    .errors.f.text configure -state disabled
    .errors.f.text see end
    return
}

# The font.  Fixed-width, because every line in here is either a stack trace or
# a Tcl value and both are read by their columns.  Created rather than named
# inline so the console can share it.
if {[lsearch -exact [font names] errorfont] < 0} {
    font create errorfont -family Menlo -size 11
}

# Install the handler.  This happens at source time rather than when the window
# opens, because the errors worth catching are the ones that happen before
# anyone opens anything.
interp bgerror {} ::errorsink::background

# --- What this does not take from 2001 ---------------------------------------
#
# The original tracked errors with `trace variable errorInfo w`, which is wrong
# now in three ways and was only ever half right.  `trace variable` is the
# pre-8.4 spelling, removed in Tcl 9 in favour of `trace add variable`.  More
# importantly errorInfo is written for *every* error, including the ones a
# `catch` is about to handle deliberately -- so the window filled with
# non-problems and the real ones scrolled away.  And it needed the hand-rolled
# prefix comparison in `tracecmd` to work out whether a write was a new error or
# an unwind of the one before, which is a lot of machinery for a question
# `interp bgerror` answers by construction: it is called once, per error, with
# the error.
#
# Kept: the batched `after idle` flush, the bounded buffer with the trim from
# the top, `see end`, and the `Debug` entry point, which is `note` here.
