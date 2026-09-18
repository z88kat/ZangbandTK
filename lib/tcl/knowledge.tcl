# The monster knowledge browser.
#
# Decision 16 makes it a window rather than a pane; decision 17 draws it with
# the Classical design system, which is why this file is mostly a list of what
# goes where and hardly any of how it looks.
#
# It shows what the character knows and nothing else.  The list is the races
# they have seen or killed, and the recall is the game's own -- the same
# lore_description the look command and the knowledge screens use -- so this
# window cannot tell a player something their character has not learned, and
# cannot drift from what the game would say.

set knowledge(search) ""
set knowledge(rows) {}

proc knowledge_fill {} {
    global knowledge

    if {![winfo exists .knowledge]} return

    set pattern $knowledge(search)
    if {$pattern eq ""} {
        set pattern *
    } else {
        set pattern "*[string tolower $pattern]*"
    }

    if {[catch {angband_monster list $pattern} rows]} { set rows {} }
    set knowledge(rows) $rows

    set list .knowledge.card.body.cols.left.llist.list
    $list delete 0 end
    foreach row $rows {
        lassign $row idx name level unique kills
        # Uniques are worth picking out of a list of seven hundred, and the
        # game's own convention for that is the name alone, capitalised.
        $list insert end [format "%-34s %3s" \
            [expr {$unique ? [string totitle $name] : $name}] \
            [expr {$level == 0 ? "town" : $level}]]
    }

    set knowledge(count) "[llength $rows] known"
    knowledge_show
}

# Whatever is selected, or nothing.
proc knowledge_show {} {
    global knowledge

    set list .knowledge.card.body.cols.left.llist.list
    set sel [$list curselection]

    if {$sel eq "" || ![llength $knowledge(rows)]} {
        set knowledge(name) ""
        set knowledge(sub) ""
        foreach f $knowledge(fields) { set knowledge(v,$f) "—" }
        classical::prose_set .knowledge.card.body.cols.right.plore \
            "Choose a monster to read what your character has learned of it."
        return
    }

    set idx [lindex [lindex $knowledge(rows) [lindex $sel 0]] 0]
    set info [angband_monster info $idx]

    set knowledge(name) [string totitle [dict get $info name]]
    set knowledge(sub) [expr {[dict get $info unique]
        ? "a unique [dict get $info base]" : "[dict get $info base]"}]

    foreach f $knowledge(fields) {
        set knowledge(v,$f) [expr {[dict exists $info $f]
            ? [dict get $info $f] : "—"}]
    }
    set knowledge(v,level) [expr {[dict get $info level] == 0
        ? "Town" : "[expr {[dict get $info level] * 50}] ft"}]

    classical::prose_set .knowledge.card.body.cols.right.plore \
        [angband_monster lore $idx]
}

proc knowledge_window {} {
    global knowledge

    if {[winfo exists .knowledge]} {
        wm deiconify .knowledge
        raise .knowledge
        return
    }

    set card [classical::window .knowledge "Knowledge"]
    wm protocol .knowledge WM_DELETE_WINDOW { destroy .knowledge }
    wm geometry .knowledge 1100x720
    wm minsize .knowledge 720 480

    # Keys still play, except the ones the search box wants.  Bound on the
    # toplevel, so the entry sees them first and stops them going further.
    bind .knowledge <Key> { angband_key %N %s %A }
    bind .knowledge <Escape> { destroy .knowledge ; break }

    classical::titlestrip $card ZANGBAND KNOWLEDGE
    classical::footer $card { esc "close this window" } knowledge(count)

    set body $card.body
    frame $body -bg [classical::c bg]
    pack $body -fill both -expand 1 -padx [classical::sp 6] \
        -pady [list [classical::sp 4] [classical::sp 6]]

    set cols $body.cols
    frame $cols -bg [classical::c bg]
    pack $cols -fill both -expand 1

    # --- the list ---------------------------------------------------------
    set left $cols.left
    frame $left -bg [classical::c bg]
    classical::sectionhead $left monsters "MONSTERS" i
    pack $left.hmonsters -fill x -pady [list 0 [classical::sp 2]]

    pack [classical::searchbox $left search knowledge(search)] -fill x \
        -pady [list 0 [classical::sp 2]]
    pack [classical::scrolllist $left list 40] -fill both -expand 1

    # The search filters as it is typed; the list is short enough that
    # rebuilding it per keystroke is cheaper than being clever about it.
    trace add variable knowledge(search) write {apply {{a b c} {
        after idle knowledge_fill
    }}}
    bind $left.llist.list <<ListboxSelect>> { knowledge_show }

    # --- the recall -------------------------------------------------------
    set right $cols.right
    frame $right -bg [classical::c bg]

    label $right.name -textvariable knowledge(name) \
        -font [classical::f title] -bg [classical::c bg] \
        -fg [classical::c text] -anchor w
    label $right.sub -textvariable knowledge(sub) \
        -font [classical::f subhead] -bg [classical::c bg] \
        -fg [classical::c neutral-700] -anchor w
    pack $right.name $right.sub -fill x

    classical::sectionhead $right facts "FACTS" ii
    pack $right.hfacts -fill x -pady [list [classical::sp 4] 0]

    set knowledge(fields) {}
    set rows {
        level       "Native depth"
        speed       "Speed"
        avg_hp      "Hit points"
        armor_class "Armour class"
        exp         "Experience"
        sights      "Times seen"
        kills       "Slain, all lives"
    }
    set n [expr {[llength $rows] / 2}]
    set i 0
    foreach {field label} $rows {
        incr i
        lappend knowledge(fields) $field
        set knowledge(v,$field) "—"
        classical::statrow $right $field $label knowledge(v,$field) \
            [expr {$i == $n}]
    }

    classical::sectionhead $right lore "LORE" iii
    pack $right.hlore -fill x -pady [list [classical::sp 4] [classical::sp 2]]
    pack [classical::prose $right lore 8] -fill both -expand 1

    grid $left -row 0 -column 0 -sticky nsew -padx [list 0 [classical::sp 4]]
    grid $right -row 0 -column 1 -sticky nsew
    grid columnconfigure $cols 0 -weight 0 -minsize 320
    grid columnconfigure $cols 1 -weight 1
    grid rowconfigure $cols 0 -weight 1

    knowledge_fill
}

# The list changes when the character learns something, which is a kill or a
# first sighting.  Both of those are the game telling us the monster list moved.
foreach event {MONSTERLIST ENTER_GAME LEAVE_BIRTH} {
    bind . <<Angband_$event>> {+ if {[winfo exists .knowledge]} { knowledge_fill }}
}
