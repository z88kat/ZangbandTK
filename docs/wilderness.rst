The Wilderness
==============

Angband is played in a single dungeon beneath a single town. ZangbandZK is
played across a generated overworld, and the town stands on it.

This is the change that most alters how the game feels from the first turn. You
begin, as in Angband, on the down staircase in the middle of town, surrounded by
shops. But the town does not end in a wall. Walk to its edge and you keep
walking, out into whatever country the world put there.

.. note::

   **Milestone M4.** The world generates, the town stands in it, and you can
   walk out of the town and back into it. Several things stated in the Phase 1
   requirements are not here yet, and are listed under `What is not here yet`_
   at the end of this chapter rather than being quietly omitted.

The world
---------

The world is a square grid of **blocks**, each 16 by 16 grids. At the default
size of 129 blocks across — Zangband's own — that is a world of 2064 by 2064
grids, some two thousand times the area of an Angband town. You will not walk to
the edge of it by accident.

Every world is generated from a single seed, fixed when your character is
created. That seed decides everything: where the sea is, where the mountains
are, where your town stands. Two characters made from the same seed walk the
same world. A character always returns to the world they were born in, because
the seed is stored in the savefile and the world is rebuilt from it rather than
saved.

That is the reason the world can be as large as it is. Nothing about it is kept
in the savefile except the seed and where you are standing.

Terrain
-------

Each block has a position in a three-dimensional space — how high it stands, how
settled it is, and how orderly. Terrain follows from that position rather than
being chosen at random, which is why the world comes out looking like a place
rather than like noise: coastlines run, forests mass together, and mountains
form ranges.

The seven kinds of country, and roughly what each is made of:

.. list-table::
   :header-rows: 1
   :widths: 20 80

   * - Terrain
     - What you find there
   * - Ocean
     - Deep water. About a quarter of the world, as in Zangband. See
       `Deep water`_ — it is crossed, not walled off.
   * - Shore
     - Sand and shallow water, with grass behind it.
   * - Grassland
     - Open grass, scattered trees. The easiest country to cross.
   * - Forest
     - Mostly trees, with clearings.
   * - Swamp
     - Water, mud and scrub. Slow going.
   * - Wasteland
     - Bare earth, rubble and rock.
   * - Mountains
     - Rock, broken by passes.

.. _deep water:

Deep water
----------

Deep water is not a fence. You can wade into it, as you could in Zangband, and
what stops you is weight rather than the water.

Carry no more than half what your strength allows and you keep your head above
it and cross freely. Carry more than that and you begin to drown, a little each
turn, for as long as you stay in — and the game asks you to confirm before you
step in. Anything that lets you float over the ground carries you over water as
well, at no cost and no matter what you are carrying.

Nothing else can swim yet. Monsters treat deep water as terrain that will kill
them and keep out of it, which makes water a reliable way of breaking pursuit.
Zangband's swimming and aquatic monsters are recorded against the imported
bestiary and waiting on the flag to drive them.

Roads run between the places worth reaching. A road is safer and quicker than
the country beside it, and it is a reliable way of finding a town: follow one
far enough and it will take you somewhere.

Travel
------

You walk. There are no travel commands and no map transitions — the wilderness
is one continuous surface, and you cross it a step at a time as you would cross
a dungeon level.

What the game keeps live around you is a **window** onto the world: nine blocks
by nine, or 144 grids square. This is not a boundary you can see or reach. When
you come within a block's width of the window's edge, the game quietly rebuilds
it centred further along, and you carry on walking. Your position in the world
is what the game tracks; where you stand in the window is worked out from it.

The one edge you can reach is the edge of the world itself. It is not walled.
The land runs out into open sea some way before it, so what you find at the end
of the world is more sea, running away past the point where the map stops. You
can look out over it. You cannot sail it.

What lives there
----------------

The wilderness is inhabited, and how dangerous it is depends on one thing: how
far the law reaches. Orderly country is quiet. Lawless country is not, and the
worst of it is as dangerous as the deep dungeon.

Your town is not placed at random. It stands in the most orderly ground the
world has to offer, and because order spreads by contiguity, the country around
it is orderly too. So the first hour is survivable, and the danger climbs as you
walk away from home rather than waiting on the doorstep. If you want to know
whether you have gone too far, the answer is usually that you have, and that you
noticed a little late.

How much lives there is a separate question, and depends on how much the land
can support: a lush valley teems, a waste is bare. More things are abroad at
night than by day.

Monsters cannot swim, so deep water is a reliable way of breaking pursuit.

What you leave behind
---------------------

Drop something in the wilderness and it stays where you dropped it — for a
while. The longer you leave it, the likelier it is that somebody has come along
and found it, and how long that takes depends on where you left it. A sword
dropped outside a busy town is gone by morning. One left in a waste may lie
there a good while.

It is a half-life rather than a deadline, so most things go early and the
occasional one survives a remarkably long time. If you leave something valuable
in the wilderness and mean to come back for it, come back soon, and remember
that "soon" means something different in settled country.

What you are carrying is yours, of course, and so is anything you are standing
on when the world scrolls beneath you.

What you wounded
----------------

Ordinary monsters are not remembered. Walk far enough away and the country is
repopulated rather than resumed — which amounts to the same thing as the ones
you left having recovered and moved on, because that is what they would have
done.

Named monsters are remembered. Bring a unique to within an inch of its life,
walk away, and it is still out there: the same one, still hurt, though it will
have healed by the time you find it again and it will not be where you left it.
It will also be awake and no longer afraid, so the second meeting is generally
worse than the first.

One consequence worth knowing. While you are underground, nothing is holding
that unique's place in the world, so it is free to turn up in the dungeon
instead. If it does, it is no longer in the wilderness, and the world quietly
forgets it was ever there.

What you know
-------------

You know your town. You have lived in it, and it is on your map from the first
turn, all of it.

You do not know the world. Angband hands you the whole of the town level as soon
as you arrive, which is fair when the town level is all there is; doing the same
here would hand you the coastline, the forests and the mountain ranges before
you had taken a step. So beyond the town's edge the ordinary rules apply — you
learn the world by looking at it, and daylight lets you see a long way across
open ground but tells you nothing about what is behind the trees.

Leaving the town
----------------

Follow a road out. There is no gate and no transition — you just walk.

The town is Angband 4.2's town, entire: the same clearing, the same streets, the
same shops, ruins and rubble, and the same rock standing around it. That rock
earns its keep in a world in a way it never had to as a level of its own. It is
what keeps the wilderness out of the market square, and it is what stops you
seeing half a county from the staircase.

What ZangbandZK adds is a way out on each of the four sides, cut through the rock
where a road would meet the town: at the end of a street, not at a corner.

Zangband's own towns were walled and moated, with locked gates. That is not
being copied: it is what Angband 2.8.1's town looked like, which is the game
Zangband was built on, and 4.2 has replaced it with something better. What is
taken from Zangband is the part that is actually a Zangband idea — that the town
stands in a world you can walk out into.

Going down
----------

The down staircase in the middle of town works exactly as it does in Angband:
stand on it and press ``>``.

Coming back up puts you where you went down. The world remembers where you left
it, so a character who walked three days into the hills before finding a way
underground comes back up in the hills, not in the town.

.. _what is not here yet:

What is not here yet
--------------------

Stated plainly, because a manual that only describes what works is not much use
for judging what to expect:

**One town, and one way underground.** The world holds a single town and the
single staircase in it. Several towns of differing size and character — carrying
different stores, so that a frontier village is not a city with the same eight
shops — several dungeons with their own depth ranges, and the buildings and
services that go in them are all milestone M5.

**No overhead map.** There is no way to see the world except by walking it. The
map display is M5.

**Rivers and lakes are missing**, and the terrain is coarser than Zangband's.
Zangband distinguished about thirty kinds of country, including impassable
jungle, snow and lava; ZangbandZK currently has seven. Snow in particular
implies a climate axis the parameter space does not yet carry.
