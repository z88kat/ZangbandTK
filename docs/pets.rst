Pets and Allies
===============

.. note::

   **Being written.** This chapter is filled in by milestone **M10**, one
   section per phase, and it is not finished yet. What is here describes what
   the game does today; what is missing is named at the end.

   Requirements: PLR-22 to PLR-33.

In Angband every monster is an enemy. That is not a rule of the game so much as
an assumption in it — a search of Angband 4.2's source for the word *friendly*
finds two hits, one a comment and one a shopkeeper's greeting. Nothing in the
targeting, the monster AI, the projection code or the combat resolution has
anywhere to put the idea of a creature that is on your side.

ZangbandTK has three sides.


Whose side a monster is on
--------------------------

.. list-table::
   :header-rows: 1
   :widths: 14 16 16 16 38

   * - State
     - Attacks you
     - Takes orders
     - Mana upkeep
     - Where it comes from
   * - **Hostile**
     - yes
     - —
     - —
     - everything, unless something changed it
   * - **Friendly**
     - no
     - no
     - none
     - found that way; summoned by another friendly monster
   * - **Pet**
     - no
     - yes
     - **yes**
     - charmed, summoned, dominated, a figurine, a patron's gift

The middle row is the one that is easy to miss, and it is why this is three
states rather than a *friendly* flag. A friendly monster will not attack you
and that is the whole of what it promises: it takes no orders, follows nobody,
and costs nothing. A pet does what it is told and is paid for. Something that
was only ever true or false could express one of those two, and the other would
quietly become unreachable.

Two consequences follow immediately, and both are deliberate:

**A pet and a friendly monster are on the same side as each other.** They will
not fight, and neither will interfere with the other. The distinction is about
your relationship with them, not theirs with each other.

**Two of your own pets may still fight.** Alignment is checked before sides
are: a creature flagged good and one flagged evil are enemies wherever they
stand, so a charmed angel and a summoned demon will go for each other in front
of you. Keeping a mixed stable is a decision with a cost.

There is a third case that only sounds like a contradiction. A handful of
creatures — the balance drake, the Great Wyrm of Balance — are flagged **both**
good and evil, which makes them the enemy of everything aligned and at peace
with everything that is neither. That is what Balance means in this setting,
and it is Zangband's own data rather than an accident of ours.


Telling them apart
------------------

Looking at a monster says whose side it is on, before it says anything about
its wounds::

   pet, unhurt
   friendly, somewhat wounded
   almost dead

The word comes first on purpose. Zangband put it last, after the health and
after the recall prompt, which is where a player scanning a crowded floor stops
reading. The side is the thing that decides whether you attack at all.

The monster list counts them the same way it counts sleepers::

   4 kobolds (2 pets)
   a soldier (pet, asleep)

The list groups by race, so a single row can cover several creatures and only a
count can be honest about them.

The glyph itself is **not** recoloured. Zangband did not recolour it either —
the pet colour flags in its source belonged to its graphical Tk client, not to
the map — and there are already three rules competing for a monster's colour
here (multi-hued monsters, purple uniques, and shapechangers, which borrow the
appearance of something else entirely). A fourth rule would lose to all three
on exactly the monsters most worth identifying.


Saved games
-----------

A monster's side is written into the savefile, so pets survive a save, a load
and a walk between levels.

Characters saved before this was added still open. Their monsters are all
hostile, which is what they were.


How they fight
--------------

A monster on your side is not simply a hostile monster with the attacking
switched off. Angband's monster AI has exactly one goal in it — every road
through it leads to the player, following the sound and scent that spread out
from wherever you are standing, or running away from the same place, or working
with its pack to surround you. None of that describes an ally, so allies think
differently.

**They pick a fight.** An ally looks for the nearest thing it is at war with,
within about twenty squares, that it can actually reach, and goes for it. Once
it has picked something it stays with it until the fight is over or the target
stops being worth chasing — it will not drop a wounded enemy because something
fresher walked into the room.

**They stay awake.** Angband decides whether a monster is thinking by asking
whether it can see, hear or smell *you*. An ally on the far side of a level
satisfies none of those and would otherwise sleep through its own battle.

**They cast at what they are fighting.** A pet with spells throws them at its
enemy, rolling against that creature's armour rather than against your saving
throw.

**And when two enemies meet, they fight.** Neither shoves past the other, and
neither tramples the other underfoot — a large hostile monster cannot simply
walk over your pet and delete it on the way to you. It has to kill it, one blow
at a time, where you can see it happening.

A pet fighting for you earns you nothing. Experience is for the killing blow,
and if your pet struck it, the blow was not yours. A pet cannot finish a unique
either: uniques survive anything that is not the player at one hit point.

Pets also keep to a leash — how close they stay, whether they will fight near
you, whether they open doors, whether they pick things up. Those are orders you
give, and the next section covers giving them; until then every pet follows the
default, which is to stay within about six squares, leave doors alone and leave
your things where they fall.


Giving orders
-------------

Press ``A`` for the pet menu. Zangband used ``p``; here that key is
auto-explore.

The orders are a policy, not instructions to an individual — you are setting how
your animals behave, and every pet you have follows the same one. They keep
until you change them, they survive a save, and they apply to pets you have not
acquired yet.

Five of them are a leash:

.. list-table::
   :header-rows: 1
   :widths: 22 78

   * - Order
     - What it means
   * - **Stay close**
     - Never more than a square away. They will not chase anything.
   * - **Follow me**
     - Within about six squares. The default, and the useful one.
   * - **Seek and destroy**
     - Go anywhere. They will cross the level for a fight and you will lose
       track of them.
   * - **Give me space**
     - Keep ten squares off, and take no fight closer to you than that.
   * - **Stay away**
     - Twenty-five squares. For when the thing following you breathes fire.

The last two are the ones to reach for before you cast something with a radius.
A pet that is keeping its distance also refuses to start a fight near you, which
is the point: the danger is not the pet, it is what the pet drags into the room.

Two more are switches:

**Pets may open doors** — off by default. A pet that opens doors is a pet that
lets things out.

**Pets may pick up items** — off by default. This governs picking up, not
destroying: a pet that eats your drops is a monster being a monster and no order
will stop it.

And two do something:

**What follows you** lists your pets with their levels, and the total. That
total is the number your mana upkeep will be charged on.

**Dismiss pets** asks about all of them, then one at a time. A dismissed pet is
gone, not released — turning a stable loose as enemies would make dismissing
them worse than keeping them.


Between levels
--------------

**Your pets follow you down a staircase.** All of them, wherever they were
standing — a pet across the level is still yours.

They arrive around you, within about five squares, the ones that were following
closest getting the closest ground, and you are told they came::

   Your 3 pets follow you down.

If there is no room for one of them it stays behind, and you are told which::

   The soldier cannot follow you.

One line per animal, by name — arriving with four when you left with six and
not knowing which two you lost is worse than losing them. Nothing is said at
all when you have no pets, so the line means something when it appears.

There is no room at all now and then — a staircase in a dead end — and then the
whole stable is left. It is rare, and it is still named.

Zangband did not do this. It deleted every pet at a level change, and that was
its region model deciding rather than a design: there is no pet-carrying code
anywhere in its source and its documentation never raises the subject. Pets
following you is one of the things people remember liking about Zangband, so
this game does the thing people remember rather than the thing the code did.

Nothing follows you into an arena, or out of one.


What they cost
--------------

Keeping control of a charmed creature takes concentration, and past a point it
takes more than you have to spare.

**A few are free.** You maintain ``1 + level/20`` of them for nothing — two at
level 20, three at 48. A Mage divides by 15 instead and a High-Mage by 12, so
they keep four and five where a Warrior keeps three.

**Past that, the whole stable is charged.** Not the extra one: *all* of them.
The bill is the sum of your pets' levels, as a percentage of your mana
regeneration, and it is never less than 5% or more than 95%.

That is a cliff and it is meant to be. Two pets inside your allowance cost you
nothing at all; a third takes a slice of your regeneration proportional to all
three. The pet menu tells you the figure before you go looking for a fourth.

The warning in Zangband's own documentation is worth repeating: *keep this in
mind if you have a pet which can summon or otherwise produce more pets.*

If you are a Blackguard, none of this touches you — your mana burns rather than
regenerates, and a stable of pets does not slow the burning. Zangband had no
class that loses mana by design, so it never had to decide this; scaling the
burn the same way would have paid you for keeping pets.


What they earn you
------------------

**Nothing.** Experience is for the killing blow, and if your pet struck it, it
was not yours.

Nor can a pet finish a unique. Anything that is not the player leaves a unique
alive on its last hit point, however hard it is hit — so a pet cannot be sent
into a vault to clear it for you.

Between them these are why a stable is a tactical asset and not a way to win the
game while standing still.


Losing one
----------

Pets are easily irritable. **Anything you do that hurts one turns it against
you** — a blow, an arrow, a bolt that passes through it, a fireball that catches
it at the edge. It stops being yours on the spot, and it remembers.

That is the thing to think about before lighting up a room while you have pet
orcs, and it is the reason area spells are a different proposition with a
stable behind you. Damage from *other* monsters does not count against you: a
pet caught in something else's breath is unhurt in its opinion of you.

**Carrying something that aggravates turns them all.** Nobody wants to be your
friend if you are aggravating.

Turning on a creature that trusted you costs you virtue: a gain in
Individualism and a loss in Honour, Justice and Compassion. Those are real
numbers here and other things read them.

Walking into your own pet does **not** hurt it. You change places with it —
"you push past it" — which is what stops the whole mechanism firing every time
an animal gets into a doorway ahead of you.

Unless you are not in command of yourself. Confused, hallucinating, stunned,
berserk, or unable to see what is in front of you, the blow lands and the pet
takes it personally. Confusion is a different kind of dangerous when you have
things following you.


Getting one
-----------

**Charm something that is already there.** Three spells do it, and they are in
three realms because they are three different things:

- *Day of the Dove* (Life) turns everything in sight, if it will be turned.
- *Animal Taming* (Nature) takes one animal; *Animal Friendship* takes every
  animal in sight.
- *Enslave Undead* (Death) binds one undead creature to you.

A Wand of Tame Monster does the same thing from an item, at a fixed strength
that does not depend on your level — which is what makes it worth carrying deep.
A hypnotic gaze, if chaos has given you one, is a charm as well.

Three creatures will never be charmed: uniques, quest monsters, and anything
whose mind cannot be confused — though a mind that resists persuasion can still
be tamed if it is an animal, or commanded if it is dead. Everything else rolls
its level against the strength of the attempt, so a strong charm on a shallow
creature is nearly certain and a weak one on something deep is nearly hopeless.

And if you are carrying something that aggravates, nothing will have you. The
charm lands, the creature considers it, and hates you too much.

**Or summon something that arrives on your side.** *Summon Animal* (Nature)
calls one. Chaos's *Summon Demon* calls one demon in three that will serve you
and two that will not. If you can grow mould, the eight that come up around you
are yours — which is the point of a wall of mould.

**And anything your pets summon is yours too.** So is anything a friendly
monster summons. That is worth reading twice next to the upkeep: a pet that can
summon builds you a stable you are paying for.


**Or take up Trump.** The seventh realm is a realm of pets: twelve of its
spells call something that serves you, from an animal at level 24 to one of the
great undead at 49. The Mage, Priest, Rogue, Ranger, Warrior-Mage and High-Mage
may all study it.


Still to come
-------------

Two of the ways Zangband gives you a pet are not here yet: a Mindcrafter's
domination, and a Chaos patron handing you one as a gift.
