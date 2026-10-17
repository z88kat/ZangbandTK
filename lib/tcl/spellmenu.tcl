# The Spells menu: realm, then book, then spell.
#
# The Spell Book window already lists spells and casts them, so this is not a
# second way to read them -- it is a way to cast one without opening anything.
# On this platform the menu bar is the surface a player looks at first, and
# until now nothing on it mentioned magic at all: casting was the keyboard's
# 'm' or a window you had to know about.
#
# What it shows is angband_book's, the same accessor the window reads, so the
# two cannot drift. What it does is angband_book cast, which asks
# command_may_start() and pushes CMD_CAST, so the turn, the "not enough mana,
# attempt it anyway?" question and the aiming are the game's.
#
# Rebuilt every time it is posted rather than when something changes. A menu
# is only looked at while it is open, the pack and the character's mana move
# under it constantly, and -postcommand is the one moment both are settled.
# The alternative is a dozen event bindings keeping a menu current that nobody
# is looking at.

namespace eval spellmenu {
    variable P
    set P(msg) ""
}

# The numbers a caster acts on, for the right-hand column.
#
# Both are what the game would really do, not what the book says. The failure
# chance is spell_chance(), which has the character's stat, level, stun and
# the shortfall if they are low on mana already in it -- the book's own sfail
# is only its starting point. The mana is what the cast will actually take.
proc spellmenu_numbers {mana fail} {
    return "$mana mana · $fail%"
}

# Why this entry cannot be used, or "" if it can.
#
# Inert comes first because it is the one a player cannot discover any other
# way: the spell is known, it is castable as far as the game is concerned, and
# casting it spends the turn and the mana and does nothing. Two spells are
# like this. Greying them out is a judgement -- the keyboard will still cast
# them -- and the reason is in the label so that it reads as a statement about
# the spell rather than as the menu being broken.
proc spellmenu_why {state castable inert} {
    if {$inert} { return "nothing yet" }
    if {!$castable} { return $state }
    return ""
}

# Add one spell to a menu.
proc spellmenu_spell {m row} {
    lassign $row sidx name level mana fail info state castable learnable inert

    set why [spellmenu_why $state $castable $inert]
    set label $name
    if {$why ne ""} { append label " — $why" }

    $m add command -label $label \
        -accelerator [spellmenu_numbers $mana $fail] \
        -command [list spellmenu_cast $sidx]
    if {$why ne ""} { $m entryconfigure end -state disabled }
}

# Fill a menu with one book's spells.
proc spellmenu_book {m n} {
    if {[catch {angband_book spells $n} rows] || ![llength $rows]} {
        $m add command -label "Nothing in it" -state disabled
        return
    }
    foreach row $rows { spellmenu_spell $m $row }
}

# Build the whole thing, from whatever is in the pack right now.
proc spellmenu_fill {m} {
    # Everything below this menu is rebuilt, so the old entries and the old
    # submenus both go. destroy on the children, not just delete on the
    # entries, or the submenu widgets accumulate one set per posting.
    $m delete 0 end
    foreach child [winfo children $m] { destroy $child }

    if {[catch {angband_book books} books]} { set books {} }
    if {![llength $books]} {
        $m add command -label "No spell books" -state disabled
        return
    }

    # Group by realm, keeping the pack's order within each.
    set realms {}
    foreach row $books {
        lassign $row n name realm
        if {$realm eq ""} { set realm "other" }
        dict lappend realms $realm $row
    }

    # One realm is the ordinary case, and a cascade with a single item in it
    # is a step the player has to take for no reason. So the realm level
    # appears only when there is more than one to choose between.
    set many [expr {[dict size $realms] > 1}]
    set i 0
    dict for {realm rows} $realms {
        if {$many} {
            set rm $m.r[incr i]
            menu $rm -tearoff 0
            $m add cascade -label [string totitle $realm] -menu $rm
        } else {
            set rm $m
        }
        foreach row $rows {
            lassign $row n name realm2
            set bm $rm.b$n
            menu $bm -tearoff 0
            $rm add cascade -label $name -menu $bm
            spellmenu_book $bm $n
        }
    }
}

# Cast it, and let the game say what it thinks.
#
# A refusal leaves nothing changed and is recorded rather than thrown away:
# "the game is in the middle of something else" is the guard doing its job and
# the player should be able to find out that is what happened. The menu is
# rebuilt on the next posting, so nothing here has to put a display back.
proc spellmenu_cast {sidx} {
    if {[catch {angband_book cast $sidx} err]} {
        errorsink::record "cast from the Spells menu" $err $::errorInfo
        bell
    }
}
