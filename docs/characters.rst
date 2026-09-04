Races and Classes
=================

Angband gives you eleven races and nine classes. ZangbandTK has twenty and
fourteen, and the new ones are not variations on the old — they are the reason
the game feels different from the first screen.

:doc:`birth` has the full descriptions and the tables. This page is what is
different from Angband, and why.

Nine new races
--------------

Curated, not imported. Zangband has twenty-one races Angband does not; nine are
in, four are turned down on the evidence, and eight are waiting. The rule was
that a race has to add a decision rather than a line on the menu — three of
Zangband's are a Half-Troll with one resistance swapped, and those are the ones
that did not make it.

.. list-table::
   :widths: 18 82
   :header-rows: 1

   * - Race
     - What it is for
   * - **Amberite**
     - Of Amber's own blood. Better than a human at nearly everything, heals
       unnaturally fast, and dear: 225 per cent experience. The game is named
       after these people.
   * - **Draconian**
     - Breathes. The second most expensive thing you can be.
   * - **Vampire**
     - Drinks blood to heal, from level 5 — the earliest power in the game.
   * - **Mindflayer**
     - Enormous intelligence and wisdom, a feeble body, and it attacks minds
       directly.
   * - **Golem**
     - Made, not born. Very strong, very stupid, and can turn its skin to
       stone.
   * - **Sprite**
     - Flies. Fragile, quick, and puts things to sleep.
   * - **Half-Titan**
     - The most expensive thing you can be, at 255 per cent.
   * - **Yeek**
     - Feeble, cowardly, and cheap — it levels as fast as a human, which is
       the only kindness it is shown.
   * - **Beastman**
     - Chaos-touched and born wrong, and the one race that mutates without
       being made to: one mutation at birth and a one-in-five chance at every
       level after.

**Experience cost is a real dial here.** Angband flattened it — nearly every
race costs 120 per cent and the field barely matters. Zangband ran it from 100
to 255 and used it to price what a race can do, and ZangbandTK keeps Zangband's
figures. So a Half-Titan is slow to level because it is a Half-Titan, and a Yeek
is not because it is a Yeek. Read the XP column in :doc:`birth` as part of the
race, not as a footnote.

Racial powers
-------------

New mechanism. Angband has no such thing: a race there is a set of adjustments
and flags, and everything a character *does* comes from a spell, an object or a
shape.

Eight of the nine new races can do something no class will ever teach — press
``N`` for the list. An Amberite shifts into shadow and, thirty levels later,
walks the Pattern. A Draconian breathes, a Golem turns to stone, a Yeek screams,
a Half-Titan sizes up what it is looking at.

Powers cost mana where there is any and **hit points where there is not**, which
is what keeps them worth having whatever class you chose. The levels, costs and
failure rates are Zangband's own numbers. See :ref:`Racial powers
<racial-powers>`.

The Monk, and martial arts
--------------------------

The first of Zangband's five classes, and the one that needed a mechanism
Angband does not have.

In Angband, fighting bare-handed is a punishment: one point of damage a blow,
criticals skipped outright. A Monk turns that on its head. It carries no weapon
because it does not need one, and unarmed it climbs a ladder of seventeen
techniques — a punch at level 1 up to a crushing blow at 48 — throwing two
strikes a turn at the start and **eight** by the end, which is more than a
Warrior gets out of any weapon in the game.

Which technique lands is decided fresh on every swing, so a Grand Master usually
— but never reliably — throws something from the top of the ladder. Some strikes
stun; a knee doubles a male opponent up; a kick to the ankle leaves anything
that walks limping.

And it is the one class in the game that is **punished for wearing armour**.
Over the weight limit and the strikes halve and every bonus withdraws; under it,
each empty armour slot is worth armour class in its own right. See
:ref:`Martial arts <martial-arts>`.

The Mindcrafter, and psionics
-----------------------------

The second of Zangband's classes, and the opposite kind of thing to the Monk.

A Mindcrafter has no spellbooks and will never find any. Its craft is twelve
powers that arrive as it levels — and they are deliberately **not a realm of
magic**: nothing to study, nothing to choose at birth, nothing that can be
taken away. That distinction is the whole requirement, not a detail of how it
was built.

Most of the twelve grow into something else as the character does, which is
worth knowing because it is unlike anything in Angband. *See what is coming*
spots monsters at level 2, finds traps and doors at 5, sees invisibility at 15,
maps the level at 20, grants telepathy from 25, detects everything at 30 and
lights the whole level at 45 — one power, one key, for a whole career.

**And psionic force is a new kind of damage.** Every damaging type Angband has
is an element, resisted with a flag or with armour. This one asks whether there
is a mind there to hurt: a golem, a mould or an animated weapon is completely
unaffected however hard it is hit, and a strange or feeble mind takes a third.
Against anything else, nothing resists it. See :ref:`Psionics <psionics>`.

The Chaos-Warrior, and who owns it
----------------------------------

The third of Zangband's classes, and the only one in the game that belongs to
somebody.

At birth a Chaos-Warrior is sworn to one of nine Lords of the Courts of Chaos.
It does not choose which, it cannot leave, and every time it gains a level its
Lord looks up and decides how it feels about the servant it has. Usually that is
a gift — a healing, a raised stat, experience, an object out of nothing. Sometimes
it is a cursed weapon, a ruined stat, monsters dropped on top of you, or the
level destroyed around you. The class fights nearly as well as a Warrior, and
the relationship *is* the class.

**It also casts.** A Chaos-Warrior learns Chaos magic from level 2 — all
thirty-two spells, and it is the only class in the game with exactly one realm
and no choice about it. Zangband is blunt on the point: they are "not interested
in any other form of magic". Until the Chaos realm existed there was nothing to
give them and the class shipped unable to hold spell points at all.

**And thirteen is an unlucky level.** The odds of the cruel end of the repertoire
are normally about one in six; on reaching level 13 they are one in two. Every
other thirteenth level is one in three, and every fourteenth is one in twelve —
the safest a Chaos-Warrior ever gets. It is nowhere in the interface and a player
can only learn it by living through it.

**The nine Lords are Zelazny's.** This is the clearest case in the game of the
drift this project exists to undo. Zangband's sixteen patrons were Moorcock's
Elric gods — Slortar, Arioch, Xiombarg, Mabelode — with Khorne, Slaanesh, Nurgle
and Tzeentch from Warhammer among them, and not one had any connection to Amber.
They are replaced by Swayvill the King of Chaos, Suhuy who keeps the Logrus,
Mandor, Dara, Gramble, Jurt, Despil, Borel and Gilva. Four of them are already
walking around in the bestiary, so you can be gifted by a Lord you later meet.

Still to come
-------------

Two of Zangband's classes are not in yet: the Warrior-Mage and the High-Mage,
both defined by which realms they may choose, so both waiting on :doc:`realms`.
Eight more races are waiting too, four of them undead, which want one mechanism
between them and should arrive together.
