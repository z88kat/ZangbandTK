Virtues
=======

Your character is measured against eight virtues, drawn at birth from a pool of
eighteen, and where they stand in each moves with how you play.

You don't choose them. Your class, your race and the magic you study choose
them for you, so a Warrior and a Necromancer aren't asked the same questions,
and two Warriors of different blood aren't asked quite the same questions
either. You aren't told at birth which eight you drew; you find out by watching
what the world makes of you.

The eighteen
------------

Compassion, Honour, Justice, Sacrifice, Knowledge, Faith, Enlightenment,
Mysticism, Chance, Nature, Harmony, Vitality, Unlife, Patience, Temperance,
Diligence, Valour, Individualism.

Nine of them are general enough that any character might be asked about them:
Sacrifice, Compassion, Valour, Honour, Justice, Temperance, Harmony, Patience
and Diligence. The last slots are filled from those. The other nine come only
through what you are. A Necromancer is measured against Unlife because of what
a Necromancer is; a Warrior never will be.

How they move
-------------

There's no vow to take and no altar to swear at. A virtue moves when you do
something that bears on it, and most of the time you won't be told:

- Killing is the busiest of them. Something far above your depth is brave; a
  unique is a thing removed from the world for good; an angel and a demon are
  not the same death. Killing the harmless (the beggars and drunks of the
  towns) is noted, and not kindly.
- Striking something asleep costs you Compassion, and Honour as well unless
  you're a Rogue, for whom it's just the job.
- Hasting yourself is impatient and industrious at once. Becoming invulnerable
  is the largest single mark against you anywhere in the game: twenty-five
  points across four virtues, most of them off Valour. The charge is on being
  invulnerable, however you got there, so the mutation that makes a character
  invulnerable at random is charged the full amount every time it fires,
  without the character having chosen anything. See
  :doc:`mutations`.
- Digging is diligent. Clearing a stand of trees is diligent and something
  else besides.
- Mapping a level you haven't walked is knowledge got cheaply. Enchanting a
  weapon is faith placed in the work rather than the world.
- Collecting a bounty is the one obviously just thing most characters ever do.

A standing runs from 125 for to 125 against, and reads on a ladder: neutral, then
somewhat virtuous, virtuous, very virtuous, a champion, a great champion, and at
the top the living embodiment of the thing. Downward it runs from straying,
through sinning, to enemy, bitter enemy, arch-enemy and finally the polar
opposite.

What reads them
---------------

Two things read them, and Zangband had neither. The first is the Lords of the
Courts of Chaos. A Chaos-Warrior's patron (or whoever has taken an interest in
a character carrying the right item) hands down a reward on every level gained,
from the generous to the actively hostile.
What it reaches for depends on how you've lived. A Lord of Chaos isn't
impressed by a well-run life: it recognises Chance and Individualism, and it
wearies of Harmony and Temperance. Live chaotically and the worst of the ladder
comes up less often.

The second is the dream at the inn. A night's sleep can show you a true thing,
a place on the map you hadn't found, or it can show you a dark one. Where you sleep
counts for most, and a lawful town gives clearer nights than a frontier one.
But a sleeper who has spent their life looking sees further. Enlightenment and
Knowledge make a true dream likelier; Unlife and Chance make a dark one
likelier.

Neither effect is large, and they aren't meant to be. A virtue at full stretch
moves the patron's roll two steps and the dream's odds by a few points in a
hundred. That's small, but in Zangband it was nothing.

Where to see them
-----------------

Press ``C`` for your character sheet and then space until you reach the
**Virtues** page. It's the fourth, after your statistics, your equipment flags
and your mutations. There are eight lines, one per virtue, each saying where
you stand on a thirteen-rung ladder that runs from *the polar opposite of*
through *neutral to* up to *the living embodiment of*.

There are no numbers. Zangband didn't show them either, and the rung is what
means something: knowing you're *very virtuous in Valour* tells you what a Lord
of Chaos will make of you, and knowing you're at 47 doesn't.

The same eight lines appear in the character dump ``f`` writes, in a
``[Virtues]`` section, so you can read a dump and the sheet against each other.

Zangband showed them in neither place; its knowledge-screen entry was
commented out. For a long time this game only wrote them to the dump, and a
file you have to ask for and open in something else isn't the same as being
told.

Where this came from
--------------------

Topi Ylinen wrote the virtue system for Zangband in 1998, as an Ultima IV-style
measure of how a character behaves. It was never finished. By the time
development stopped in 2005 there were 168 places in the source that *wrote* a
virtue and not one that read a virtue back, and the knowledge-screen entry that
would have shown them to the player was commented out with the note *"Display
virtues option is always left out"*. Every character in Zangband had eight
counters that moved with everything they did, and in seven years not one line
of code ever looked at them.

The selection is Zangband's, near enough unchanged, because that part already
worked. The two things that read a virtue are new, and so is the rule that
every virtue a character can be given must be one their play can move. The
original had no such rule and wouldn't have needed one, since nothing was
reading.
