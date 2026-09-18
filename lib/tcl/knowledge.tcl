# The knowledge browser.
#
# Decision 16 makes it a window rather than a pane; decision 17 draws it with
# the Classical design system, which is why this file is mostly a list of what
# goes where and hardly any of how it looks.
#
# It shows what the character knows and nothing else.  The lists are what they
# have seen, and the descriptions are the game's own -- lore_description for a
# monster, object_info for an object kind -- so the window cannot tell a player
# something their character has not learned, and cannot drift from what the
# game says.
#
# One pane per category, all the same shape: a search box over a list on the
# left, a name, a table of facts and a description on the right.  Adding
# artifacts, ego items, runes, features or traps is a row in `categories`
# below and nothing else, which is the point of writing it this way.

set knowledge(tab) monsters

# {key Label accessor {field Label ...}}
#
# The accessor is a command that takes `list`, `info` and `lore`, which is the
# shape every knowledge table in the front end uses.
set knowledge(categories) {
    {monsters MONSTERS angband_monster {
        level       "Native depth"
        speed       "Speed"
        avg_hp      "Hit points"
        armor_class "Armour class"
        exp         "Experience"
        sights      "Times seen"
        kills       "Slain, all lives"
    }}
    {objects OBJECTS angband_object {
        kind     "Kind"
        level    "Found at depth"
        cost     "Value"
        weight   "Weight"
        aware    "Identified"
    }}
    {artifacts ARTIFACTS angband_artifact {
        kind        "Kind"
        level       "Found at depth"
        cost        "Value"
        weight      "Weight"
        armor_class "Armour class"
        fully_known "Fully known"
    }}
    {egos "EGO ITEMS" angband_ego {
        level   "From depth"
        deepest "To depth"
        cost    "Added value"
        rating  "Level rating"
    }}
    {runes RUNES angband_rune {
        kind "Kind"
    }}
    {features TERRAIN angband_feature {
        kind    "Kind"
        digging "Digging difficulty"
        shop    "Shop number"
    }}
    {traps TRAPS angband_trap {
        kind "Kind"
    }}
}

proc knowledge_cat {key what} {
    global knowledge
    foreach c $knowledge(categories) {
        lassign $c k label accessor fields
        switch $what {
            label    { if {$k eq $key} { return $label } }
            accessor { if {$k eq $key} { return $accessor } }
            fields   { if {$k eq $key} { return $fields } }
        }
    }
    return ""
}

# --- filling ----------------------------------------------------------------

proc knowledge_fill {{key ""}} {
    global knowledge

    if {![winfo exists .knowledge]} return
    if {$key eq ""} { set key $knowledge(tab) }

    set pattern $knowledge(search,$key)
    if {$pattern eq ""} {
        set pattern *
    } else {
        set pattern "*$pattern*"
    }

    set accessor [knowledge_cat $key accessor]
    if {[catch {$accessor list $pattern} rows]} { set rows {} }
    set knowledge(rows,$key) $rows

    set list .knowledge.card.body.$key.left.llist.list
    $list delete 0 end
    foreach row $rows {
        lassign $row idx name a b

        # Monsters capitalise a unique; objects have no equivalent, so the
        # third field is read per category rather than assumed.
        if {$key eq "monsters" && $b} { set name [string totitle $name] }

        # Cut rather than wrapped: the column to the right has to line up, and
        # a name long enough to push it is rare enough that losing its tail
        # costs less than a ragged table.
        if {[string length $name] > 30} {
            set name "[string range $name 0 28]…"
        }

        if {$key in {monsters egos}} {
            # A depth.  Ego items carry the shallowest they appear at, which
            # is the same question asked of a monster.
            set right [expr {$a == 0 ? "town" : $a}]
        } else {
            # Everything else puts its kind here: it is what tells you that
            # "Green" is a potion.
            set right $a
        }
        $list insert end [format "%-30s %5s" $name $right]
    }

    set knowledge(count) "[llength $rows] known"
    knowledge_show $key
}

proc knowledge_show {{key ""}} {
    global knowledge

    if {![winfo exists .knowledge]} return
    if {$key eq ""} { set key $knowledge(tab) }

    set list .knowledge.card.body.$key.left.llist.list
    set sel [$list curselection]
    set fields [knowledge_cat $key fields]

    if {$sel eq "" || ![llength $knowledge(rows,$key)]} {
        set knowledge(name,$key) ""
        set knowledge(sub,$key) ""
        foreach {f label} $fields { set knowledge(v,$key,$f) "—" }
        classical::prose_set .knowledge.card.body.$key.right.plore \
            "Choose one to read what your character has learned of it."
        return
    }

    set accessor [knowledge_cat $key accessor]
    set idx [lindex [lindex $knowledge(rows,$key) [lindex $sel 0]] 0]
    set info [$accessor info $idx]

    # Monsters only.  Their names are lowercase by convention, so the heading
    # capitalises the first letter; object and artifact names arrive already
    # capitalised the way the game means them, and touching those turns
    # "Wooden Torch" into "Wooden torch".
    set n [dict get $info name]
    if {$key eq "monsters"} {
        set n "[string toupper [string index $n 0]][string range $n 1 end]"
    }
    set knowledge(name,$key) $n

    switch $key {
        monsters {
            set knowledge(sub,$key) [expr {[dict get $info unique]
                ? "a unique [dict get $info base]" : [dict get $info base]}]
        }
        objects {
            set knowledge(sub,$key) [expr {[dict get $info aware]
                ? [dict get $info kind]
                : "[dict get $info kind], not yet identified"}]
        }
        artifacts {
            set knowledge(sub,$key) [expr {[dict get $info fully_known]
                ? "[dict get $info kind], and you have held it"
                : "[dict get $info kind], not yet in your hands"}]
        }
        default {
            set knowledge(sub,$key) [expr {[dict exists $info kind]
                ? [dict get $info kind] : ""}]
        }
    }

    foreach {f label} $fields {
        set knowledge(v,$key,$f) [expr {[dict exists $info $f]
            ? [dict get $info $f] : "—"}]
    }

    # Depths read as depths, not as bare numbers.
    if {[dict exists $info level]} {
        set knowledge(v,$key,level) [expr {[dict get $info level] == 0
            ? "Town" : "[expr {[dict get $info level] * 50}] ft"}]
    }
    if {$key eq "objects"} {
        set knowledge(v,$key,aware) [expr {[dict get $info aware]
            ? "yes" : "no"}]
    }
    if {$key eq "artifacts"} {
        set knowledge(v,$key,fully_known) [expr {[dict get $info fully_known]
            ? "yes" : "no"}]
    }

    classical::prose_set .knowledge.card.body.$key.right.plore \
        [$accessor lore $idx]
}

proc knowledge_switch {} {
    global knowledge

    foreach c $knowledge(categories) {
        lassign $c key label accessor fields
        if {$key eq $knowledge(tab)} {
            grid .knowledge.card.body.$key -row 0 -column 0 -sticky nsew
        } else {
            grid remove .knowledge.card.body.$key
        }
    }
    knowledge_fill
}

# --- the window --------------------------------------------------------------

proc knowledge_pane {parent key fields} {
    global knowledge

    set pane $parent.$key
    frame $pane -bg [classical::c bg]

    # --- the list ---------------------------------------------------------
    set left $pane.left
    frame $left -bg [classical::c bg]

    set knowledge(search,$key) ""
    pack [classical::searchbox $left search knowledge(search,$key) \
        "search by name"] -fill x -pady [list 0 [classical::sp 2]]
    pack [classical::scrolllist $left list 40] -fill both -expand 1

    # Filters as it is typed; the lists are short enough that rebuilding one
    # per keystroke is cheaper than being clever about it.
    trace add variable ::knowledge(search,$key) write \
        [list apply {{key a b c} { after idle [list knowledge_fill $key] }} $key]
    bind $left.llist.list <<ListboxSelect>> [list knowledge_show $key]

    # --- the detail -------------------------------------------------------
    set right $pane.right
    frame $right -bg [classical::c bg]

    set knowledge(name,$key) ""
    set knowledge(sub,$key) ""
    label $right.name -textvariable knowledge(name,$key) \
        -font [classical::f title] -bg [classical::c bg] \
        -fg [classical::c text] -anchor w
    label $right.sub -textvariable knowledge(sub,$key) \
        -font [classical::f subhead] -bg [classical::c bg] \
        -fg [classical::c neutral-700] -anchor w
    pack $right.name $right.sub -fill x

    classical::sectionhead $right facts "FACTS" i
    pack $right.hfacts -fill x -pady [list [classical::sp 4] 0]

    set n [expr {[llength $fields] / 2}]
    set i 0
    foreach {field label} $fields {
        incr i
        set knowledge(v,$key,$field) "—"
        classical::statrow $right $field $label knowledge(v,$key,$field) \
            [expr {$i == $n}]
    }

    classical::sectionhead $right lore "WHAT IS KNOWN" ii
    pack $right.hlore -fill x -pady [list [classical::sp 4] [classical::sp 2]]
    pack [classical::prose $right lore 8] -fill both -expand 1

    grid $left -row 0 -column 0 -sticky nsew -padx [list 0 [classical::sp 4]]
    grid $right -row 0 -column 1 -sticky nsew
    grid columnconfigure $pane 0 -weight 0 -minsize 320
    grid columnconfigure $pane 1 -weight 1
    grid rowconfigure $pane 0 -weight 1

    return $pane
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
    wm geometry .knowledge 1100x740
    wm minsize .knowledge 720 480

    bind .knowledge <Key> { angband_key %N %s %A }
    bind .knowledge <Escape> { destroy .knowledge ; break }

    classical::titlestrip $card ZANGBAND KNOWLEDGE
    classical::footer $card { esc "close this window" } knowledge(count)

    set tabs {}
    foreach c $knowledge(categories) {
        lassign $c key label accessor fields
        lappend tabs $key $label
    }
    set strip [classical::tabstrip $card cats $tabs knowledge(tab) \
        knowledge_switch]
    pack $strip -fill x -padx [classical::sp 6] \
        -pady [list [classical::sp 4] [classical::sp 3]]

    # Every pane in the same cell; switching raises one and removes the rest.
    set body $card.body
    frame $body -bg [classical::c bg]
    pack $body -fill both -expand 1 -padx [classical::sp 6] \
        -pady [list 0 [classical::sp 6]]
    grid columnconfigure $body 0 -weight 1
    grid rowconfigure $body 0 -weight 1

    foreach c $knowledge(categories) {
        lassign $c key label accessor fields
        knowledge_pane $body $key $fields
    }

    classical::tab_paint $strip knowledge(tab)
    knowledge_switch
}

# The lists change when the character learns something: a kill, a first
# sighting, an identification.
foreach event {MONSTERLIST ITEMLIST INVENTORY ENTER_GAME LEAVE_BIRTH} {
    bind . <<Angband_$event>> {+ if {[winfo exists .knowledge]} { knowledge_fill }}
}
