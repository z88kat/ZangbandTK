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


Not yet written
---------------

Giving the orders, what pets cost you in mana, what makes one in the first
place, and what makes one stop being one. Those arrive with the rest of M10 and
this chapter grows a section each time.
