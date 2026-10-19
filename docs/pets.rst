Pets and Allies
===============

.. note::

   **Milestone M10 is complete**, all nine phases of it, along with the two pet
   sources from PLR-28 that the last phase first shipped without. Two of
   Zangband's routes remain, and both need an object that doesn't exist here
   yet; they're named at the end.

   Requirements: PLR-22 to PLR-33.

In Angband every monster is an enemy. That's less a rule of the game than an
assumption built into it: a search of Angband 4.2's source for the word
*friendly* finds two hits, one a comment and one a shopkeeper's greeting.
Nothing in the targeting, the monster AI, the projection code or the combat
resolution has anywhere to put the idea of a creature that is on your side.
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

The middle row is the easy one to miss, and it's why there are three states
instead of a *friendly* flag. A friendly monster will not attack you, and that's
all it promises: it takes no orders, follows nobody, and costs nothing. A pet
does what it's told and is paid for. A flag that could only be true or false
could express one of those two, and the other would become unreachable.

A pet and a friendly monster are on the same side as each other. They will not
fight, and neither will interfere with the other. The difference between them
is only in how each one stands with you.

Two of your own pets may still fight, though. Alignment is checked before sides
are: a creature flagged good and one flagged evil are enemies wherever they
stand, so a charmed angel and a summoned demon will go for each other in front
of you. Keeping a mixed stable has a cost.

A third case only sounds like a contradiction. A handful of creatures (the
balance drake, the Great Wyrm of Balance) are flagged *both* good and evil,
which makes them the enemy of everything aligned and at peace with everything
that is neither. That's what Balance means in this setting, and it comes from
Zangband's own data.


Telling them apart
------------------

Looking at a monster tells you whose side it's on before it says anything
about its wounds::

   pet, unhurt
   friendly, somewhat wounded
   almost dead

The side comes first because it decides whether you attack at all. Zangband
put it last, after the health and after the recall prompt, which is where a
player scanning a crowded floor stops reading.

The monster list counts them the same way it counts sleepers::

   4 kobolds (2 pets)
   a soldier (pet, asleep)

The list groups by race, so a single row can cover several creatures, and only
a count can be accurate about them.

The glyph itself is *not* recoloured. Zangband did not recolour it either (the
pet colour flags in its source belonged to its graphical Tk client, not to the
map), and there are already three rules competing for a monster's colour here:
multi-hued monsters, purple uniques, and shapechangers, which borrow the
appearance of something else entirely. A fourth rule would lose to all three on
exactly the monsters you most need to identify.


Saved games
-----------

A monster's side is written into the savefile, so pets survive a save, a load
and a walk between levels.

Characters saved before this was added still open. Their monsters are all
hostile, which is what they were.


How they fight
--------------

Angband's monster AI has exactly one goal in it. Every road through it leads to
the player: following the sound and scent that spread out from wherever you're
standing, or running away from the same place, or working with its pack to
surround you. None of that describes an ally, so allies think differently.

They pick a fight. An ally looks for the nearest thing it's at war with, within
about twenty squares, that it can reach, and goes for it. Once it has picked
something it stays with it until the fight is over or the target stops being
worth chasing. It will not drop a wounded enemy because something fresher walked
into the room.

They stay awake. Angband decides whether a monster is thinking by asking
whether it can see, hear or smell *you*. An ally on the far side of a level
does none of those, and would otherwise sleep through its own battle.

They cast at what they're fighting. A pet with spells throws them at its
enemy, rolling against that creature's armour instead of your saving throw.

They will not hit you, and they will not interrupt you. An ally that steps into the
square you're standing in is trying to get past you, not attacking you, so it
goes around instead, and waits if there's nowhere to go around to. Nor does
its shuffling about count as something moving in view: you can rest
with a pet standing over you, which you couldn't do if the game treated your
own animal as a warning. Only a creature that has turned hostile does either.

The noise it makes elsewhere doesn't interrupt you either. A pet bursting
through a door still announces itself (you hear a door burst open), but hearing
it doesn't stop you resting, and nor does a fight you can't make out. An ally
trading spells with something out of your sight, or beyond it, is left to get
on with it; a fight where you can see one of the two stops you, as it should.
Anything aimed at *you* always stops you, seen or unseen.

And when two enemies meet, they fight. Neither shoves past the other, and
neither tramples the other underfoot, so a large hostile monster can't walk
over your pet and delete it on the way to you. It has to kill it, one blow at a
time, where you can see it happening.

A pet fighting for you earns you nothing. Experience is for the killing blow,
and if your pet struck it, the blow wasn't yours. A pet cannot finish a unique
either: uniques survive anything that isn't the player at one hit point.

Pets also keep to a leash: how close they stay, whether they'll fight near you,
whether they open doors, whether they pick things up. Those are orders you
give, and the next section covers giving them. Until you do, every pet follows
the default, which is to stay within about six squares, leave doors alone and
leave your things where they fall.


Giving orders
-------------

Press ``P`` for the pet menu, or ``!`` in the roguelike keyset, where ``P``
peruses a book. Zangband used ``p``; here that key is auto-explore.

The orders are a policy for all your animals at once, and every pet you have
follows the same one. They keep until you change them, they survive a save,
and they apply to pets you haven't acquired yet.

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

.. note::

   Ten squares is a hard limit on every setting above. A pet more than ten
   squares from you abandons whatever it's doing and comes back, whichever
   setting it's on, "Seek and destroy" included. So that setting doesn't mean
   "go anywhere": it means "go anywhere within ten squares". A pet that seems
   to give up on a fight across the room hasn't lost its nerve; it has hit the
   leash.

The last two are the ones to reach for before you cast something with a radius.
A pet that's keeping its distance also refuses to start a fight near you. What
a pet drags into the room is more dangerous than the pet.

Two more are switches:

**Pets may open doors** is off by default. A pet that opens doors is a pet that
lets things out.

**Pets may pick up items** is off by default too. It governs picking up, not
destroying: a pet that eats your drops is a monster being a monster, and no
order will stop it.

And two do something:

**What follows you** lists your pets with their levels, and the total. That
total is what your mana upkeep is charged on.

**Dismiss pets** asks about all of them, then one at a time. A dismissed pet is
gone. It isn't released, because turning a stable loose as enemies would make
dismissing them worse than keeping them.


Between levels
--------------

Your pets follow you down a staircase, up to four of them, wherever they were
standing. A pet across the level is still yours.

Zangband did not do this. It deleted every pet at every level change, which
was its region model at work and nobody's design: there's no pet-carrying code
anywhere in its source, and its documentation never raises the subject. Pets
following you is one of the things people remember liking about Zangband, so
this game does what people remember instead of what the code did.

They arrive around you, within about five squares, with the ones that were
following closest getting the closest ground, and you're told they came::

   Your 3 pets follow you down.

Nothing is said at all when you have no pets, so the line means something when
it appears.


Four, and why
~~~~~~~~~~~~~

The limit of four is about the screen, not the balance. A dozen animals
arriving around you is a mess to look at and worse to move through. If you have
more than four, the nearest four come and the rest stay where they were.

The mana upkeep has usually settled the question well before the fourth one
anyway. In `What they cost`_, the fourth young red dragon costs a level-30 Mage
95 per cent of its regeneration. Four is the limit you notice with a stable of
cheap animals; the upkeep is the limit you notice with a stable worth having.


One in twenty walks away
~~~~~~~~~~~~~~~~~~~~~~~~

Each pet has a 5% chance, at each level change, of leaving you. It's a flat
chance. It doesn't depend on the creature, on how hurt it is, on how frightened
it is, or on anything you did. Sometimes a pet just goes. Cats do that::

   The cave spider does a runner, looking for a new owner.

A pet that leaves is gone from the game. It isn't standing on the level you
left, it hasn't gone wild, and it hasn't turned against you. There's nothing to
go back for, and persistent levels make no difference. That's what the message
is telling you, and it's why it's worded the way it is.

One in twenty sounds small, but it compounds:

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - After this many level changes
     - a given pet is still with you
   * - 1
     - 95%
   * - 5
     - 77%
   * - 10
     - 60%
   * - 20
     - 36%
   * - 50
     - 8%

A pet lasts about thirteen or fourteen level changes before the odds turn
against it, and twenty on average. With a full stable of four, roughly one
descent in five costs you one of them, and after twenty levels you should
expect one or two of your original four.

That's attrition, and it's meant to be. Compare it with what it replaced: four
pets over twenty levels losing two or three, against four pets over *one* level
losing all four.


Which ones you lost, and whether you can get them back
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Every pet that does not come with you is named, one line each, however many
there are. Arriving with two when you left with five and not knowing which
three you lost is worse than losing them.

Each line tells you something different::

   The cave spider does a runner, looking for a new owner.
   The soldier stays behind; you cannot lead more than 4.
   There is no room here for the giant frog.
   The creeping coins cannot follow you.

.. list-table::
   :header-rows: 1
   :widths: 34 22 44

   * - What happened
     - Still yours?
     -
   * - It ran off
     - **no**
     - gone from the game; nothing to go back for
   * - Over the limit of four
     - yes
     - standing where you left it
   * - No room to arrive
     - yes
     - standing where you left it
   * - A mimic
     - yes
     - its disguise belongs to the old level

Three of those four are recoverable: the pet is on the level you came from,
still yours, and if that level persists it's still yours when you go back up.
Only the first is permanent.

Now and then there's no room at all (a staircase in a dead end), and then the
whole stable is left behind. It's rare, it isn't permanent, and every one of
them is still named.

Nothing follows you into an arena, or out of one.


What they cost
--------------

Keeping control of a charmed creature takes concentration, and past a point it
takes more than you have to spare.

A few are free. You maintain ``1 + level/20`` of them for nothing: two at level
20, three at 40. The divisor is a class property, ``pet-upkeep-div`` in
``lib/gamedata/class.txt``. A Mage divides by 15 and a High-Mage by 12, so at
level 48 they keep four and five where a Warrior keeps three.

Past that, the whole stable is charged, *all* of them, the free ones included.
The bill is the sum of your pets' levels, as a percentage of your mana
regeneration, and it's never less than 5% or more than 95%.

That's a cliff, and it's meant to be, and for high-level pets it's a very steep
one. Here's a level 30 Mage, who has 120 spell points, regains 36 per hundred
turns with no pets, and keeps three free (``1 + 30/15``):

.. list-table::
   :header-rows: 1
   :widths: 34 14 14 19 19

   * - The stable
     - Sum of levels
     - Upkeep
     - sp per 100 turns
     - against none
   * - 3 soldiers (level 2)
     - 6
     - 0%
     - 36
     - 100%
   * - 4 soldiers
     - 8
     - 8%
     - 33
     - 92%
   * - 10 soldiers
     - 20
     - 20%
     - 29
     - 80%
   * - 3 young red dragons (level 31)
     - 93
     - 0%
     - 36
     - 100%
   * - **4 young red dragons**
     - 124
     - **95%**
     - **1**
     - **2%**

Read the last row before you charm a fourth dragon. Three are free; the fourth
takes you from thirty-six spell points per hundred turns to one. A caster with
an army of deep pets cannot cast, and it's that, not the limit of four, that
decides the size of a useful stable.

The cliff is at the free allowance, not at any particular weight of pet: three
level-31 dragons cost nothing at all, and the fourth costs almost everything.
Your own allowance depends on your class and your level, so work it out before
you go looking.

Shallow pets are cheap: a dozen level-2 soldiers come to 24%. They're also a
dozen things that die to the first breath weapon.

The pet menu tells you the figure before you go looking for another.

Zangband's own documentation has a warning that still applies: *keep this in
mind if you have a pet which can summon or otherwise produce more pets.*

If you're a Blackguard, none of this touches you. Your mana burns instead of
regenerating, and a stable of pets does not slow the burning. Zangband had no
class that loses mana by design, so it never had to decide this; scaling the
burn the same way would have paid you for keeping pets.


What they earn you
------------------

Nothing. Experience is for the killing blow, and if your pet struck it, it
wasn't yours.

Nor can a pet finish a unique. Anything that is not the player leaves a unique
alive on its last hit point, however hard it's hit, so you cannot send a pet
into a vault to clear it for you.

Between them, these are why a stable is a tactical asset and not a way to win
the game while standing still.


Losing one
----------

Pets are easily irritable. Anything you do that hurts one turns it against you:
a blow, an arrow, a bolt that passes through it, a fireball that catches it at
the edge. It stops being yours on the spot, and it remembers.

That's the thing to think about before lighting up a room while you have pet
orcs, and it's why area spells are a different proposition with a stable
behind you. Damage from *other* monsters does not count against you: a pet
caught in something else's breath is unhurt in its opinion of you.

Carrying something that aggravates turns them all. Nobody wants to be your
friend if you're aggravating.

Turning on a creature that trusted you costs you virtue: a gain in
Individualism and a loss in Honour, Justice and Compassion. Those are real
numbers here, and other things read them.

Making one costs virtue too, in the opposite direction. Every successful charm
takes a point off your Individualism, since taking a creature into your service
is a step away from standing alone, and if what you charmed was an animal it
adds a point to Nature. A character who builds a menagerie of beasts will find
both scales have moved a long way without a single message saying so. See
:doc:`virtues`.

Walking into your own pet does *not* hurt it. You change places with it
("you push past it"), and that's what stops the whole mechanism firing every
time an animal gets into a doorway ahead of you.

Unless you aren't in command of yourself. If you're confused, hallucinating,
stunned, berserk, or unable to see what's in front of you, the blow lands and
the pet takes it personally. Confusion is a different kind of dangerous when
you have things following you.

Changing a pet's shape doesn't change its side. Polymorph one with Chaos's
*Polymorph Other*, or a Wand or a Rod of Polymorph, and whatever it turns into
is still yours.

Polymorphing your own animal deep in a dungeon is how a weak pet becomes a
strong one, because the new shape is drawn from somewhere between the
creature's own depth and the depth you're standing on. It's where the dragons
come from. It's a gamble every time: you don't get to choose what you get, it
can come back worse, and a unique won't change at all. What holds it in check
is the upkeep. A stronger pet is a heavier one, and the charge is on the *sum*
of your pets' levels.

It never takes the creature away. The shape drawn is always one that can stand
where your pet is standing, so a polymorph on the shore won't turn your hound
into a kraken and leave you with an empty patch of sand.

Chaos itself isn't so obliging. A chaos attack that polymorphs also *hurts*,
and hurting a pet turns it against you before its shape has changed, so a chaos
ball dropped on your own animal costs you the animal, whatever it becomes.


Getting one
-----------

You can charm something that's already there. Three spells do it, and they're
in three realms because they're three different things:

- *Day of the Dove* (Life) turns everything in sight, if it will be turned.
- *Animal Taming* (Nature) takes one animal; *Animal Friendship* takes every
  animal in sight.
- *Enslave Undead* (Death) binds one undead creature to you.

A Wand of Tame Monster does the same thing from an item, at a fixed strength
that does not depend on your level, which is what makes it worth carrying deep.
A hypnotic gaze, if chaos has given you one, is a charm as well.

Three kinds of creature will never be charmed: uniques, quest monsters, and
anything whose mind cannot be confused. A mind that resists persuasion can still
be tamed if it's an animal, though, or commanded if it's dead. Everything else
rolls its level against the strength of the attempt, so a strong charm on a
shallow creature is nearly certain and a weak one on something deep is nearly
hopeless.

And if you're carrying something that aggravates, nothing will have you. The
charm lands, the creature considers it, and hates you too much.

Or summon something that arrives on your side. *Summon Animal* (Nature) calls
one. Chaos's *Summon Demon* calls one demon in three that will serve you and
two that won't. If you can grow mould, the eight that come up around you are
yours, and that's what a wall of mould is for.

Anything your pets summon is yours too, and so is anything a friendly monster
summons. Read that twice, next to the upkeep: a pet that can summon builds you a
stable you're paying for.

Or take up Trump. The seventh realm is a realm of pets: twelve of its spells
call something that serves you, from an animal at level 24 to one of the great
undead at 49. The Mage, Priest, Rogue, Ranger, Warrior-Mage and High-Mage may
all study it.

Or dominate a mind. A Mindcrafter's *dominate a mind* arrives at level 9, and
what it does changes at 30. Below that it's a mind attack that confuses what it
hits, and from 30 it takes every hostile mind in sight. The split is
Zangband's: below 30 the power cannot hold anything in thrall at all, and above
it the domination is wholesale.

Or be given one. All nine Lords of Chaos will hand a Chaos-Warrior something
living, somewhere in the generous half of their moods: a servant, a demonic
servant, or an undead one, depending on the Lord. It arrives beside you and
it's yours.

Or throw one. A **Magical Figurine** is a monster in miniature, and its name
says which: *a Magical Figurine of a cave spider*. Throw it and it breaks, and
the monster is there where it came down (beside whatever you threw it at, or
where it fell), on your side. Figurines are found from dungeon level 26. Each
holds an ordinary monster, never a unique, and usually one no deeper than where
it was found, though now and then something a good deal worse. A figurine is
worth that monster's level squared, times five. The **Figurine Store**, in a
magical city, sells them and buys them, and so does the black market
(:doc:`towns`).

The monster has to have somewhere to stand, within a few paces of where the
figurine lands. A fish needs water, and nothing comes down on lava or deep
water unless it can bear it. With nowhere to stand, or in single combat, where
nothing may be summoned at all, *the figurine writhes and then shatters*, and
is gone. Figurines do not stack, even two of the same monster.

That's every one of Zangband's routes to a pet. Its wand of charm monster is
the Wand of Tame Monster above, which is Zangband's own name for it.
