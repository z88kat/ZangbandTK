# The Keymaps window: what each key does, and binding a key to something else.
#
# OBS-31's design.  The original's editor drew a keyboard, which is the one
# thing a list cannot do -- show at a glance which keys are taken -- and made
# the player type what a key should do as an encoded string, "\e" for Escape,
# which is the thing nobody could do.  So the keyboard stays and the encoding
# goes: a key is chosen by pressing it or clicking it, and what it does is
# chosen from the game's own commands, or recorded by pressing the keys.
#
# The keymaps are the game's own (ui-keymap.c, through angband_keymap), in
# 4.2's own form, and kept in user.prf in the player's user folder, which
# every character loads -- under the title the game's options screen saves
# them by, so a key bound here is bound in the text front end too, and the
# other way round.  A keymap belongs to a keyset: the original keys have
# their keymaps and the roguelike keys theirs.
#
# The keycaps are the keyboard's own, asked of macOS (src/tcl/keyboard.c), so
# a German keyboard shows its umlauts and its < > key where they are.  A key
# whose character the game cannot read -- ö, say -- is drawn but cannot be
# bound.
#
# Built from Tk's own widgets in the classic theme (decision 18).

set keymap(view) plain
set keymap(layer) plain
set keymap(selected) ""
set keymap(pending) ""
set keymap(recording) 0
set keymap(filter) ""
set keymap(msg) ""

# The keyboard, row by row: a number is a key that types a character, by its
# macOS virtual key code; Name:width is one that does not, or a modifier,
# which switches what the picture shows; gap:width is space.  Two shapes:
# ANSI, and ISO, which has a key beside the left Shift and a taller Return.
set keymap(rows,ansi) {
    {Escape:1.5 gap:0.5 F1 F2 F3 F4 gap:0.5 F5 F6 F7 F8 gap:0.5 F9 F10 F11 F12}
    {0x32 0x12 0x13 0x14 0x15 0x17 0x16 0x1A 0x1C 0x19 0x1D 0x1B 0x18 BackSpace:2}
    {Tab:1.5 0x0C 0x0D 0x0E 0x0F 0x11 0x10 0x20 0x22 0x1F 0x23 0x21 0x1E 0x2A:1.5}
    {gap:1.75 0x00 0x01 0x02 0x03 0x05 0x04 0x26 0x28 0x25 0x29 0x27 Return:2.25}
    {Shift:2.25 0x06 0x07 0x08 0x09 0x0B 0x2D 0x2E 0x2B 0x2F 0x2C Shift:2.75}
    {Control:1.5 gap:2.5 space:6 gap:2.5 Control:1.5}
}
set keymap(rows,iso) {
    {Escape:1.5 gap:0.5 F1 F2 F3 F4 gap:0.5 F5 F6 F7 F8 gap:0.5 F9 F10 F11 F12}
    {0x0A 0x12 0x13 0x14 0x15 0x17 0x16 0x1A 0x1C 0x19 0x1D 0x1B 0x18 BackSpace:2}
    {Tab:1.5 0x0C 0x0D 0x0E 0x0F 0x11 0x10 0x20 0x22 0x1F 0x23 0x21 0x1E Return:1.5}
    {gap:1.75 0x00 0x01 0x02 0x03 0x05 0x04 0x26 0x28 0x25 0x29 0x27 0x2A Return:1.25}
    {Shift:1.25 0x32 0x06 0x07 0x08 0x09 0x0B 0x2D 0x2E 0x2B 0x2F 0x2C Shift:2.75}
    {Control:1.5 gap:2.5 space:6 gap:2.5 Control:1.5}
}

# What a US keyboard types, for when nobody can be asked (off the Mac).
set keymap(us) {
    0x32 {` ~} 0x12 {1 !} 0x13 {2 @} 0x14 {3 #} 0x15 {4 $} 0x17 {5 %}
    0x16 {6 ^} 0x1A {7 &} 0x1C {8 *} 0x19 {9 (} 0x1D {0 )} 0x1B {- _}
    0x18 {= +} 0x0C {q Q} 0x0D {w W} 0x0E {e E} 0x0F {r R} 0x11 {t T}
    0x10 {y Y} 0x20 {u U} 0x22 {i I} 0x1F {o O} 0x23 {p P} 0x21 {[ \{}
    0x1E {] \}} 0x2A {\\ |} 0x00 {a A} 0x01 {s S} 0x02 {d D} 0x03 {f F}
    0x05 {g G} 0x04 {h H} 0x26 {j J} 0x28 {k K} 0x25 {l L} 0x29 {; :}
    0x27 {' \"} 0x06 {z Z} 0x07 {x X} 0x08 {c C} 0x09 {v V} 0x0B {b B}
    0x2D {n N} 0x2E {m M} 0x2B {, <} 0x2F {. >} 0x2C {/ ?} 0x0A {§ ±}
}

# The keys that type no character, or that the game tells apart from the ones
# that type the same -- the function keys and the keypad -- by their X11
# keysyms, and their caps.
set keymap(special) {
    Escape {0xFF1B esc} Tab {0xFF09 tab} BackSpace {0xFF08 delete}
    Return {0xFF0D return} space {0x20 space}
    Left {0xFF51 ←} Up {0xFF52 ↑} Right {0xFF53 →}
    Down {0xFF54 ↓}
    F1 {0xFFBE F1} F2 {0xFFBF F2} F3 {0xFFC0 F3} F4 {0xFFC1 F4}
    F5 {0xFFC2 F5} F6 {0xFFC3 F6} F7 {0xFFC4 F7} F8 {0xFFC5 F8}
    F9 {0xFFC6 F9} F10 {0xFFC7 F10} F11 {0xFFC8 F11} F12 {0xFFC9 F12}
    KP_0 {0xFFB0 0} KP_1 {0xFFB1 1} KP_2 {0xFFB2 2} KP_3 {0xFFB3 3}
    KP_4 {0xFFB4 4} KP_5 {0xFFB5 5} KP_6 {0xFFB6 6} KP_7 {0xFFB7 7}
    KP_8 {0xFFB8 8} KP_9 {0xFFB9 9} KP_Decimal {0xFFAE .}
    KP_Enter {0xFF8D enter} KP_Add {0xFFAB +} KP_Subtract {0xFFAD −}
    KP_Multiply {0xFFAA *} KP_Divide {0xFFAF /} KP_Equal {0xFFBD =}
    Clear {0xFF0B clear}
}

# The keypad, as the Mac's is laid out, beside the arrows: name, column and
# row in keys from its top left, width and height.
set keymap(keypad) {
    Clear 0 0 1 1  KP_Equal 1 0 1 1  KP_Divide 2 0 1 1  KP_Multiply 3 0 1 1
    KP_7 0 1 1 1  KP_8 1 1 1 1  KP_9 2 1 1 1  KP_Subtract 3 1 1 1
    KP_4 0 2 1 1  KP_5 1 2 1 1  KP_6 2 2 1 1  KP_Add 3 2 1 1
    KP_1 0 3 1 1  KP_2 1 3 1 1  KP_3 2 3 1 1  KP_Enter 3 3 1 2
    KP_0 0 4 2 1  KP_Decimal 2 4 1 1
}

proc keymap_window {} {
    if {![winfo exists .keymap]} keymap_build
    keymap_refresh
    wm deiconify .keymap
    raise .keymap
    focus .keymap.f.kb
}

# --- what the keys are ------------------------------------------------------------

# Every key on the picture: id -> {kind vk-or-name plain shifted}.  Asked
# again each time the window is built, since the layout can change.
proc keymap_keys {} {
    global keymap

    set keymap(type) [expr {[catch {angband_keyboard type} t] ? "ansi" : $t}]
    if {$keymap(type) ni {ansi iso}} { set keymap(type) ansi }
    set keymap(keys) [dict create]
    foreach row $keymap(rows,$keymap(type)) {
        foreach item $row {
            lassign [split $item :] name width
            if {[string is integer -strict $name]} {
                set plain ""
                set shifted ""
                catch {set plain [angband_keyboard char $name 0]}
                catch {set shifted [angband_keyboard char $name 1]}
                if {$plain eq "" && [dict exists $keymap(us) $name]} {
                    lassign [dict get $keymap(us) $name] plain shifted
                }
                dict set keymap(keys) k$name [list char $name $plain $shifted]
            } elseif {[dict exists $keymap(special) $name]} {
                dict set keymap(keys) k$name [list special $name]
            }
        }
    }
    foreach name {Left Up Right Down} {
        dict set keymap(keys) k$name [list special $name]
    }
    foreach {name col row w h} $keymap(keypad) {
        dict set keymap(keys) k$name [list special $name]
    }
}

# A character the game can take as a key: printable ASCII.
proc keymap_ascii {c} {
    expr {[string length $c] == 1 && [scan $c %c] > 0x20 && [scan $c %c] < 0x7f}
}

# The trigger a key on the picture is in a layer -- plain, shift or control --
# as typing it would send, through the bridge's own conversion; "" for one
# the game cannot read.
proc keymap_trigger {id layer} {
    global keymap
    set state [dict get {plain 0 shift 1 control 4} $layer]
    set key [dict get $keymap(keys) $id]
    if {[lindex $key 0] eq "special"} {
        set name [lindex $key 1]
        lassign [dict get $keymap(special) $name] keysym
        set char [expr {$name eq "space" && $layer ne "control" ? " " : ""}]
        return [angband_keymap trigger [expr {$keysym}] $state $char]
    }
    lassign $key kind vk plain shifted
    switch -- $layer {
        plain {
            if {![keymap_ascii $plain]} { return "" }
            return [angband_keymap trigger [scan $plain %c] 0 $plain]
        }
        shift {
            if {![keymap_ascii $shifted]} { return "" }
            return [angband_keymap trigger [scan $shifted %c] 1 $shifted]
        }
        control {
            if {![keymap_ascii $plain]} { return "" }
            return [angband_keymap trigger [scan [string tolower $plain] %c] 4 ""]
        }
    }
}

# What a key does: {kind text}, kind one of mine, game, command, free, none.
proc keymap_does {trigger} {
    global keymap
    if {$trigger eq ""} { return {none ""} }
    if {[dict exists $keymap(maps) $trigger]} {
        lassign [dict get $keymap(maps) $trigger] action user
        return [list [expr {$user ? "mine" : "game"}] [keymap_describe $action]]
    }
    set cmd [angband_keymap command $trigger]
    if {$cmd ne ""} { return [list command $cmd] }
    # The game has no use for F1, and the main window opens the manual with
    # it -- until the player binds it (main.tcl).
    if {$trigger eq {[F1]}} {
        return {free "nothing in the game: in the main window it opens the\
            manual, until you bind it"}
    }
    return {free ""}
}

# An action as a person reads it: its keys, and what the first one runs.
proc keymap_describe {action} {
    if {$action eq ""} { return "" }
    set keys [lmap k [angband_keymap readable $action] {
        expr {$k eq " " ? "Space" : $k}
    }]
    set text [join $keys " "]
    set cmd [angband_keymap command $action]
    if {$cmd ne ""} { append text " — $cmd" }
    return $text
}

proc keymap_readable {trigger} {
    set k [lindex [angband_keymap readable $trigger] 0]
    expr {$k eq " " ? "Space" : $k}
}

# --- the window ---------------------------------------------------------------------

proc keymap_build {} {
    global keymap

    toplevel .keymap
    wm title .keymap "Zangband: Keymaps"
    wm minsize .keymap 860 600

    set mb .keymap.mb
    menu $mb
    .keymap configure -menu $mb
    menu $mb.file -tearoff 0
    $mb add cascade -label File -underline 0 -menu $mb.file
    $mb.file add command -label "Close" -underline 0 \
        -command {destroy .keymap}
    # Not $mb.help: on macOS Tk treats a menu with that name as the platform's
    # own Help menu and fills it with things this window does not have.
    menu $mb.about
    $mb add cascade -label Help -underline 0 -menu $mb.about
    $mb.about add command -label "On Keymaps..." -underline 0 -accelerator F1 \
        -command keymap_help

    set w .keymap.f
    ttk::frame $w -padding 10
    pack $w -fill both -expand 1

    ttk::label $w.status -textvariable keymap(msg) -font [uifont 12] \
        -relief sunken -borderwidth 1 -anchor w -padding {4 1}
    pack $w.status -side bottom -fill x -pady {8 0}

    ttk::label $w.keyset -font [uifont 12] -textvariable keymap(keyset)
    pack $w.keyset -anchor w

    # The keyboard, then what the colours mean, then which layer it shows.
    canvas $w.kb -width 840 -height 230 -highlightthickness 1 \
        -background #d9d9d9 -takefocus 1
    pack $w.kb -fill x -pady {6 4}
    set lg [ttk::frame $w.legend]
    pack $lg -fill x
    foreach {kind text} {mine "Your keymap" game "The game's keymap"
            command "A command" free "Nothing" none "Not a key the game reads"} {
        canvas $lg.c$kind -width 14 -height 14 -highlightthickness 0 \
            -background [keymap_colour $kind]
        ttk::label $lg.l$kind -text $text -font [uifont 11]
        pack $lg.c$kind -side left -padx {0 4}
        pack $lg.l$kind -side left -padx {0 14}
    }
    set ly [ttk::frame $w.layers]
    pack $ly -fill x -pady {4 8}
    ttk::label $ly.l -text "Showing:" -font [uifont 12]
    pack $ly.l -side left
    foreach {layer text} {plain "The keys" shift "With Shift" control "With Control"} {
        ttk::radiobutton $ly.$layer -text $text -variable keymap(layer) \
            -value $layer -command keymap_layer_chosen
        pack $ly.$layer -side left -padx {6 0}
    }
    ttk::label $ly.hint -font [uifont 11] -foreground #555 \
        -text "  — or hold Shift or Control.  Press a key to choose it."
    pack $ly.hint -side left

    # Below: the chosen key on the left, the player's keymaps on the right.
    set lo [ttk::frame $w.lower]
    pack $lo -fill both -expand 1
    set sel [ttk::labelframe $lo.sel -text "The key" -padding 8]
    set yours [ttk::labelframe $lo.yours -text "Your keymaps" -padding 8]
    grid $sel -row 0 -column 0 -sticky nsew -padx {0 10}
    grid $yours -row 0 -column 1 -sticky nsew
    grid columnconfigure $lo 0 -weight 3
    grid columnconfigure $lo 1 -weight 2
    grid rowconfigure $lo 0 -weight 1

    ttk::label $sel.key -textvariable keymap(keyname) -font [uifont 20 bold]
    ttk::label $sel.now -textvariable keymap(now) -font [uifont 12] \
        -wraplength 420 -justify left
    grid $sel.key -row 0 -column 0 -columnspan 3 -sticky w
    grid $sel.now -row 1 -column 0 -columnspan 3 -sticky w -pady {0 8}

    ttk::label $sel.cl -text "Make it do a command:" -font [uifont 12]
    ttk::entry $sel.filter -textvariable keymap(filter) -width 24
    bind $sel.filter <KeyRelease> {keymap_fill_commands; break}
    grid $sel.cl -row 2 -column 0 -sticky w
    grid $sel.filter -row 2 -column 1 -columnspan 2 -sticky ew
    listbox $sel.cmds -height 8 -exportselection 0 -activestyle none \
        -yscrollcommand [list $sel.sb set] -font [uifont 12]
    ttk::scrollbar $sel.sb -orient vertical -command [list $sel.cmds yview]
    bind $sel.cmds <<ListboxSelect>> keymap_command_chosen
    grid $sel.cmds -row 3 -column 0 -columnspan 2 -sticky nsew -pady {4 0}
    grid $sel.sb -row 3 -column 2 -sticky ns -pady {4 0}

    ttk::label $sel.rl -text "Or keys pressed:" -font [uifont 12]
    ttk::button $sel.record -text "Record" -width 8 -command keymap_record
    ttk::button $sel.clear -text "Clear" -width 8 -command keymap_clear
    grid $sel.rl -row 4 -column 0 -sticky w -pady {8 0}
    grid $sel.record -row 4 -column 1 -sticky w -pady {8 0}
    grid $sel.clear -row 4 -column 2 -sticky w -pady {8 0}

    ttk::label $sel.will -textvariable keymap(will) -font [uifont 12] \
        -wraplength 420 -justify left
    grid $sel.will -row 5 -column 0 -columnspan 3 -sticky w -pady {8 0}

    set b [ttk::frame $sel.btns]
    ttk::button $b.bind -text "Bind" -width 10 -command keymap_bind
    ttk::button $b.remove -text "Remove" -width 10 -command keymap_remove
    pack $b.bind $b.remove -side left -padx {0 8}
    grid $b -row 6 -column 0 -columnspan 3 -sticky w -pady {10 0}
    grid columnconfigure $sel 1 -weight 1
    grid rowconfigure $sel 3 -weight 1

    ttk::treeview $yours.list -columns {key does} -show headings -height 10 \
        -selectmode browse -yscrollcommand [list $yours.sb set]
    $yours.list heading key -text "Key"
    $yours.list heading does -text "Does"
    $yours.list column key -width 90 -stretch 0
    $yours.list column does -width 220
    ttk::scrollbar $yours.sb -orient vertical -command [list $yours.list yview]
    pack $yours.sb -side right -fill y
    pack $yours.list -fill both -expand 1
    bind $yours.list <<TreeviewSelect>> keymap_yours_chosen

    # Keys: chosen by pressing them, or recorded while recording.  Not in the
    # filter, where typing is typing.  Escape is a key to bind like any other,
    # so it does not close the window; the File menu and Option-C do.
    bind .keymap <KeyPress> {keymap_key %W %N %s %A}
    foreach mod {Shift_L Shift_R Control_L Control_R} {
        bind .keymap <KeyPress-$mod> {keymap_hold %K 1}
        bind .keymap <KeyRelease-$mod> {keymap_hold %K 0}
    }
    bind .keymap <Key-F1> {keymap_help; break}
    accel_bind .keymap c {destroy .keymap}
    bind .keymap <FocusIn> {if {"%W" eq ".keymap"} keymap_refresh}
    $w.kb bind key <Enter> {keymap_hover current}
    $w.kb bind key <Leave> {set ::keymap(msg) ""}
    $w.kb bind key <ButtonRelease-1> {keymap_click current}

    keymap_keys
    keymap_fill_commands
}

bind . <<WindowTextChanged>> {+ if {[winfo exists .keymap]} keymap_draw }

proc keymap_colour {kind} {
    dict get {mine #a8dca0 game #c9def2 command #ffffff free #ededed
        none #bdbdbd mod #dcdcdc modon #ffd27f} $kind
}

# --- drawing ---------------------------------------------------------------------------

proc keymap_refresh {} {
    global keymap
    if {![winfo exists .keymap]} return

    set keymap(maps) [dict create]
    if {![catch {angband_keymap list} rows]} {
        foreach row $rows {
            lassign $row trigger action user
            # The first of a trigger is the one that wins, as keymap_find has it.
            if {![dict exists $keymap(maps) $trigger]} {
                dict set keymap(maps) $trigger [list $action $user]
            }
        }
    }
    set rogue 0
    if {![catch {angband_option} orows]} {
        set r [lsearch -inline -index 0 $orows rogue_like_commands]
        if {$r ne ""} { set rogue [lindex $r 3] }
        set keymap(character) 1
    } else {
        set keymap(character) 0
    }
    set keymap(keyset) [expr {$rogue
        ? "The roguelike keys, and their keymaps -- the original keys have their own."
        : "The original keys, and their keymaps -- the roguelike keys have their own."}]
    if {!$keymap(character)} {
        set keymap(keyset) "The game's keys.  Keymaps can be changed once a\
            character is in play."
    }
    keymap_draw
    keymap_fill_yours
    keymap_show_selected
}

proc keymap_draw {} {
    global keymap
    set c .keymap.f.kb
    $c delete all
    # Keys as large as their caps: the window text's size decides both.
    set u [ui_scaled 36]
    set pad 3
    set y 8
    set view $keymap(view)
    foreach row $keymap(rows,$keymap(type)) {
        set x 8
        foreach item $row {
            lassign [split $item :] name width
            if {$width eq ""} { set width 1 }
            set x2 [expr {$x + $width * $u - $pad}]
            set y2 [expr {$y + $u - $pad}]
            if {$name eq "gap"} {
                set x [expr {$x + $width * $u}]
                continue
            }
            if {$name in {Shift Control}} {
                set on [expr {$view eq [string tolower $name]}]
                $c create rectangle $x $y $x2 $y2 -width 1 -outline #8a8a8a \
                    -fill [keymap_colour [expr {$on ? "modon" : "mod"}]] \
                    -tags [list mod m$name]
                $c create text [expr {($x + $x2) / 2}] [expr {($y + $y2) / 2}] \
                    -text [string tolower $name] -font [uifont 10] \
                    -fill #444 -tags [list mod m$name]
                $c bind m$name <ButtonRelease-1> [list keymap_toggle_layer \
                    [string tolower $name]]
                set x [expr {$x + $width * $u}]
                continue
            }
            keymap_draw_key $c k$name $x $y $x2 $y2
            set x [expr {$x + $width * $u}]
        }
        set y [expr {$y + $u}]
    }
    # The arrows, to the right of the main block, as an inverted T.
    set ax [expr {8 + 15.5 * $u}]
    set ay [expr {8 + 4 * $u}]
    keymap_draw_key $c kUp [expr {$ax + $u}] $ay [expr {$ax + 2 * $u - $pad}] \
        [expr {$ay + $u - $pad}]
    set ay [expr {$ay + $u}]
    foreach name {Left Down Right} {
        keymap_draw_key $c k$name $ax $ay [expr {$ax + $u - $pad}] \
            [expr {$ay + $u - $pad}]
        set ax [expr {$ax + $u}]
    }
    # The keypad, level with the number row.
    set kx [expr {8 + 19 * $u}]
    set ky [expr {8 + $u}]
    foreach {name col row w h} $keymap(keypad) {
        set x [expr {$kx + $col * $u}]
        set y1 [expr {$ky + $row * $u}]
        keymap_draw_key $c k$name $x $y1 [expr {$x + $w * $u - $pad}] \
            [expr {$y1 + $h * $u - $pad}]
    }
    $c configure -height [expr {$y + 8}] -width [expr {8 + 23 * $u + 8}]
}

proc keymap_draw_key {c id x y x2 y2} {
    global keymap
    set trigger [keymap_trigger $id $keymap(view)]
    lassign [keymap_does $trigger] kind
    set key [dict get $keymap(keys) $id]
    if {[lindex $key 0] eq "special"} {
        set cap [lindex [dict get $keymap(special) [lindex $key 1]] 1]
        # A word small, a keypad digit or sign as large as any other key's.
        set font [expr {[string length $cap] == 1 ? [uifont 13] : [uifont 10]}]
    } else {
        lassign $key - - plain shifted
        set cap [string toupper $plain]
        if {$keymap(view) eq "shift"} { set cap $shifted }
        if {$keymap(view) eq "control"} { set cap "^[string toupper $plain]" }
        if {$keymap(view) eq "plain" && ![string is alpha -strict $plain]} {
            set cap $plain
        }
        set font [uifont 13]
    }
    set chosen [expr {$trigger ne "" && $trigger eq $keymap(selected)}]
    $c create rectangle $x $y $x2 $y2 -fill [keymap_colour $kind] \
        -outline [expr {$chosen ? "#c03a00" : "#8a8a8a"}] \
        -width [expr {$chosen ? 3 : 1}] -tags [list key $id]
    $c create text [expr {($x + $x2) / 2}] [expr {($y + $y2) / 2}] -text $cap \
        -font $font -fill [expr {$kind eq "none" ? "#7a7a7a" : "black"}] \
        -tags [list key $id]
}

# --- choosing a key -------------------------------------------------------------------------

proc keymap_id_at {c} {
    foreach t [$c gettags current] {
        if {[string match k* $t] && $t ne "key"} { return $t }
    }
    return ""
}

proc keymap_hover {what} {
    global keymap
    set id [keymap_id_at .keymap.f.kb]
    if {$id eq ""} return
    set trigger [keymap_trigger $id $keymap(view)]
    if {$trigger eq ""} {
        set keymap(msg) "The game does not read this key."
        return
    }
    lassign [keymap_does $trigger] kind text
    set keymap(msg) "[keymap_readable $trigger]: [keymap_what $kind $text]"
}

proc keymap_what {kind text} {
    switch -- $kind {
        mine { return "your keymap, $text" }
        game { return "the game's keymap, $text" }
        command { return $text }
        free { return [expr {$text ne "" ? $text : "nothing"}] }
        default { return "" }
    }
}

proc keymap_click {what} {
    global keymap
    set id [keymap_id_at .keymap.f.kb]
    if {$id eq ""} return
    set trigger [keymap_trigger $id $keymap(view)]
    if {$trigger eq ""} {
        set keymap(msg) "The game does not read this key, so it cannot be bound."
        return
    }
    keymap_select $trigger
}

proc keymap_select {trigger} {
    global keymap
    set keymap(selected) $trigger
    set keymap(pending) ""
    keymap_stop_recording
    keymap_draw
    keymap_show_selected
}

proc keymap_show_selected {} {
    global keymap
    set sel .keymap.f.lower.sel
    if {![winfo exists $sel]} return
    if {$keymap(selected) eq ""} {
        set keymap(keyname) "No key chosen"
        set keymap(now) "Press a key, or click one."
    } else {
        lassign [keymap_does $keymap(selected)] kind text
        set keymap(keyname) [keymap_readable $keymap(selected)]
        set keymap(now) "Now: [string toupper [keymap_what $kind $text] 0 0]."
        if {$kind eq "game"} {
            append keymap(now) "  The game's own keymaps come back with\
                every character, so bind it to something else to change it."
        }
    }
    keymap_show_pending
    set mine [expr {$keymap(selected) ne ""
        && [dict exists $keymap(maps) $keymap(selected)]
        && [lindex [dict get $keymap(maps) $keymap(selected)] 1]}]
    $sel.btns.remove state [expr {$mine && $keymap(character)
        ? "!disabled" : "disabled"}]
}

proc keymap_show_pending {} {
    global keymap
    set sel .keymap.f.lower.sel
    if {$keymap(recording)} {
        set keymap(will) "Recording: [keymap_describe $keymap(pending)]"
    } elseif {$keymap(pending) eq ""} {
        set keymap(will) ""
    } else {
        set keymap(will) "Will do: [keymap_describe $keymap(pending)]"
    }
    $sel.btns.bind state [expr {$keymap(selected) ne "" && $keymap(pending) ne ""
        && !$keymap(recording) && $keymap(character) ? "!disabled" : "disabled"}]
}

# A key pressed in the window: chosen, or recorded.  Not while typing in the
# filter, and not a bare modifier.
proc keymap_key {w keysym state char} {
    global keymap
    if {[winfo class $w] eq "TEntry"} return
    if {[catch {angband_keymap trigger $keysym $state $char} trigger]
            || $trigger eq ""} return
    if {$keymap(recording)} {
        set keys [llength [angband_keymap readable $keymap(pending)$trigger]]
        if {$keys > 20} {
            set keymap(msg) "A keymap is at most 20 keys."
            return -code break
        }
        append keymap(pending) $trigger
        keymap_show_pending
        return -code break
    }
    set keymap(view) [expr {$state & 4 ? "control" : ($state & 1 ? "shift" : "plain")}]
    set keymap(layer) $keymap(view)
    keymap_select $trigger
    return -code break
}

proc keymap_hold {keysym down} {
    global keymap
    if {$keymap(recording)} return
    set layer [expr {[string match Shift* $keysym] ? "shift" : "control"}]
    set keymap(view) [expr {$down ? $layer : $keymap(layer)}]
    keymap_draw
}

proc keymap_layer_chosen {} {
    global keymap
    set keymap(view) $keymap(layer)
    keymap_draw
    focus .keymap.f.kb
}

proc keymap_toggle_layer {layer} {
    global keymap
    set keymap(layer) [expr {$keymap(layer) eq $layer ? "plain" : $layer}]
    keymap_layer_chosen
}

# --- what to bind it to ---------------------------------------------------------------------

# The game's commands, by name, with their keys -- the same table the menus
# are made from, so each runs exactly what its menu entry does.
proc keymap_fill_commands {} {
    global keymap
    set lb .keymap.f.lower.sel.cmds
    if {![winfo exists $lb]} return
    $lb delete 0 end
    set keymap(commands) {}
    if {[catch {angband_commands} rows]} return
    set want [string tolower $keymap(filter)]
    foreach row [lsort -index 3 -dictionary $rows] {
        lassign $row gidx group idx label key enabled level
        if {$level != 0} continue
        if {$want ne "" && [string first $want [string tolower $label]] < 0} continue
        if {[catch {angband_keymap action $gidx $idx} action] || $action eq ""} continue
        lappend keymap(commands) $action
        $lb insert end [expr {$key eq "" ? $label : "$label    ($key)"}]
    }
}

proc keymap_command_chosen {} {
    global keymap
    set lb .keymap.f.lower.sel.cmds
    set i [lindex [$lb curselection] 0]
    if {$i eq ""} return
    keymap_stop_recording
    set keymap(pending) [lindex $keymap(commands) $i]
    keymap_show_pending
}

proc keymap_record {} {
    global keymap
    if {$keymap(recording)} {
        keymap_stop_recording
    } else {
        set keymap(recording) 1
        set keymap(pending) ""
        .keymap.f.lower.sel.record configure -text "Stop"
        .keymap.f.lower.sel.cmds selection clear 0 end
        set keymap(msg) "Press the keys, then Stop.  Every key counts,\
            Escape and Return included."
        focus .keymap.f.kb
    }
    keymap_show_pending
}

proc keymap_stop_recording {} {
    global keymap
    if {!$keymap(recording)} return
    set keymap(recording) 0
    .keymap.f.lower.sel.record configure -text "Record"
    set keymap(msg) ""
    keymap_show_pending
}

proc keymap_clear {} {
    global keymap
    keymap_stop_recording
    set keymap(pending) ""
    .keymap.f.lower.sel.cmds selection clear 0 end
    keymap_show_pending
}

# --- binding, and keeping it ------------------------------------------------------------------

proc keymap_bind {} {
    global keymap
    if {$keymap(selected) eq "" || $keymap(pending) eq ""} return
    if {[catch {angband_keymap set $keymap(selected) $keymap(pending)} err]} {
        set keymap(msg) [string toupper $err 0 0]
        return
    }
    keymap_keep "[keymap_readable $keymap(selected)] now does\
        [keymap_describe $keymap(pending)]."
    set keymap(pending) ""
    .keymap.f.lower.sel.cmds selection clear 0 end
}

proc keymap_remove {} {
    global keymap
    if {$keymap(selected) eq ""} return
    if {[catch {angband_keymap remove $keymap(selected)} err]} {
        set keymap(msg) [string toupper $err 0 0]
        return
    }
    keymap_keep "[keymap_readable $keymap(selected)] is the game's again."
}

# Every change is kept at once, for every character: there is no Save.
proc keymap_keep {said} {
    global keymap
    if {[catch {angband_keymap save} err]} {
        set keymap(msg) "$said  But it could not be kept: $err"
    } else {
        set keymap(msg) "$said  Kept for every character."
    }
    keymap_refresh
}

# --- the player's own keymaps ------------------------------------------------------------------

proc keymap_fill_yours {} {
    global keymap
    set t .keymap.f.lower.yours.list
    if {![winfo exists $t]} return
    $t delete [$t children {}]
    set keymap(yours) [dict create]
    dict for {trigger info} $keymap(maps) {
        lassign $info action user
        if {!$user} continue
        set id [$t insert {} end -values [list [keymap_readable $trigger] \
            [keymap_describe $action]]]
        dict set keymap(yours) $id $trigger
    }
}

proc keymap_yours_chosen {} {
    global keymap
    set t .keymap.f.lower.yours.list
    set id [lindex [$t selection] 0]
    if {$id eq "" || ![dict exists $keymap(yours) $id]} return
    set trigger [dict get $keymap(yours) $id]
    if {$trigger ne $keymap(selected)} { keymap_select $trigger }
}

proc keymap_help {} {
    help_window [help_first customize#keymaps customize]
}
