Quests
======

Angband has two quests and you are on both of them from the moment you are born:
kill Sauron, then kill Morgoth. Nobody gives them to you and there is nothing to
report back. ZangbandTK keeps that shape for the ending and adds the other kind —
work you are offered, accept, carry about with you, and hand back.

Where work comes from
---------------------

From a building. Walk into an inn that is hiring and you will be offered a job
before you are offered a bed.

Quest-giving is a **property a building carries**, not a building of its own.
There is no quest-giver's hut and there never will be: the inn simply has the
property at the moment, and it could as easily be the magetower commissioning a
retrieval or a temple sending you on a pilgrimage. That is deliberate, and it
means a new source of work costs a line rather than a new building, a new door
and a new kind of terrain.

The inn has it for a reason worth knowing. It is where people who have been
somewhere else are sitting — and because a town that has fallen keeps no services
at all, the work dries up exactly where you would expect it to, without any rule
needing to say so. About half the towns in a world are hiring.

What you may be asked
---------------------

**A bounty.** Kill so many of a creature. It counts wherever you find them, above
ground or below, because a bounty is about the creature and not the place. The
work is drawn near your own depth, so it is neither trivial nor suicidal, and
never on a unique — there is only one of those and a bounty on it could be
impossible to fill.

**A delivery.** Carry word to a named town. It is finished by arriving, and only
by arriving: killing the person who asked you does not deliver the parcel.

**Finding a place.** Somebody has heard of a town nobody here has been to. Go and
look at it. This is the only kind of work that gives you knowledge of the world
rather than asking for it back, and it pairs well with a magetower — once you
have stood in a place, the mages will carry you there again.

**A killing down a particular dungeon.** Not just the creature but the place: so
many of them, at a named depth of a named dungeon. Only dungeons you have already
found, and only depths that dungeon actually reaches — a job at a depth its
dungeon does not go to could never be done.

**A killing in the open.** The same, but it counts only what dies above ground.
What you kill underground is nobody's business.

**Fetching something.** Bring a particular thing back. It is finished by having
it, and the check is on your pack rather than on the floor — buying it, or taking
it out of a chest, is fetching it just as much as finding it lying about.

Carrying and reporting
----------------------

Press ``J`` for the list of what you have taken on, how far along each is, and
where the ones about travelling are pointing. The quests the game is *won* by are
not in that list: you are on those from birth, and putting "kill the Serpent of
Chaos" at the top of a first-level character's list would give away the ending
and tell you nothing you can act on.

You can carry several jobs at once. Any building that hires will tell you how far
along you are, or pay you when you are done — the reward scales with what you
were asked to kill and how many of them.

Giving up
---------

Walk into a building that hires while you owe it something and it will list what
you are carrying and offer to let you off. Pick a job, confirm, and it is gone.

This matters more than it sounds. Without it, a bounty on something twenty levels
deeper than you can survive occupies one of your slots until the character dies —
taken, impossible to finish, impossible to be rid of. Giving up is what makes
taking work on a guess a reasonable thing to do.

There is no penalty at present beyond the walk back. Whether the trade should
remember, and charge you for it later, is not decided.

.. note::

   Work can currently be reported at *any* building that hires, not only the one
   that gave it to you. That is a simplification: recording which building sent
   you means another field in the savefile, and the bounty did not need it to
   prove the machinery worked. It will tighten up.

What the game is won by
-----------------------

Two quests, and they are not offered to you by anybody: **Oberon**, at the
ninety-ninth level of the Courts of Chaos, and then **the Serpent of Chaos** at
the hundredth.

Angband ends at Sauron and Morgoth. Zangband replaced them with Oberon and the
Serpent, and that replacement was already the right one for this game — the
Serpent is Zelazny's, not a generic dark lord. The bestiary's own description of
it has been sitting in the imported data all along: *"The Unicorn of Order fought
with Serpent and stole one of its eyes, known as the Jewel of Judgement. With the
Jewel, Dworkin drew the Pattern and thus gave birth to the infinite worlds of
shadow."* The :doc:`Unicorn <monsters>` is in this game. The Serpent is the other
half of her.

A quest names its dungeon as well as its depth, which Angband never needed to do:
it has one dungeon, so a depth is a place. Here the Courts of Chaos run from 75
to 110 and the Abyss from 90 to 127, so depth 100 is two different places — and
without naming the dungeon the ending could have been reached in the wrong one.

All six of Zangband's kinds of work are now written. Which one you are offered
depends on what the world can supply: an errand to another town needs another
town, a job down a particular dungeon needs one you have found, and there is no
sense sending you to look at a place you are standing in. Every kind falls back
to a bounty, which needs nothing but a bestiary.
