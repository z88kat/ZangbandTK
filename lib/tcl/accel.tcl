# The underlined letters in the windows, and the keys that go with them.
#
# The original's convention, and Tk's on X11 and Windows: a button, a tab or a
# field label with a letter underlined answers to Alt and that letter.  On the
# Mac the Alt key is Option, and Option and a letter is a character in its own
# right -- Option-O types "ø" -- so Tk reports the character, and a binding on
# <Alt-o> or on <Option-o> never fires.  Every underlined shortcut in every
# window did nothing on the Mac; found by pressing them, 12 October 2026.
#
# So the windows bind their shortcuts with accel_bind, which binds Alt for the
# other two and, on the Mac, reads Option and a letter back from the key that
# was pressed rather than from the character it typed.  The key is read as a
# position on a US keyboard, which is exact on US and UK layouts; on one that
# moves letters about -- Y and Z on a German keyboard -- a few follow the key's
# US position rather than its label.  And where the focus is in a field for
# typing, Option and a letter types the character, as the Mac means it to.

# Carbon's kVK_ANSI_ codes for the letter keys: where each letter sits.
set accel(keys) {
     0 a  1 s  2 d  3 f  4 h  5 g  6 z  7 x  8 c  9 v 11 b 12 q 13 w 14 e
    15 r 16 y 17 t 31 o 32 u 34 i 35 p 37 l 38 j 40 k 45 n 46 m
}

# Run `script` for Alt (Option on the Mac) and `letter`, in the window `top`.
# The script is a binding script, so it may end in break.
proc accel_bind {top letter script} {
    global accel

    bind $top <Alt-$letter> $script
    if {[tk windowingsystem] ne "aqua"} return

    # A window built again starts again: what the last one bound goes with it.
    if {[bind $top <Option-KeyPress>] eq ""} {
        array unset accel $top,*
        bind $top <Option-KeyPress> \
            "if \[[list accel_option $top] %W %k\] break"
    }
    set accel($top,$letter) $script
}

# Option and a key, on the Mac: whether it was one of `top`'s shortcuts, which
# is then done.  Tk's key code carries the Mac's own code for the key in its
# top byte, whatever character the key made.
proc accel_option {top w code} {
    global accel

    if {[winfo class $w] in {TEntry Entry TSpinbox Spinbox TCombobox Text}} {
        return 0
    }
    set key [expr {($code >> 24) & 0xFF}]
    if {![dict exists $accel(keys) $key]} { return 0 }
    set letter [dict get $accel(keys) $key]
    if {![info exists accel($top,$letter)]} { return 0 }

    # A binding script's break means it was handled; an error is an error.
    if {[catch {uplevel #0 $accel($top,$letter)} err opts] == 1} {
        return -options $opts $err
    }
    return 1
}
