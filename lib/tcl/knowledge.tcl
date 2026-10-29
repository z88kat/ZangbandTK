# The knowledge browser.
#
# Decision 16 makes it a window rather than a pane.  Decision 18 builds it from
# Tk's own widgets in Tk's classic theme, to the handoff in
# .claude/plans/design_handoff_knowledge_tk.  Nothing here is drawn by hand and
# the window chooses no colours of its own: the controls are Ttk in the classic
# theme, the list and the text, which have no Ttk equivalent, take their
# colours from that theme too, and the glyph is in the map's colours.
#
# It shows what the character knows and nothing else.  The lists are what they
# have seen, and the descriptions are the game's own -- lore_description for a
# monster, object_info for an object kind -- so the window cannot tell a player
# something their character has not learned, and cannot drift from what the
# game says.
#
# One layout for every category: the list on the left, and a name, a table of
# facts and a description on the right.  Adding a category is a row in
# `categories` below and nothing else.

set knowledge(cat) monsters
set knowledge(sort) Depth
set knowledge(query) ""
set knowledge(rows) {}
set knowledge(count) ""
set knowledge(msg) ""
set knowledge(pos) ""
set knowledge(header) ""
set knowledge(name) ""
set knowledge(kind) ""
set knowledge(insc) ""

# {key Label accessor right depth {field Label ...}}
#
# The accessor takes `list`, `info` and `lore`, which is the shape every
# knowledge table in the front end uses.  `right` is what the list's second
# column shows: the depth, or the kind, which is what tells you that "Green" is
# a potion.  `depth` is the column of a list row that holds the native depth,
# for sorting, or -1 for a table that has none.
set knowledge(categories) {
    {monsters Monsters angband_monster depth 2 {
        level       "Native depth"
        speed       "Speed"
        avg_hp      "Hit points"
        armor_class "Armour class"
        exp         "Experience"
        sights      "Times seen"
        kills       "Slain, all lives"
    }}
    {objects Objects angband_object kind 4 {
        kind     "Kind"
        level    "Found at depth"
        cost     "Value"
        weight   "Weight"
        aware    "Identified"
    }}
    {artifacts Artifacts angband_artifact kind 3 {
        kind        "Kind"
        level       "Found at depth"
        cost        "Value"
        weight      "Weight"
        armor_class "Armour class"
        fully_known "Fully known"
    }}
    {egos "Ego items" angband_ego depth 2 {
        level   "From depth"
        deepest "To depth"
        cost    "Added value"
        rating  "Level rating"
    }}
    {runes Runes angband_rune kind -1 {
        kind "Kind"
    }}
    {features Terrain angband_feature kind -1 {
        kind    "Kind"
        digging "Digging difficulty"
        shop    "Shop number"
    }}
    {traps Traps angband_trap kind -1 {
        kind "Kind"
    }}
    {quests Quests angband_quest State -1 {
        kind   "Kind of errand"
        state  "Where it has got to"
        quarry "What it is about"
        sofar  "Done so far"
        needed "Needed in all"
        fixed  "Judged on it"
    }}
    {home Home angband_home kind -1 {
        kind   "Kind"
        number "How many"
        weight "Weight"
    }}
}

foreach c $knowledge(categories) {
    lassign $c key label accessor right depth fields
    set knowledge(label,$key) $label
    set knowledge(accessor,$key) $accessor
    set knowledge(right,$key) $right
    set knowledge(depth,$key) $depth
    set knowledge(fields,$key) $fields
}
unset c key label accessor right depth fields

# --- the list ----------------------------------------------------------------

# Monster names are lowercase by convention, so a heading capitalises the first
# letter.  Everything else arrives capitalised the way the game means it, and
# touching it turns "Wooden Torch" into "Wooden torch".
proc knowledge_title {key name} {
    if {$key ne "monsters"} { return $name }
    return "[string toupper [string index $name 0]][string range $name 1 end]"
}

# Cut rather than wrapped: the column to the right has to line up, and a name
# long enough to push it is rare enough that losing its tail costs less than a
# ragged table.
proc knowledge_cut {text width} {
    if {[string length $text] <= $width} { return $text }
    return "[string range $text 0 [expr {$width - 2}]]…"
}

# One row of the listbox.  The heading over it is formatted the same way in
# the same font, which is how the two columns line up without a table widget.
proc knowledge_line {key name right} {
    global knowledge
    if {$knowledge(right,$key) eq "depth"} {
        return [format "%-30s %5s" [knowledge_cut $name 30] $right]
    }
    return [format "%-26s %s" [knowledge_cut $name 26] $right]
}

proc knowledge_fill {} {
    global knowledge

    if {![winfo exists .knowledge]} return
    set key $knowledge(cat)
    set accessor $knowledge(accessor,$key)
    set lb .knowledge.f.body.list.lb

    # Before there is a character every table refuses, and an empty list is
    # the honest answer to that.
    if {[catch {$accessor list} all]} { set all {} }

    # Find matches anywhere in the name, through the same glob the C side
    # already takes.  Its wildcards are escaped, so typing one finds it.
    set q [string map {\\ \\\\ * \\* ? \\? [ \\[ ] \\]} $knowledge(query)]
    if {$q eq ""} {
        set rows $all
    } elseif {[catch {$accessor list "*$q*"} rows]} {
        set rows {}
    }

    # lsort is stable, so a depth sort leaves each depth in the game's order.
    set depth $knowledge(depth,$key)
    switch $knowledge(sort) {
        Name  { set rows [lsort -dictionary -index 1 $rows] }
        Depth {
            if {$depth >= 0} { set rows [lsort -integer -index $depth $rows] }
        }
    }
    set knowledge(rows) $rows

    $lb delete 0 end
    foreach row $rows {
        set name [lindex $row 1]
        if {$knowledge(right,$key) eq "depth"} {
            set right [lindex $row $depth]
            if {$right == 0} { set right town }
        } else {
            set right [lindex $row 2]
        }
        # A unique is a proper name.
        if {$key eq "monsters" && [lindex $row 3]} {
            set name [knowledge_title $key $name]
        }
        $lb insert end [knowledge_line $key $name $right]
    }

    set total [llength $all]
    set knowledge(count) "$total known"
    # The right-hand column's heading is the category's own word for it. This
    # was a fixed "Kind" for everything that was not a depth, which was true
    # while every such column held a kind -- the Quests tab's holds a state.
    set knowledge(header) [knowledge_line $key Name \
        [expr {$knowledge(right,$key) eq "depth"
            ? "Depth" : [string totitle $knowledge(right,$key)]}]]
    .knowledge.f.body.list configure \
        -text "$knowledge(label,$key) — $knowledge(count)"

    if {![llength $rows]} {
        set knowledge(msg) [expr {$total ? "No matches." : "Nothing is known yet."}]
        knowledge_show
        return
    }

    # Keep the one that was chosen in this category if it is still listed.
    set at 0
    if {[info exists knowledge(sel,$key)]} {
        set i [lsearch -exact -integer -index 0 $rows $knowledge(sel,$key)]
        if {$i >= 0} { set at $i }
    }
    knowledge_select $at
}

proc knowledge_select {i} {
    set lb .knowledge.f.body.list.lb
    $lb selection clear 0 end
    $lb selection set $i
    $lb activate $i
    $lb see $i
    knowledge_show
}

# Previous and Next.
proc knowledge_step {by} {
    global knowledge
    set sel [.knowledge.f.body.list.lb curselection]
    if {$sel eq ""} return
    set i [expr {$sel + $by}]
    if {$i < 0 || $i >= [llength $knowledge(rows)]} return
    knowledge_select $i
    set knowledge(msg) "Recalled: $knowledge(name)."
}

# --- the detail --------------------------------------------------------------

# One fact, as a reader wants it rather than as the table stores it.
proc knowledge_fact {key field info} {
    if {![dict exists $info $field]} { return "—" }
    set v [dict get $info $field]
    switch $field {
        level {
            if {$v == 0} { return Town }
            return "Level $v ([expr {$v * 50}] ft)"
        }
        speed {
            # 110 is normal speed, and the game says +10 rather than 120.
            set v [expr {$v - 110}]
            if {$v == 0} { return "Normal (+0)" }
            return [format %+d $v]
        }
        weight {
            # In tenths of a pound, as the game keeps it.
            return [format "%d.%d lb" [expr {$v / 10}] [expr {$v % 10}]]
        }
        aware - fully_known - fixed {
            return [expr {$v ? "yes" : "no"}]
        }
    }
    return $v
}

proc knowledge_kind {key info} {
    switch $key {
        monsters {
            return [expr {[dict get $info unique]
                ? "a unique [dict get $info base]" : [dict get $info base]}]
        }
        objects {
            return [expr {[dict get $info aware]
                ? [dict get $info kind]
                : "[dict get $info kind], not yet identified"}]
        }
        artifacts {
            return [expr {[dict get $info fully_known]
                ? "[dict get $info kind], and you have held it"
                : "[dict get $info kind], not yet in your hands"}]
        }
    }
    return [expr {[dict exists $info kind] ? [dict get $info kind] : ""}]
}

# The game wraps its own prose and doubles its spaces.  The text widget wraps,
# so the game's line breaks are noise; flatten them.
proc knowledge_prose {text} {
    return [regsub -all {\s+} [string trim $text] " "]
}

proc knowledge_text {text {head ""}} {
    set t .knowledge.f.body.detail.known.txt
    $t configure -state normal
    $t delete 1.0 end
    if {$head ne ""} { $t insert end $head head " " }
    $t insert end $text
    $t configure -state disabled
    $t yview moveto 0
}

proc knowledge_show {} {
    global knowledge

    set key $knowledge(cat)
    set w .knowledge.f
    set sel [$w.body.list.lb curselection]

    if {$sel eq "" || ![llength $knowledge(rows)]} {
        set knowledge(name) ""
        set knowledge(kind) ""
        $w.body.detail.head.glyph itemconfigure glyph -text ""
        grid remove $w.body.detail.head.glyph
        foreach {field label} $knowledge(fields,$key) {
            set knowledge(v,$field) "—"
        }
        set knowledge(pos) "0 of [llength $knowledge(rows)]"
        set knowledge(insc) ""
        knowledge_insc_state 0
        knowledge_text [expr {[llength $knowledge(rows)]
            ? "Choose one to read what your character has learned of it."
            : ""}]
        $w.btns.prev state disabled
        $w.btns.next state disabled
        return
    }

    set accessor $knowledge(accessor,$key)
    set idx [lindex $knowledge(rows) $sel 0]
    set knowledge(sel,$key) $idx
    set info [$accessor info $idx]

    set knowledge(name) [knowledge_title $key [dict get $info name]]
    set knowledge(kind) [knowledge_kind $key $info]

    # Ego items and runes have no glyph of their own, and an empty black box
    # would look like one that failed to draw.
    set g $w.body.detail.head.glyph
    if {[dict exists $info glyph]} {
        $g itemconfigure glyph -text [dict get $info glyph] \
            -fill [dict get $info colour]
        grid $g
    } else {
        grid remove $g
    }
    foreach {field label} $knowledge(fields,$key) {
        set knowledge(v,$field) [knowledge_fact $key $field $info]
    }
    set knowledge(insc) [expr {[dict exists $info inscription]
        ? [dict get $info inscription] : ""}]
    knowledge_insc_state 1

    # A monster's recall opens with its name and symbol, "The scruffy little
    # dog ('C')", which is set in bold as the heading of what follows.
    set lore [knowledge_prose [$accessor lore $idx]]
    if {$key eq "monsters" && [regexp {^(.*?\('.+?'\))\s*(.*)$} $lore -> head body]} {
        knowledge_text $body $head
    } else {
        knowledge_text $lore
    }

    set n [llength $knowledge(rows)]
    set knowledge(pos) "[expr {$sel + 1}] of $n"
    $w.btns.prev state [expr {$sel > 0 ? "!disabled" : "disabled"}]
    $w.btns.next state [expr {$sel < $n - 1 ? "!disabled" : "disabled"}]
}

# The facts are a grid of label and value, two pairs to a row, and which facts
# there are depends on the category, so the grid is rebuilt when it changes.
proc knowledge_facts {} {
    global knowledge

    set f .knowledge.f.body.detail.facts
    destroy {*}[winfo children $f]

    set i 0
    foreach {field label} $knowledge(fields,$knowledge(cat)) {
        set row [expr {$i / 2}]
        set col [expr {($i % 2) * 2}]
        set knowledge(v,$field) "—"
        ttk::label $f.l$field -text "$label:" -font [uifont 12]
        ttk::label $f.v$field -textvariable knowledge(v,$field) \
            -relief sunken -borderwidth 1 -anchor w -padding {4 1}
        grid $f.l$field -row $row -column $col -sticky e -pady 3 \
            -padx [expr {$col ? {18 6} : {0 6}}]
        grid $f.v$field -row $row -column [expr {$col + 1}] -sticky ew -pady 3
        incr i
    }
    grid columnconfigure $f {1 3} -weight 1 -uniform value

    set d .knowledge.f.body.detail
    if {$knowledge(cat) in {objects runes}} {
        pack $d.insc -after $d.facts -fill x -pady {8 0}
    } else {
        pack forget $d.insc
    }
}

# --- auto-inscriptions -----------------------------------------------------------

# Objects and runes can be inscribed for good, as the game's knowledge screen
# does with { and }: everything of that kind, or bearing that rune, is
# inscribed when it is found, and what is carried now is inscribed at once.  A
# kind not yet identified is inscribed by its look, as there.
proc knowledge_insc_state {on} {
    set i .knowledge.f.body.detail.insc
    if {![winfo exists $i]} return
    foreach w [list $i.e $i.set $i.clear] {
        $w state [expr {$on ? "!disabled" : "disabled"}]
    }
}

proc knowledge_inscribe {{erase 0}} {
    global knowledge

    set key $knowledge(cat)
    if {$key ni {objects runes} || ![info exists knowledge(sel,$key)]} return
    set text [expr {$erase ? "" : [string trim $knowledge(insc)]}]
    if {[catch {$knowledge(accessor,$key) inscribe $knowledge(sel,$key) $text} err]} {
        set knowledge(msg) [string toupper $err 0 0]
        bell
        return
    }
    set knowledge(insc) $text
    set knowledge(msg) [expr {$text eq ""
        ? "$knowledge(name) is no longer inscribed when found."
        : "$knowledge(name) is inscribed $text when found."}]
}

# As the options screen's "Save autoinscriptions to pref file", into the pref
# file every character reads.  Only the identified kinds' go, as there.
proc knowledge_save_inscriptions {} {
    global knowledge
    if {[catch {angband_object dump} path]} {
        set knowledge(msg) [string toupper $path 0 0]
        bell
        return
    }
    set knowledge(msg) "Saved the inscriptions to [file tail $path], for\
        every character."
}

proc knowledge_switch {} {
    global knowledge
    if {![winfo exists .knowledge]} return
    knowledge_facts
    .knowledge.f.body.list.lb yview moveto 0
    knowledge_fill
    if {[llength $knowledge(rows)]} {
        set knowledge(msg) "$knowledge(label,$knowledge(cat)): $knowledge(count)."
    }
}

# Find filters as it is typed.  The lists are short enough that rebuilding one
# per keystroke is cheaper than being clever about it, but a burst of
# keystrokes is still one rebuild.
proc knowledge_typed {args} {
    global knowledge
    if {[info exists knowledge(pending)]} return
    set knowledge(pending) 1
    after idle {
        unset -nocomplain knowledge(pending)
        knowledge_fill
    }
}
trace add variable knowledge(query) write knowledge_typed

# --- the menus' commands -------------------------------------------------------

proc knowledge_dump {} {
    global knowledge

    set key $knowledge(cat)
    set accessor $knowledge(accessor,$key)
    set file [tk_getSaveFile -parent .knowledge -title "Dump to File" \
        -initialfile "$key.txt"]
    if {$file eq ""} return

    if {[catch {
        set out [open $file w]
        fconfigure $out -encoding utf-8
        foreach row $knowledge(rows) {
            set idx [lindex $row 0]
            puts $out [knowledge_title $key [lindex $row 1]]
            puts $out [knowledge_prose [$accessor lore $idx]]
            puts $out ""
        }
        close $out
    } err]} {
        catch {close $out}
        set knowledge(msg) "Could not write [file tail $file]: $err"
        return
    }
    set knowledge(msg) "Wrote [llength $knowledge(rows)] to [file tail $file]."
}

# F1: the manual on the game's knowledge menu, which this window is, and how
# to use the window in the status line.
proc knowledge_help {} {
    global knowledge
    help_window [help_first command#game-status-commands command]
    set knowledge(msg) "Choose a category, then an entry, to read what your\
        character has learned about it.  Find narrows the list as you type."
}

# What the window uses itself stays here: what is typed into Find, and the
# keys that move about the list.  Everything else goes to the game, so the
# window can be left open while you play.
proc knowledge_key {w keysym n s a} {
    switch -- [winfo class $w] {
        TEntry  { return }
        Listbox { if {$keysym in {Up Down Prior Next Home End}} return }
    }
    angband_key $n $s $a
}

# --- the window ----------------------------------------------------------------

proc knowledge_menus {} {
    global knowledge

    set mb .knowledge.mb
    menu $mb
    .knowledge configure -menu $mb

    menu $mb.file
    $mb add cascade -label File -underline 0 -menu $mb.file
    $mb.file add command -label "Dump to File..." -underline 0 \
        -accelerator Ctrl+S -command knowledge_dump
    $mb.file add command -label "Save Inscriptions" -underline 5 \
        -command knowledge_save_inscriptions
    $mb.file add separator
    $mb.file add command -label Close -underline 0 -command {destroy .knowledge}

    menu $mb.view
    $mb add cascade -label View -underline 0 -menu $mb.view
    foreach c $knowledge(categories) {
        set key [lindex $c 0]
        $mb.view add radiobutton -label $knowledge(label,$key) \
            -variable knowledge(cat) -value $key -command knowledge_switch
    }

    # Not $mb.help: on macOS Tk treats a menu with that name as the platform's
    # own Help menu and fills it with things this window does not have.
    menu $mb.about
    $mb add cascade -label Help -underline 0 -menu $mb.about
    $mb.about add command -label "On Knowledge..." -underline 0 \
        -accelerator F1 -command knowledge_help
}

proc knowledge_window {} {
    global knowledge

    if {[winfo exists .knowledge]} {
        wm deiconify .knowledge
        raise .knowledge
        return
    }

    toplevel .knowledge
    wm title .knowledge "Zangband: Knowledge"
    wm protocol .knowledge WM_DELETE_WINDOW { destroy .knowledge }
    wm geometry .knowledge 1100x740
    wm minsize .knowledge 940 560

    knowledge_menus

    # One Ttk frame under everything, because that is what carries the theme's
    # grey: the toplevel itself is drawn by the platform.
    set w .knowledge.f
    ttk::frame $w
    pack $w -fill both -expand 1

    # The list and the text are classic widgets, Ttk having neither, so they
    # are given the colours the theme gives an entry.  The classic theme never
    # sets a field colour, and its entry falls back to white, so that is the
    # default asked for here.
    set ground [ttk::style lookup . -background]
    set field  [ttk::style lookup TEntry -fieldbackground {} white]
    set chosen [ttk::style lookup . -selectbackground]

    # --- category
    ttk::frame $w.cat
    pack $w.cat -fill x -padx 10 -pady {10 4}
    ttk::label $w.cat.l -text "Category:"
    pack $w.cat.l -side left
    foreach c $knowledge(categories) {
        set key [lindex $c 0]
        ttk::radiobutton $w.cat.$key -text $knowledge(label,$key) \
            -variable knowledge(cat) -value $key -command knowledge_switch
        pack $w.cat.$key -side left -padx {16 0}
    }

    # --- find and sort
    ttk::frame $w.tools
    pack $w.tools -fill x -padx 10 -pady 4
    ttk::label $w.tools.fl -text "Find:" -underline 0
    ttk::entry $w.tools.find -textvariable knowledge(query) -font [uifont Courier 12]
    ttk::label $w.tools.sl -text "Sort by:"
    ttk::menubutton $w.tools.sort -textvariable knowledge(sort) \
        -menu $w.tools.sort.m -width 8
    menu $w.tools.sort.m -tearoff 0
    foreach s {Depth Name} {
        $w.tools.sort.m add radiobutton -label $s \
            -variable knowledge(sort) -value $s -command knowledge_fill
    }
    pack $w.tools.fl -side left
    pack $w.tools.find -side left -fill x -expand 1 -padx {4 16}
    pack $w.tools.sl -side left
    pack $w.tools.sort -side left -padx {4 0}

    # --- status, buttons and the rule over them, packed before the body so
    # that they keep their room when the window is made smaller
    ttk::frame $w.status
    pack $w.status -side bottom -fill x -padx 2 -pady 2
    ttk::label $w.status.msg -textvariable knowledge(msg) -font [uifont 12] \
        -relief sunken -borderwidth 1 -anchor w -padding {4 1}
    ttk::label $w.status.pos -textvariable knowledge(pos) -font [uifont 12] \
        -relief sunken -borderwidth 1 -anchor center -width 12 -padding {4 1}
    pack $w.status.pos -side right
    pack $w.status.msg -side left -fill x -expand 1 -padx {0 2}

    ttk::frame $w.btns
    pack $w.btns -side bottom -fill x -padx 10 -pady 10
    ttk::button $w.btns.close -text Close -underline 0 -width 10 \
        -default active -command {destroy .knowledge}
    ttk::button $w.btns.next -text Next -underline 0 -width 10 \
        -command {knowledge_step 1}
    ttk::button $w.btns.prev -text Previous -underline 0 -width 10 \
        -command {knowledge_step -1}
    pack $w.btns.close $w.btns.next $w.btns.prev -side right -padx {10 0}

    ttk::separator $w.sep -orient horizontal
    pack $w.sep -side bottom -fill x

    # --- the body: the list, and the detail beside it
    set body $w.body
    ttk::frame $body
    pack $body -fill both -expand 1 -padx 10 -pady {6 0}

    set l $body.list
    ttk::labelframe $l -padding 6
    ttk::label $l.hdr -textvariable knowledge(header) -font [uifont Courier 12] \
        -padding {3 0}
    listbox $l.lb -width 40 -height 22 -font [uifont Courier 12] \
        -selectmode browse -exportselection 0 -activestyle none \
        -background $field -foreground black \
        -selectbackground $chosen -selectforeground black -selectborderwidth 1 \
        -relief sunken -borderwidth 2 -highlightthickness 1 \
        -highlightcolor black -highlightbackground $ground \
        -yscrollcommand [list $l.sb set]
    ttk::scrollbar $l.sb -orient vertical -command [list $l.lb yview]
    grid $l.hdr -row 0 -column 0 -sticky w
    grid $l.lb  -row 1 -column 0 -sticky nsew
    grid $l.sb  -row 1 -column 1 -sticky ns
    grid rowconfigure $l 1 -weight 1
    grid columnconfigure $l 0 -weight 1

    set d $body.detail
    ttk::frame $d

    # The glyph is what the map draws for it in text mode, in the map's
    # colours, which is why the ground is black whatever the theme says.
    set h $d.head
    ttk::frame $h
    canvas $h.glyph -width 40 -height 40 -background black \
        -relief sunken -borderwidth 2 -highlightthickness 0
    $h.glyph create text 22 22 -tags glyph -anchor center \
        -font [uifont Courier 20 bold]
    ttk::label $h.name -textvariable knowledge(name) -font [uifont 18 bold]
    ttk::label $h.kind -textvariable knowledge(kind) -font [uifont 12 italic]
    grid $h.glyph -row 0 -column 0 -rowspan 2 -sticky w -padx {0 10}
    grid $h.name  -row 0 -column 1 -sticky w
    grid $h.kind  -row 1 -column 1 -sticky w

    ttk::labelframe $d.facts -text Facts -padding 8

    # Packed by knowledge_facts, for objects and runes.
    set i [ttk::labelframe $d.insc -text "Inscribe When Found" -padding 8]
    ttk::entry $i.e -textvariable knowledge(insc) -font [uifont Courier 12]
    ttk::button $i.set -text Inscribe -underline 0 -width 9 \
        -command knowledge_inscribe
    ttk::button $i.clear -text Clear -width 7 -command {knowledge_inscribe 1}
    pack $i.clear -side right
    pack $i.set -side right -padx {6 6}
    pack $i.e -side left -fill x -expand 1
    bind $i.e <Return> { knowledge_inscribe ; break }
    ttk::labelframe $d.known -text "What Is Known" -padding 6
    text $d.known.txt -height 8 -width 40 -wrap word -font [uifont Courier 12] \
        -background $field -foreground black \
        -relief sunken -borderwidth 2 -highlightthickness 1 \
        -highlightcolor black -highlightbackground $ground -padx 4 -pady 4 \
        -yscrollcommand [list $d.known.sb set]
    $d.known.txt tag configure head -font [uifont Courier 12 bold]
    ttk::scrollbar $d.known.sb -orient vertical -command [list $d.known.txt yview]
    pack $d.known.sb -side right -fill y
    pack $d.known.txt -side left -fill both -expand 1
    pack $h -fill x
    pack $d.facts -fill x -pady {8 0}
    pack $d.known -fill both -expand 1 -pady {8 0}

    grid $l -row 0 -column 0 -sticky nsew -padx {0 12}
    grid $d -row 0 -column 1 -sticky nsew
    grid columnconfigure $body 1 -weight 1
    grid rowconfigure $body 0 -weight 1

    bind $l.lb <<ListboxSelect>> {
        knowledge_show
        set knowledge(msg) "Recalled: $knowledge(name)."
    }

    bind .knowledge <Key> { knowledge_key %W %K [key_sym %N %k] %s %A }
    bind .knowledge <Escape> { destroy .knowledge ; break }
    bind .knowledge <Control-s> { knowledge_dump ; break }
    bind .knowledge <F1> { knowledge_help ; break }
    accel_bind .knowledge f { focus .knowledge.f.tools.find ; break }
    accel_bind .knowledge p { .knowledge.f.btns.prev invoke ; break }
    accel_bind .knowledge n { .knowledge.f.btns.next invoke ; break }
    accel_bind .knowledge c { destroy .knowledge ; break }
    accel_bind .knowledge i { knowledge_inscribe ; break }

    knowledge_switch
    focus $l.lb
}

# The lists change when the character learns something: a kill, a first
# sighting, an identification.  Once for each burst of events: see
# refresh_soon.  knowledge_fill returns at once when the window is shut.
foreach event {MONSTERLIST ITEMLIST INVENTORY ENTER_GAME LEAVE_BIRTH} {
    bind . <<Angband_$event>> {+ refresh_soon knowledge_fill }
}
