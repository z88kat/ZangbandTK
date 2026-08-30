Mutations
=========

Exposure to chaos changes you, permanently and not always for the better.

There are ninety-six ways it can, and you do not choose between them. A
mutation arrives because something happened to you — a spell went wrong, a
Lord of Chaos took an interest, you were standing in the wrong place when
something breathed — and what you get is what the roll gives you.

.. note::

   **Under construction.** The model is built, the standing changes act, and
   the powers can be invoked. What is left is the mutations that fire on their
   own and the ones that are extra attacks.

The four kinds
--------------

**Continuous.** Simply true of you from the moment you have them. Superhuman
strength, a moronic mind, skin turned to steel, wings, a rotting body, good
luck, bad luck. Thirty-two of them, and twelve are purely bad.

These act on your character directly, the way a shapechange does rather than
the way a ring does — there is nothing to wear and nothing to take off. They
show up on your character sheet under **Mutations**, and in your stats,
armour, speed, saving throw and resistances without being labelled.

.. warning::

   **A mutation is rarely only what it is called.** Superhuman strength is
   +4 STR and also -1 INT and -1 WIS; being puny is -4 STR and *+2 DEX*; a
   moronic mind is -4 INT and -4 WIS and makes you immune to fear and
   confusion; iron skin is +25 AC and -3 DEX. The Zangband spoiler gives the
   headline of each and stops, and the headline is generally the good half.
   What this game implements is what Zangband's code did, which is harsher on
   the good mutations and kinder on the bad ones.

Two of the thirty-two do nothing at all. A silly voice and an illusory normal
appearance changed only your charisma, and there is no charisma in Angband
4.2 — the stat was removed in 4.2.0. You can still gain them, they are still
described on your sheet, and they have no effect. They were left in rather than
dropped because a mutation that vanishes from a savefile is worse than one that
does nothing.

**Activatable.** Powers you invoke, at a cost in mana — or in blood, if you
are not a caster. Spitting acid, breathing fire, eating rock, turning objects
to gold, teleporting at will. Thirty-two, each with its own level, price, and
a stat it is rolled against.

They appear in the same list as your racial and class powers, after them, and
work the same way — the same failure roll, the same fall back to hit points
when you are short of mana. Some grow with you: spitting acid throws a wider
ball from level 30, breathing fire from level 40, and hardening yourself
against the elements resists more of them the higher you go.

**Nine of the thirty-two cannot be used yet.** They are listed with *not yet*
beside them rather than hidden, because your character sheet describes them and
you would come looking. What they need is machinery Angband 4.2 does not have,
and in most cases inventing it to carry one mutation would be the wrong way
round:

.. list-table::
   :header-rows: 1
   :widths: 24 76

   * - Mutation
     - What it needs
   * - Telekinesis
     - Pulling a distant object to you. 4.2 can move *you* to a place; it
       cannot bring a thing to you.
   * - Swap position
     - Exchanging places with a monster. 4.2's nearest move puts you in a
       square that is still occupied.
   * - Sense curses
     - Something hidden to reveal. 4.2 has no pseudo-identification; curses are
       runes you learn by carrying the thing.
   * - Polymorph self
     - The Polymorph Self mechanic, which is coming with the acquisition paths.
       Deferred to there, not away.
   * - Midas touch
     - Turning an object into gold. There is no such thing in 4.2, and adding
       one is an economy change.
   * - Grow mold
     - Eight *friendly* molds. 4.2 has no pets, so the faithful translation
       would surround you with eight enemies.
   * - Weigh magic
     - Reporting how long your effects have left. 4.2 puts that on the status
       line already.
   * - Sterility
     - Stopping every breeder on the level. The counter exists; no effect
       reaches it.
   * - Launcher
     - A throwing multiplier. Throwing is a command in 4.2, not something a
       power can invoke.


**Random.** Things that happen to you without being asked for. Berserker fits,
a body that brews its own alcohol, attracting demons, occasionally becoming
invulnerable, occasionally dropping your weapon. Twenty-seven, most of them
rare — one turn in three thousand up to one in twelve thousand.

**Melee.** Five of them, and they are extra blows rather than anything else: a
scorpion tail, horns, a beak, an elephantine trunk, a nest of tentacles.

*Zangband files these in three groups rather than four, because thirty-two
flags is what fits in a machine word and it had three words to spend. The
grouping above is what the spoiler describes and what a player actually meets.*

How you get them
----------------

Every route is chaos of one sort or another — that is the point of them, and
the reason there is no generic "random mutation" source:

- **Being a Beastman.** One at character creation, and a one-in-five chance at
  every level after. This is what a Beastman *is*.
- **A Chaos patron's regard.** A Lord of the Courts may hand one down as a
  level-up reward.
- **The gift mutation.** One of the ninety-six makes the Chaos deities take an
  interest in you, which means more of the others.
- **Chaos itself.** Being hit by raw chaos without resisting it.
- **Polymorph Self**, whether cast or mutated into.
- **A spell going badly wrong**, if it was a chaos or death spell.

Not every route is open yet: the ones that go through the Chaos and Death
magic realms wait for the realms themselves.

Some races take more readily to particular changes. A Vampire's gaze turns
hypnotic six times out of ten; a Mindflayer sprouts tentacles as often; a Yeek
learns to shriek. A Beastman tends towards polymorphing itself, but only one
time in ten — the source is specific about that, where the documentation is
not.

Three mutations have conditions attached, and no Zangband document mentions
them. The Midas touch only comes to the already-rich: a thousand gold for every
level you have, in hand at the moment it would arrive. A silly voice and a
vulnerability to the elements only come to characters who are mutated already,
with three or more. That last is exactly what it sounds like — the more chaos
has hold of you, the worse the changes it offers.

How you get rid of them
-----------------------

Barely at all, which is what makes them matter:

- **A potion of New Life**, which is not easy to come by.
- **Gaining a mutation that cancels one you have.** Nine such pairs. Becoming
  superhumanly strong ends being puny; growing a beak replaces a trunk. Iron
  skin is the interesting one — it drives out scales, rotting flesh and warts
  all at once, while any one of those three drives out the iron skin. Scales
  and warts get along fine together.
- **The Chaos Tower**, a building service that exists for this and nothing
  else.
- **The "strangely normal" mutation**, which occasionally removes mutations —
  including, eventually, itself.
- **Polymorph Self**, which sometimes takes one away instead of adding one.

.. note::

   **Mutations do not slow your healing.** The Zangband spoiler warns that they
   do, and that was true of version 2.2.2d — but the mechanic had been taken
   out again by the version this game is built from, and it is not reinstated
   here. What mutations cost you instead is that you do not choose them:
   twelve of the continuous ones are simply bad, and being resilient or able to
   eat rock makes you hungrier. See DEC-45.

A note on the descriptions
--------------------------

Twelve mutations advertised a change to your charisma, and there is no charisma
in Angband 4.2 — the stat was removed in 4.2.0. Those descriptions have had the
charisma taken out of them and nothing else changed, so a squeaky voice now
reads as a squeaky voice and warts read as *+5 AC*, which is what they actually
are. Anything else the description claims is real.
