Monsters
========

ZangbandTK's bestiary is Angband's, plus 389 creatures drawn from Zangband.
There are 1013 in total, and the additions are not evenly spread: shallow
levels are much as you remember them, while the deep dungeon holds a great deal
you will not.

Where the new monsters come from
--------------------------------

Angband's monsters are Tolkien's. Zangband layered three further sources on
top, and those are what ZangbandTK imports.

**Amber.** The scions of Roger Zelazny's Amber — Corwin, Julian, Fiona, Bleys,
Gerard, Benedict and their kin — appear as deep uniques, along with their
servants and creatures. They are among the most dangerous things in the game and
are meant to be met late.

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

Some imported monsters carry abilities that are recorded but not yet active:
resistance to teleportation, reflecting bolt spells, damaging auras, and the
ability to taunt you. Monsters possessing them behave as though they do not.

A further group of abilities depends on the wilderness, which does not exist
yet: which terrain a monster prefers, which dungeons it inhabits, and whether it
can fly or swim.

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
