Races and Classes
=================

Angband gives you eleven races and nine classes. ZangbandTK has twenty and ten,
and the new ones are not variations on the old — they are the reason the game
feels different from the first screen.

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
     - Chaos-touched and born wrong. In Zangband it also mutated as it
       levelled; that waits for the mutation system.

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

Still to come
-------------

Four of Zangband's classes are not in yet. The Chaos-Warrior wants its sixteen
patrons, the Mindcrafter its psionics, and the Warrior-Mage and High-Mage are
defined by which realms they may choose — so those two wait on
:doc:`realms`. Eight more races are waiting too, four of them undead, which want
one mechanism between them and should arrive together.
