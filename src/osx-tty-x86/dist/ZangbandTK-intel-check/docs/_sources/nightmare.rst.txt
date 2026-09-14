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

Not built yet
-------------

The rest of M11, and worth listing because a player reading this should know
what the mode does *not* currently do: more out-of-depth monsters and
depth-restricted monsters appearing above their level; sustains that fail and
stat drains that stick; invisible walls; the midnight curse and the bell that
warns of it; Word of Recall sending you deeper than you asked; stair creation
being disabled; and nightmares at the inn and from a sleep attack.

And two things that will never be built, which are decisions rather than gaps:

- **Your score is unaffected.** Zangband gives nightmare characters a score
  bonus. This game's scoring has no multiplier of any kind to attach one to, for
  any birth option, so adding one would be inventing a system rather than
  porting a number.
- **A Golem keeps its immunity to stunning.** Zangband takes it away, but not
  for nightmare alone — two other difficulty options do the same, and this game
  has neither of them.

A note on the spoiler
---------------------

If you find Zangband's own nightmare documentation, it describes a great deal
more than this: doubled monster damage, monsters that never miss, spells that
never fail, resurrection, home robbery, and about thirty other things. Most of
it was never written. Zangband 2.7.5 checks its nightmare flag in twenty-four
places and does nineteen things, and this game is built from those. Anything
added beyond them will be described here as ours, and not as restoring
something.
