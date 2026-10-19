Nightmare Mode
==============

.. note::

   **Complete.** Milestone M11 built nightmare mode, and this chapter was
   written as each piece landed. The milestone closed on 4 October 2026
   (DEC-17).

   Requirements: BAL-15 to BAL-18.

Nightmare mode is a choice you make at birth and can't undo, and it's meant to
be unfair. Zangband's own description of the option is
*"this isn't even remotely fair!"*, and its documentation says the mode is
*"not intended to be winnable"*. Here's everything it changes, so you know what
you're taking on, and what it doesn't. The second list is there because the
mode has a spoiler that describes about three times as much as Zangband ever
built.

Choosing it
-----------

**Nightmare mode** is a birth option, off by default. Like every birth option it
can only be set while you're creating a character, and it can't be turned off
afterwards. That's what makes it irreversible, and it's why it lives in the
birth menu and nowhere else.

It is not on the race and class menus themselves. Press ``=`` at any of them for
the birth options, where it's the last of the sixteen:

.. code-block:: text

   Nightmare mode (this isn't even remotely fair!)  : no   (birth_nightmare)

Press ``y`` on it, then ``ESC`` back to the character you were making.

Knowing you are in it
---------------------

A character playing under nightmare mode has **Nightmare** on the status line,
in red, for as long as it lives. It sits beside ``Cheat`` and is there for the
same reason: the mode changes sixteen things and every one of them looks, from
inside the game, like the ordinary game going badly. A monster with twice the
hit points looks like a monster. A sustain that failed looks like a sustain
that was never bought. A wall that looks like floor looks like floor.

Zangband didn't have that marker. It showed the mode in one place only, the
character dump, and a dump is a file you write after the character is dead.

The dump still says it too, as ``Nightmare Mode: ON``, because that's where
characters get compared with each other. A character that died at depth 20
under nightmare did something quite different from one that died at depth 20
without it.

What it changes
---------------

Monsters, so far. Each of these is what Zangband does, with the source it comes
from.

Monsters have twice the hit points: everything you meet has double what it
would otherwise have. That's on top of the game's own lethality scaling, so a
monster's hit points are ``base × 0.73 × 2``.

Every monster in the game also gets a permanent ``+5`` to speed, on top of
whatever haste or slow is acting on it. At normal speed that's the difference
between trading blows and being hit twice for every one you land.

Monster recall shows both of these as they'll be in the fight. In nightmare
mode the page you read before fighting something already has the doubled hit
points and the extra five points of speed in it, whatever the monster's record
says.

Nothing is asleep when it arrives. Monsters that would normally be found
sleeping are awake from the moment the level is made. They can still be put to
sleep afterwards; the mode doesn't stop that, whatever its spoiler says.

Nothing waits for you, either. Ordinarily the game marks certain dangerous
monsters so that they give you one free move when they first appear. That
courtesy stops a summoned horror acting before you've seen it, and under
nightmare there isn't one.

Everything arrives half-started. A monster placed on a level begins with a
random amount of energy towards its next turn, and under nightmare it begins
with twice as much, so in practice it acts sooner after it appears.

Sleepers are twice as alert. A sleeping monster's chance of noticing you on any
given turn is doubled. Stealth still works, but only half as well.

Sustains fail, and drains stick. A sustain holds twelve times in thirteen
instead of always, and when a stat drain does land it's permanent twelve times
in thirteen instead of wearing off. This is probably the cruellest thing in the
mode, and nothing in Zangband's own documentation mentions it. A sustain is a
property you buy once and stop thinking about, and one that fails now and then
is a different item from one that holds.

No stairs can be made. Stair creation does nothing, so there's no scumming for a
convenient descent and no escape hatch when a level turns out badly. You leave
a floor by the stairs that are on it.

What lives on a floor isn't what should live there, and that's two changes at
once. The game's ordinary rule for putting something out of its depth adds a
few levels at most; under nightmare it multiplies, uncapped, so a monster on
the fifth floor can be one you'd expect eighty floors down. And the monsters
normally pinned to their own depth (the ones the game will never generate
early, whatever else it does) aren't pinned any more. Only quest monsters
still are.

Some walls look like floor: a few on every level, and one door in several
hundred. They are still walls. You can't walk through them or see past them,
and nothing about how they're drawn will tell you. You find them by walking
into them.

A bell tolls before midnight, and then something answers it. It rings four
times in the last hour, at eleven and at each quarter past, and on the stroke
of midnight the Ancient and Foul Curse falls on you wherever you are. The bell
is the only warning the mode gives you about anything, so heed it. There's
time to drink something, or to be somewhere else.

Word of Recall occasionally takes you somewhere worse. About one recall in six
hundred and sixty-six arrives deeper than you asked: twice as deep in the upper
dungeon, half the remaining way down in the lower. It won't take you past the
bottom of the dungeon you're recalling into, so in a shallow one this does
little. In a deep one it can end a character who was going shopping.

And four things it does not do
------------------------------

The list above is everything Zangband's nightmare mode does. These four are
*ours*: Zangband's spoiler describes a long menu of ideas its code never
implemented, and these were the four worth building. They're kept apart from
the list above because that list is a port and this one is a design. None of
it restores anything.

Breeders fill a level two and a half times over. The cap on how many breeding
monsters a level will hold goes up from 100 to 255, so a floor of giant white
lice or mice, left alone, gets very much worse than it ordinarily would.
Scrolls of Sterilize still work and still stop it.

Your bolts bounce off you half as reliably. A character with reflection
ordinarily turns aside nine bolts in ten, and here eight in ten. Monsters that
reflect are unaffected: they bounce your bolts back exactly as often as they
always did. The property is still worth having, but it isn't close to a
guarantee any more.

The changes chaos works on you are worse ones. Ten of the mutations are pure
penalty (puny, moronic, albino, rotting flesh, a blank face, short legs,
arthritis, elemental vulnerability, extra noise and extra fat), and in
nightmare mode each is twice as likely to be the one you get. Ordinarily a
mutation lands on one of them about one time in eight; here it's closer to one
in five. Two others are just as bad on paper and are left alone, because they
do nothing at all. A silly voice and bad luck both lost their mechanism when
4.2 removed charisma and the identify moment, and making a no-op likelier
does not make anything nastier.

The shopkeepers are poor, and they never leave. Each shop is run by whichever
of its owners has the smallest purse, and the usual rotation that replaces a
shopkeeper every so often doesn't happen. So the most any shop will pay you for
a find is as low as it can be, for the whole game, and waiting for a richer
buyer isn't a plan.


Four more things won't be built, and two of those are permanent decisions:

- Your score is unaffected. Zangband gives nightmare characters a score bonus,
  but this game's scoring has no multiplier of any kind to attach one to, for
  any birth option. Adding one would mean inventing a system just to port a
  number.
- A Golem keeps its immunity to stunning. Zangband takes it away, but not for
  nightmare alone: two other difficulty options do the same, and this game has
  neither of them.

The other two are the mode's two nightmares, which Zangband triggers at the inn
and when a sleep attack lands on you:

- Sleeping at the inn is unchanged. This game already dreams there, in a form
  built from its own parts and keyed on the town's law instead of Zangband's
  sanity system, which this project does not have and is not going to. Making
  those dreams darker under nightmare would mean inventing a number for a piece
  of the game you meet once a night at most.
- A sleep attack can't give you nightmares, because nothing can put you to
  sleep. There is no player sleep in this game at all. Monsters can paralyse
  you, but that's a different thing, and Zangband's trigger has nowhere to
  attach. This one is deferred, not refused: if a sleep state ever arrives for
  some other reason, the nightmare can hang off it.

A note on the spoiler
---------------------

If you find Zangband's own nightmare documentation, it describes a great deal
more than this: doubled monster damage, monsters that never miss, spells that
never fail, resurrection, home robbery, and about thirty other things. Most of
it was never written. Zangband 2.7.5 checks its nightmare flag in twenty-four
places and does nineteen things, and this game is built from those. Anything
added beyond them will be described here as ours.
