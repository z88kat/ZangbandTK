Mutations
=========

Exposure to chaos changes you, permanently and not always for the better.

There are ninety-six ways it can, and you do not choose between them. A
mutation arrives because something happened to you — a spell went wrong, a
Lord of Chaos took an interest, you were standing in the wrong place when
something breathed — and what you get is what the roll gives you.


The four kinds
--------------

**Continuous.** Simply true of you from the moment you have them. Superhuman
strength, a moronic mind, skin turned to steel, wings, a rotting body, good
luck, bad luck. Thirty-two of them, and twelve are purely bad.

These act on your character directly, the way a shapechange does rather than
the way a ring does — there is nothing to wear and nothing to take off. They
show up on your character sheet under **Mutations** — press ``C``, then ``h``
twice to reach that page — and in your stats, armour, speed, saving throw and
resistances without being labelled.

.. warning::

   **A mutation is rarely only what it is called.** Superhuman strength is
   +4 STR and also -1 INT and -1 WIS; being puny is -4 STR and *+2 DEX*; a
   moronic mind is -4 INT and -4 WIS and makes you immune to fear and
   confusion; iron skin is +25 AC and -3 DEX. The Zangband spoiler gives the
   headline of each and stops, and the headline is generally the good half.
   What this game implements is what Zangband's code did, which is harsher on
   the good mutations and kinder on the bad ones.

   **Your character sheet tells you all of it.** Everything in the bracket
   after a mutation's description is generated from what the mutation actually
   does, so the two cannot come apart: every stat it moves, every resistance
   it grants, every vulnerability it opens, and everything it takes away. It
   was not always so — a living computer brain used to advertise its four
   points of intelligence and wisdom and say nothing about the vulnerability
   to electricity that comes with it.

Three mutations take something away rather than adding anything. Rotting flesh
stops you regenerating — including the regeneration from a ring you are wearing
— and the panic-hit power and the warning mutation both stop you resisting fear,
however you came by the resistance. Three more make ordinary food barely feed
you: a beak, a mouth that eats rock, and a taste for blood all leave you a
twentieth of what you swallow. Potions still work normally.

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

**Seven of the thirty-two cannot be used yet.** They are listed with *not yet*
beside them rather than hidden, because your character sheet describes them and
you would come looking.

The Midas touch used to be an eighth, marked *dropped*: turning objects into
gold needed a mechanic the game was not going to build for one mutation. It
turned out not to be for one mutation — Sorcery's *Alchemy* spell and a
random-artifact power want the same thing — so it was built once for all three,
and the Midas touch works (DEC-52). What they need is machinery Angband 4.2 does not have,
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

Each is rolled separately, every turn, so three of these are three chances and
not one. Almost all of them stop while you are under an anti-magic effect —
cowardice is the exception, because being too frightened to act is not magic,
and Zangband is specific about that. Six of the twenty-seven do not fire yet:
turning briefly incorporeal, being warned about what is nearby, trading hit
points for spell points and back, losing a mutation at random, and the chaos
gift, which needs the patron it asks. All six still show on your character
sheet.

**Melee.** Five of them, and they are extra blows rather than anything else: a
scorpion tail, horns, a beak, an elephantine trunk, a nest of tentacles.

They land once each per melee round, after your weapon, and cost you nothing —
a mutated Warrior and a mutated Mage bite equally often. They are the reason a
low-level character with the right mutations hits harder than their class says
they should.

.. warning::

   **The melee dice are not what Zangband's own text says.** Every one of the
   five states its damage the wrong way round: a scorpion tail is written
   "3d7" and rolls **7d3**, an elephantine trunk is written "1d4" and rolls a
   flat **4d1**. The code fills in two variables and then passes them in the
   other order, and the result is that all five hit harder than advertised.
   This game uses what the code rolls and the descriptions have been corrected
   to match.

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

Everything on that list works except the two that go through a magic realm —
Polymorph Self cast as a Chaos spell, and a Chaos or Death spell going wrong.
Those wait for the realms themselves. The Polymorph Self *mutation* works now,
and so does everything else: a Lord of the Courts replaces one favour in six
with a mutation instead, the chaos gift makes a Lord take an interest in a
character who never swore to one, and unresisted chaos changes you one time in
three.

.. note::

   **Polymorph Self does less here than in Zangband, on purpose.** Zangband's
   version could change your sex and your race outright, permanently. This
   game's shapechanges you instead — the same decision the Chaos patron's
   "Thou needst a new form, mortal!" already got, for the same reason. What is
   left of it is the mutation reroll: it sheds some of what you have and gives
   you more, and how much of each depends on your level. At level 40 it is
   frightening. At level 5 it usually does nothing at all.

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

- **A potion of New Life**, which strips away every mutation at once and is
  the deepest, most expensive potion in the game. Zangband's also re-rolled
  your hit points; this one does not, because Angband 4.2 fixes those at birth.
- **Gaining a mutation that cancels one you have.** Nine such pairs. Becoming
  superhumanly strong ends being puny; growing a beak replaces a trunk. Iron
  skin is the interesting one — it drives out scales, rotting flesh and warts
  all at once, while any one of those three drives out the iron skin. Scales
  and warts get along fine together.
- **The Chaos Tower**, a building service that exists for this and nothing
  else. Only great cities have one, and it costs 2500 gold a mutation — but it
  is the only route that lets you *choose* which one goes. Everything else
  takes whichever it likes, or takes all of them.
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

Each mutation's text is Zangband's sentence with a bracket this game writes
itself, listing every effect the mutation carries. That is deliberate: the
brackets Zangband shipped were unreliable in three separate ways. Twelve of
them named a change to your charisma, and there is no charisma in Angband 4.2 —
the stat was removed in 4.2.0. All five melee mutations stated their damage
dice the wrong way round. And six named some of their effects and not others,
the worst hiding a vulnerability behind a pair of stat bonuses.

Generating the bracket instead of curating it is what stops that recurring. The
cost is the occasional redundancy — being completely fearless reads *(immune to
fear)*, which you had probably guessed — and that is a fair price for never
again being told half of what a mutation does.

If you are carrying a great many
--------------------------------

You can hold eighty-nine mutations at once — all ninety-six less the seven that
cancelling pairs make unreachable together. The power list and the Chaos Tower
show all of them. Only the first fifty-one get a letter to press; past that,
move the cursor to the one you want.
