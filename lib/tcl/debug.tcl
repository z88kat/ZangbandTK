# The debug console.
#
# Named after the 2001 script, but it is not what the 2001 script was.  That
# file -- 1,441 lines in CommonTk, 839 in the Zangband runtime -- is a *data
# browser*: NSDebug, a tabbed window over character, equipment, inventory,
# monster, object and quest structures, built on the megawidget set decision 15
# declines to port, and superseded here by knowledge.tcl and character.tcl,
# which show the same things from the same data and are already written.
#
# The console the Phase 3 plan's T3 exit is actually asking for lives at the
# bottom of the 2001 errorInfo.tcl, as `InitCommandWindow`: fifty lines, an
# entry widget, Return evaluates, Up and Down walk a history.  It is commented
# out in the file that ships.  That idea is the one taken up here; the name
# `debug.tcl` is kept because the plan names it and because what this does is
# what a person means when they say debug window now.
#
# Why this is the valuable half.  An error window tells you something broke.  A
# console is what turns "I think angband_player returns blows per round as a
# real" into "it returns 1.33", without a build, without a test file, and
# against the running game with a real character in it.  Every bridge check in
# src/tcl/tests/bridge.tcl was a question of that shape first.
#
# Deliberately plain, for the same reason as errorInfo.tcl: this is the window
# you use when the other windows are wrong, so it uses nothing that can be
# wrong.  No `classical::`.
#
# It is also the one window that does *not* forward keys to the game.  Every
# other toplevel in lib/tcl binds <Key> to angband_key so it can be left open
# while you play; a console that did that would send what you were typing to
# the game a character at a time.  The trade is deliberate and it is the right
# way round: this is a window you use instead of playing, not while.

namespace eval console {
    variable P

    # Lines typed, oldest first, and where Up/Down currently sit in them.  An
    # index of [llength history] means "not in the history", i.e. the line
    # being typed now.
    set P(history) {}
    set P(hindex) 0
    set P(maxhistory) 200

    # Accumulated input for a command that is not finished yet.  `info
    # complete` decides, which is what an interactive tclsh uses and what
    # src/tcl/tests/scripts.test already relies on, so a pasted multi-line proc
    # works rather than erroring on the first line.
    set P(partial) ""

    # The last result, kept so a test or a caller can read it back without
    # parsing the transcript.
    set P(result) ""
    set P(error) 0
}

# console::eval --
#
#   Evaluate a script against the live interpreter and return its result.
#
#   This is the console, and the window below is a way of typing into it.
#   Keeping them apart is what makes the thing testable: this proc wants no
#   window, no display and no character, so `scripts/run-tcl-tests` can reach
#   it and so can a bridge check.
#
#   Evaluation is at the global level, with `uplevel #0`, because that is where
#   everything a person would want to ask about lives -- $angband(terms), the
#   knowledge() and character() arrays, and all 23 angband_* commands.  A
#   console that evaluated in its own scope would answer questions about a
#   context nothing else shares, which is worse than useless: it would be
#   plausibly wrong.
#
#   An error is recorded in the sink and returned as text, not raised.  The
#   caller is a typist or a test; neither wants an exception.
proc console::eval {script} {
    variable P

    set P(error) 0
    if {[catch {uplevel #0 $script} result options]} {
        set P(error) 1
        set stack $result
        if {[dict exists $options -errorinfo]} {
            set stack [dict get $options -errorinfo]
        }
        errorsink::record console $result $stack
        set P(result) $result
        return $result
    }
    set P(result) $result
    return $result
}

# console::failed --
#
#   Whether the last console::eval was an error.  `eval` returns the message in
#   either case -- a result and an error message are both just text -- so this
#   is how a caller tells them apart.
proc console::failed {} {
    variable P
    return $P(error)
}

# console::submit --
#
#   One line from the entry: remember it, decide whether the command is
#   finished, and if it is, run it and show both halves.
proc console::submit {} {
    variable P

    set line [.console.entry get]
    .console.entry delete 0 end

    # An empty line at a clean prompt is nothing; at a continuation prompt it
    # is a way out of a command that will never complete, which is otherwise a
    # console you have to close to recover.
    if {$line eq "" && $P(partial) eq ""} {
        return
    }

    if {$P(partial) eq ""} {
        console::show "> $line"
    } else {
        console::show "  $line"
    }

    if {$line ne ""} {
        lappend P(history) $line
        if {[llength $P(history)] > $P(maxhistory)} {
            set P(history) [lrange $P(history) 1 end]
        }
    }
    set P(hindex) [llength $P(history)]

    if {$P(partial) eq ""} {
        set script $line
    } else {
        set script "$P(partial)\n$line"
    }

    if {$line ne "" && ![info complete $script]} {
        set P(partial) $script
        .console.prompt configure -text "..."
        return
    }

    set P(partial) ""
    .console.prompt configure -text ">"

    set result [console::eval $script]
    if {[console::failed]} {
        # The message here and the full trace in the error window.  Putting the
        # whole stack in the transcript buries the next thing you type.
        console::show "! $result"
    } elseif {$result ne ""} {
        console::show $result
    }
    return
}

# console::show --
#
#   A line into the transcript.  Not batched, unlike the error sink: this one
#   is driven by a person's typing, so there is never a burst.
proc console::show {text} {
    if {![winfo exists .console]} return
    set t .console.f.text
    $t configure -state normal
    $t insert end "$text\n"
    set lines [expr {int([$t index end]) - 1}]
    if {$lines > 2000} {
        $t delete 1.0 [expr {$lines - 2000}].0
    }
    $t configure -state disabled
    $t see end
    return
}

# console::recall --
#
#   Up and Down through what has been typed.  The 2001 version put the *result*
#   back into the entry after running, which destroyed the command you had just
#   typed and made the history the only way to run anything twice; here the
#   entry keeps the command and the result goes to the transcript, so Up is a
#   convenience rather than a repair.
proc console::recall {dir} {
    variable P
    set n [llength $P(history)]
    if {$n == 0} return

    set i [expr {$P(hindex) + $dir}]
    if {$i < 0} { set i 0 }
    if {$i > $n} { set i $n }
    set P(hindex) $i

    .console.entry delete 0 end
    if {$i < $n} {
        .console.entry insert 0 [lindex $P(history) $i]
    }
    return
}

# console_window --
#
#   Open it, or raise it.  Same shape as the other windows so the Window menu
#   treats it the same way.
proc console_window {} {
    if {[winfo exists .console]} {
        wm deiconify .console
        raise .console
        focus .console.entry
        return
    }

    toplevel .console
    wm title .console "Console"
    wm geometry .console 860x480
    wm minsize .console 480 240
    wm protocol .console WM_DELETE_WINDOW { destroy .console }

    # Escape closes.  No <Key> binding to angband_key -- see the header.
    bind .console <Escape> { destroy .console ; break }

    ttk::frame .console.f
    text .console.f.text -wrap none -undo 0 -state disabled \
        -font errorfont -highlightthickness 0 -borderwidth 0 \
        -yscrollcommand {.console.f.y set}
    ttk::scrollbar .console.f.y -orient vertical \
        -command {.console.f.text yview}
    grid .console.f.text -row 0 -column 0 -sticky news
    grid .console.f.y    -row 0 -column 1 -sticky ns
    grid rowconfigure    .console.f 0 -weight 1
    grid columnconfigure .console.f 0 -weight 1

    ttk::label .console.prompt -text ">" -font errorfont
    entry .console.entry -font errorfont -borderwidth 1

    grid .console.f      -row 0 -column 0 -columnspan 2 -sticky news
    grid .console.prompt -row 1 -column 0 -sticky w -padx 4
    grid .console.entry  -row 1 -column 1 -sticky ew -padx 4 -pady 4
    grid rowconfigure    .console 0 -weight 1
    grid columnconfigure .console 1 -weight 1

    bind .console.entry <Return>   { console::submit ; break }
    bind .console.entry <KeyPress-Up>   { console::recall -1 ; break }
    bind .console.entry <KeyPress-Down> { console::recall  1 ; break }

    # What there is to ask about, generated rather than written down.  The same
    # principle as T7's menu bar: a command added to the bridge appears here
    # without anyone editing this file, and a command removed stops being
    # advertised.  Without this the first question is always "what are the
    # accessors called", and the answer used to be "read main-tcl.c".
    console::show "# ZangbandTK console.  Evaluates at the global level."
    console::show "# Errors go to the Errors window as well as here."
    set cmds [lsort [info commands angband_*]]
    console::show "# [llength $cmds] bridge commands:"
    foreach chunk [console::wrap $cmds 72] {
        console::show "#   $chunk"
    }
    console::show ""

    focus .console.entry
    return
}

# console::wrap --
#
#   Fold a list of words into lines of at most `width` characters.  Here rather
#   than in the design system because this file does not use the design system,
#   and it is six lines.
proc console::wrap {words width} {
    set lines {}
    set line ""
    foreach w $words {
        if {$line ne "" && [string length "$line $w"] > $width} {
            lappend lines $line
            set line ""
        }
        if {$line eq ""} { set line $w } else { append line " " $w }
    }
    if {$line ne ""} { lappend lines $line }
    return $lines
}
