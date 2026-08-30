Monsters
========

ZangbandTK's bestiary is Angband's, plus 387 creatures drawn from Zangband.
There are 1013 in total, and the additions are not evenly spread: shallow
levels are much as you remember them, while the deep dungeon holds a great deal
you will not.

Where the new monsters come from
--------------------------------

Angband's monsters are Tolkien's. Zangband layered three further sources on
top, and those are what ZangbandTK imports.

**Amber.** The scions of Roger Zelazny's Amber appear as deep uniques, along with
their servants and creatures. They are among the most dangerous things in the
game and are meant to be met late. There are twelve of Oberon's blood — Oberon
himself, Benedict, Corwin, Eric, Caine, Gerard, Julian, Bleys, Fiona, Brand,
Dworkin and Rinaldo — and the game treats them as a family rather than as twelve
separate uniques. Look one up and its recall calls it an *Amberite*; some things
in the depths can call the whole house down on you at once; and see
:ref:`blood-curse` below for what killing one may cost.

**The Cthulhu Mythos.** Nyarlathotep, Hastur, Shub-Niggurath and Tsathoggua
dwell at the bottom of the dungeon, attended by formless spawn and dark young.

**Chaos.** Creatures of raw chaos, unstable and unpredictable, scattered
throughout the depths.

Monsters that are not what they appear
--------------------------------------

Zangband is fond of monsters that impersonate scenery, and ZangbandTK keeps
them. A weapon lying on the floor may be a **death sword**; a cloak may be a
**cloaker**; a door may be a **door mimic**; a patch of floor may be something
considerably worse. Two of these classes are new to this game and display as
Angband has no symbol for them:

- ``#`` — wall monsters: sentient stone, drifting mists, and things that live
  inside the dungeon's structure
- ``|`` — animated weapons, which hunt rather than wait

The rest borrow Angband's own symbols where its bestiary already had a suitable
class: floating spheres display as vortices, ents as trees, standing water as
elementals.

.. _blood-curse:

The blood curse of Amber
------------------------

Killing one of Oberon's blood is not free. About half the time, a dying Amberite
lays a blood curse on whoever killed it: your equipment is cursed, and the
Ancient and Foul Curse falls on you two to four times over. That curse is the
worst thing in the game — see :doc:`objects` — and having it arrive several times
in a row, at the end of a fight you only just won, is how a good character dies.

It is worth knowing before you pick the fight rather than after. There is no way
to prevent it and no saving throw against it; the only choice is whether to make
the kill at all, and whether to make it in a state fit to survive the aftermath.

Zangband filed this under a two-word comment: *don't kill Amberites*. It comes
out of the books, where a dying prince's curse is a real and lasting weapon —
Corwin lays one on Eric at the end of *Nine Princes in Amber*, and it outlives
them both.

Monsters that talk
------------------

Eighty-nine of the imported monsters have something to say. They speak in a
fight, roughly one turn in eight and only when you can see them; they say
something different once they have turned and run; and they have a last word
when they die. It is Zangband's own writing, and it is a large part of why its
dungeon felt inhabited rather than stocked.

There is one thing in it that is not flavour. When a **unique** that could talk
dies, about one time in ten it turns out there was a price on its head:

.. code-block:: text

   There was a price on Robin Hood, the Outlaw's head.
   Robin Hood, the Outlaw was wanted for smuggling.
   You collect a reward of 4750 gold pieces.

The reward scales with the monster's level and runs from 250 gold to 32,000, so
a deep unique that happened to be wanted is worth a great deal more than its
drop. There is no way to seek this out — you cannot tell in advance which
monsters were wanted, and the same monster is not always wanted twice — but it
is a reason to read what the dead say rather than scroll past it.

Monsters that will not be moved
-------------------------------

A hundred and two of the imported monsters resist being teleported. This is the
answer to a tactic Angband lets you rely on: *Teleport Other* removes anything
inconvenient, and against these it may simply fail.

It is not a flat immunity, and the difference matters:

- A **unique** that has it is *unaffected*, every time. If you were planning to
  teleport Oberon out of the way and fight his escort, plan something else.
- **Anything else** gets a saving throw, and a heavy monster makes it more often
  than a light one. The same creature may be shifted one turn and stand its
  ground the next, so a failed attempt is worth repeating — up to a point.

You will see ``is unaffected!`` for the first and ``resists!`` for the second,
which is how you tell which you are dealing with. The distinction is recorded in
the monster's recall once you have seen it happen: *cannot be teleported*.

It applies to every way the game has of moving a monster against its will — the
teleport-other effects, the blink a nexus attack causes, and the shove of a
gravity attack — so there is no side door.

Things it is a mistake to touch
-------------------------------

Fifty-nine of the imported monsters are wrapped in something. Strike one in
melee and it answers: ``You are suddenly very hot!``, ``very cold``, or ``You
get zapped!``, and the damage rises with how big the monster is and how deep it
lives. Immunity to the element stops it dead; resistance only blunts it. Some
carry two at once, and the recall says so — *surrounded by flames and
electricity*.

The one thing worth knowing before you meet a coast: **a monster wrapped in fire
will not enter water**. Not the deep sea and not the shallows. It is the oldest
tactical fact in this game that Zangband wrote down and never got to use, having
no water worth the name; here there is a shoreline, and stepping into it is a
way to break off a fight you are losing.

Twenty-two others reflect bolt spells. Not balls, not breaths — a single bolt,
and about nine times in ten. The bolt does not come back at you: it goes to a
grid *beside* you, so it may well miss, and it may equally well hit something
standing next to you.

You can have all three yourself. A shield **of Reflection**, a cloak **of
Immolation** or **of Electricity** does for a monster exactly what the monster's
aura does to you, on the same terms — a monster immune to the element shrugs it
off, and your reflected bolt scatters beside whatever cast it.

Five that will not hold still
-----------------------------

The **chaos shapechanger**, the **lord of chaos**, the **unmaker**,
**Nyarlathotep** and **Dworkin Barimen** are not drawn as themselves. Each time
the display refreshes — which for these is every game turn — the symbol is
replaced by that of some other creature entirely, and about one time in
twenty-five by an *object* instead. A shapechanger looks like a kobold, then a
dragon, then a mushroom, then something else.

Nothing about the monster changes. It has the same stats, the same attacks and
the same behaviour throughout; only the glyph moves. That is worth knowing
because the obvious inference — that the thing has actually become a dragon and
should be fought like one — is wrong, and acting on it is how the flag kills
people.

The company is not accidental. Four of the five are creatures of chaos, and the
fifth is Dworkin, who drew the Pattern and lost his mind doing it. In this game
he is both a shapechanger and one of Amber's blood, and he is the only monster
who is both.

The one that may not be there
-----------------------------

There is a single **quantum monster** in the game, and it is a poor thing to
rely on. Half of what you swing at it passes through — not a miss you can
improve on with skill, simply an attack that did not happen. It takes only half
its own turns. And now and then it stops existing: no death, no corpse, no
experience, just an absence where it was.

You cannot be sent to kill one, which is deliberate: a quest whose target can
evaporate is a quest that cannot be finished.

How dangerous are they?
-----------------------

Every monster in the game — Angband's and Zangband's alike — carries 73% of its
Angband hit points and 50% of its armour class. See :doc:`balance` for why, and
for how to change it.

Imported monsters were placed on Angband's own difficulty curve rather than
keeping Zangband's numbers, so a Zangband monster at depth 40 should be a
reasonable match for an Angband monster at depth 40. Where Zangband's version
was unusually tough or unusually fragile for its depth, that relationship was
preserved.

This is the least-tested part of the game. If something at a given depth feels
badly out of place, that is worth reporting.

Rooms full of them
------------------

Angband gathers monsters of a kind into pits and nests, and ZangbandTK's imports
make three more of those possible:

- **Forests** — ents, huorns and the older things of the wood
- **The living dungeon** — sentient walls and the mists that drift between them
- **Elemental** — spirits and elementals of the four elements

Not yet implemented
-------------------

Zangband also tagged every monster with the terrain it prefers and the dungeons
it belongs in. Those tags are unused here, because this game decides both
questions a different way — a dungeon lists the *kinds* of creature that live in
it, and a fish is kept in the water by being a fish. The tags are recorded
against the monsters rather than thrown away, in case that ever changes.

Things that live in the water
-----------------------------

Twenty-four of the imported monsters are aquatic, drawn as ``N``, and they are
the only creatures in the game with a place of their own to live. They are in the
open sea and the shallows, in the flooded dungeons beneath Amber — Faiella-Bionin
and Rebma — and nowhere else at all. A shark cannot come ashore.

They arrived badly. Zangband drew its aquatic monsters with ``l`` and Angband
draws *trees* with ``l``, so the import gave every fish, whale and kraken in the
game the character of a tree: unable to feel fear, unable to be confused,
regenerating, and — because dungeons choose their inhabitants by kind — living in
the forest of Arden. That is fixed, and the fixing is why they now have a letter
of their own.

Not everything is there to be fought
------------------------------------

A **white deer** is a ``q`` in white, standing too still and too unafraid of
you. Walk into it and it heals you — everything, whatever you were down to — and
then bounds away a good ten grids and carries on grazing.

It does that once. Follow it and touch it again and it shies from your hand and
bounds off, and that is all. The beast remembers, and so does your savefile, so
reloading does not persuade it either.

You can kill it, if you want to. It carries nothing and is worth no experience,
and it will not fight back.

Deer are uncommon and live in quiet country. Finding one is luck, not a plan.

And there is one of them that is not a deer.

**The Unicorn of Amber** is silver-white, watches you with an interest you have
not earned, and is always about to leave. She is the oldest thing in Amber and
there is exactly one of her in the world.

Walk into her and she makes you whole. Not your wounds only — everything the
healer in a town will sell you and charge for: the poison, the cuts, the
stunning, the blindness, the confusion, the fear, your drained stats, and the
levels a life-draining touch took off you. All of it, at once, for nothing.

Once. Then she goes about her business, and if you find her again she will shy
from your hand like any other beast.

She will not fight you, and killing her is possible and is exactly the sort of
thing the Courts would do.
