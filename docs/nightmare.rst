Nightmare Mode
==============

.. note::

   **Being written.** This chapter is filled in by milestone **M11** as each
   piece lands, and says at the bottom what is not built yet. Per DEC-17 the
   milestone is not complete until the chapter is.

   Requirements: BAL-15 to BAL-18.

An irreversible choice made at birth, designed to be unfair. Zangband's own
description of the option is *"this isn't even remotely fair!"*, and its
documentation says the mode is *"not intended to be winnable"*. This chapter
says exactly what it changes, so that a player choosing it knows what they are
accepting — and exactly what it does not, because the mode has a spoiler that
describes about three times as much as Zangband ever built.

Choosing it
-----------

**Nightmare mode** is a birth option, off by default. Like every birth option it
can only be set while creating a character, and cannot be turned off afterwards
— that is what makes it irreversible, and it is why it appears in the birth
menu rather than anywhere else.

A character playing under it says so in its dump, as ``Nightmare Mode: ON``.
That line exists because the mode changes how the game is played and nothing
else in a dump would show it: a character that died at depth 20 under nightmare
did something quite different from one that died at depth 20 without it.

What it changes
---------------

Monsters, so far. Each of these is what Zangband does, with the source it comes
from.

**Twice the hit points.** Everything you meet has double what it would
otherwise have. This composes with the game's own lethality scaling rather than
replacing it, so a monster's hit points are ``base × 0.73 × 2``.

**Five points of speed.** A permanent ``+5`` on every monster in the game, on
top of whatever haste or slow is acting on it. At normal speed that is the
difference between trading blows and being hit twice for every one you land.

**Nothing is asleep when it arrives.** Monsters that would normally be found
sleeping are awake from the moment the level is made. They can still be put to
sleep afterwards — the mode does not stop that, whatever its spoiler says.

**Nothing waits for you.** Ordinarily the game marks certain dangerous monsters
so that they give you one free move when they first appear — the courtesy that
stops a summoned horror acting before you have seen it. Under nightmare there is
no such courtesy.

**Everything arrives half-started.** A monster placed on a level begins with a
random amount of energy towards its next turn, and under nightmare it begins
with twice as much. In practice it acts sooner after it appears.

**Sleepers are twice as alert.** A sleeping monster's chance of noticing you on
any given turn is doubled. Stealth still works; it works half as well.

**Sustains fail, and drains stick.** A sustain holds twelve times in thirteen
instead of always, and when a stat drain does land it is **permanent** twelve
times in thirteen rather than wearing off. This is probably the cruellest thing
in the mode and nothing in Zangband's own documentation mentions it: a sustain
is a property you buy once and stop thinking about, and one that fails
occasionally is a different item from one that holds.

**No stairs can be made.** Stair creation does nothing — no scumming for a
convenient descent, no escape hatch when a level turns out badly. You leave a
floor by the stairs that are on it.

**What lives on a floor is not what should live there.** Two changes at once.
The game's ordinary rule for putting something out of its depth adds a few
levels at most; under nightmare it multiplies, uncapped, so a monster on the
fifth floor can be one you would expect eighty floors down. And the monsters
normally pinned to their own depth — the ones the game will never generate
early, whatever else it does — are no longer pinned. Only quest monsters still
are.

**Some walls look like floor.** A few on every level, and one door in several
hundred. They are walls: you cannot walk through them, you cannot see past
them, and nothing about how they are drawn will tell you. You find them by
walking into them.

**A bell tolls before midnight, and then something answers it.** Four times in
the last hour — at eleven, and at each quarter past — and on the stroke of
midnight the Ancient and Foul Curse falls on you wherever you are. The bell is
the only warning the mode gives you about anything, and it is worth heeding:
there is time to drink something, or to be somewhere else.

**Word of Recall occasionally takes you somewhere worse.** About one recall in
six hundred and sixty-six arrives deeper than you asked — twice as deep in the
upper dungeon, half the remaining way down in the lower. It will not take you
past the bottom of the dungeon you are recalling into, so in a shallow one this
does little; in a deep one it can end a character who was going shopping.

Everything Zangband's nightmare mode actually does is now here.

And four things that will not be built. Two are permanent decisions:

- **Your score is unaffected.** Zangband gives nightmare characters a score
  bonus. This game's scoring has no multiplier of any kind to attach one to, for
  any birth option, so adding one would be inventing a system rather than
  porting a number.
- **A Golem keeps its immunity to stunning.** Zangband takes it away, but not
  for nightmare alone — two other difficulty options do the same, and this game
  has neither of them.

And two are about the mode's two nightmares, which Zangband triggers at the inn
and when a sleep attack lands on you:

- **Sleeping at the inn is unchanged.** This game already dreams there, in a
  form built from its own parts and keyed on the town's law rather than on
  Zangband's sanity system — which this project does not have and is not going
  to. Making those dreams darker under nightmare would mean inventing a number
  for a piece of the game you meet once a night at most.
- **A sleep attack cannot give you nightmares, because nothing can put you to
  sleep.** There is no player sleep in this game at all — monsters can paralyse
  you, which is a different thing. Zangband's trigger has nowhere to attach.
  *This one is deferred rather than refused*: if a sleep state ever arrives for
  some other reason, the nightmare can hang off it.

A note on the spoiler
---------------------

If you find Zangband's own nightmare documentation, it describes a great deal
more than this: doubled monster damage, monsters that never miss, spells that
never fail, resurrection, home robbery, and about thirty other things. Most of
it was never written. Zangband 2.7.5 checks its nightmare flag in twenty-four
places and does nineteen things, and this game is built from those. Anything
added beyond them will be described here as ours, and not as restoring
something.
