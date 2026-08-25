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

Not here yet
------------

Three of Zangband's six kinds of work are not built: killing something at a named
depth of a named dungeon, clearing a stretch of open country, and being sent to
find a particular object. The machinery is there — each is a value in the same
enumeration and a check in the same two places — but the errands themselves are
not written.
