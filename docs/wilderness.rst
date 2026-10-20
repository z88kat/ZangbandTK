The Wilderness
==============

Angband is played in a single dungeon beneath a single town. ZangbandTK is
played across a generated overworld, and the town stands on it.

It changes how the game feels more than anything else, from the first turn. You
begin, as in Angband, on the town's down staircase, a short walk from the
shops. But the town doesn't end in a wall. Walk to its edge and you keep
walking, out into whatever country the world put there.

.. note::

   The world is done: it generates, towns stand in it with their shops and
   services, roads run between them, dungeon mouths open on it, and there's
   work to be had in the towns (see :doc:`quests`). Where it's still narrower
   than Zangband, that's by choice or by the shape of the thing, not because
   it's unfinished. Those places are listed under `Where this is narrower than
   Zangband`_ at the end of this chapter.

The world
---------

The world is a square grid of **blocks**, each 16 by 16 grids. At the default
size of 129 blocks across (Zangband's own), that's a world of 2064 by 2064
grids, some two thousand times the area of an Angband town. You won't walk to
the edge of it by accident.

Every world is generated from a single seed, fixed when your character is
created. That seed decides everything: where the sea is, where the mountains
are, where your town stands. Two characters made from the same seed walk the
same world. A character always returns to the world they were born in, because
the seed is stored in the savefile and the world is rebuilt from it, never
saved.

That's why the world can be as large as it is. The savefile keeps the seed,
where you're standing, and the handful of things that can't be recomputed from
a seed: what you've dropped, which named monsters you've wounded, and where
you've been.

Terrain
-------

Each block has a position in a three-dimensional space: how high it stands, how
settled it is, and how orderly. Terrain follows from that position instead of
being chosen at random, which is why the world comes out looking like a place.
Coastlines run, forests mass together, and mountains form ranges.

The seven kinds of country, and roughly what each is made of:

.. list-table::
   :header-rows: 1
   :widths: 20 80

   * - Terrain
     - What you find there
   * - Ocean
     - Deep water. About a quarter of the world, as in Zangband. You can cross
       it; see `Deep water`_.
   * - Shore
     - Sand and shallow water, with grass behind it.
   * - Grassland
     - Open grass, scattered trees. The easiest country to cross.
   * - Forest
     - Mostly trees, with clearings.
   * - Swamp
     - Water, mud and scrub. Mostly mud.
   * - Wasteland
     - Bare earth, rubble and rock.
   * - Mountains
     - Rock, broken by passes.

.. _deep water:

Deep water
----------

You can wade into deep water, as you could in Zangband, and what stops you is
the weight you're carrying.

Carry no more than half what your strength allows and you keep your head above
it and cross freely. Carry more than that and you begin to drown, a little each
turn, for as long as you stay in, and the game asks you to confirm before you
step in. Anything that lets you float over the ground carries you over water as
well, at no cost and no matter what you're carrying.

Don't count on it to break pursuit. A monster that would drown keeps out of
deep water, so wading does shake off the things that only walk. But two large
groups of the imported bestiary don't care: the ones that swim cross deep water
without being fish, and the ones that fly cross anything at all without
touching it. The flyers are the larger group of the two, and a raven or a
dragon will come over the water after you as though it weren't there. The
counts are in ``lib/gamedata/monster.zangband.txt``, as ``CAN_SWIM`` and
``CAN_FLY``; between them they cover a couple of hundred creatures.

Only true fish are barred from the shore. A shark can't follow you onto the
beach, and neither can a kraken, but a lizardman swims and walks, so the
waterline is a line both ways only for the things that live in the sea.

Rivers and lakes
----------------

Rivers rise in the high country and wander downhill to the sea. A river is six
or eight grids across with ragged banks, deep in the middle and shallow at the
edges, so crossing one is a decision: wade it lightly loaded, or go round, or
find where it narrows.

Lakes sit in hollows inland. There aren't many. Four are attempted per world,
and any that would have reached the coast is abandoned, not moved, so a world
with a lot of coastline has fewer of them.

Towns are never built in water, though they're often built beside it.

Roads join the towns. A road takes the cheapest way across the country, so it
runs down the valleys, keeps out of the swamp and goes round a mountain instead
of over it. It keeps to the land as well. Two towns on either side of an inland
sea get a causeway, but that's the one exception the sea is opened for. Roads
don't take short cuts across bays.

Every town and every dungeon mouth is on the network. The roads are laid as a
spanning tree over the towns first, so there's always a road out of the
starting village, and following roads from it reaches every other town in the
world. Then any two towns within ``wild:road-dist`` blocks of each other get a
road directly between them, which turns the tree into a network in settled
country. Last, every dungeon mouth gets a branch road of its own to the nearest
town. About three blocks of the world in a hundred have a road.

If you're lost, find a road and walk along it. It goes somewhere, and
:doc:`towns` covers what you'll find at the other end.

Travel
------

You walk. There are no travel commands and no map transitions: the wilderness
is one continuous surface, and you cross it a step at a time as you'd cross a
dungeon level.

The game keeps a **window** onto the world live around you: nine blocks by
nine, or 144 grids square. You can't see or reach its edge. When you come
within sight of it, the game rebuilds the window centred further along, and you
carry on walking. You shouldn't be able to tell it's happened. The game tracks
your position in the world, and where you stand in the window is worked out
from that, so scrolling can't move you.

The one edge you can reach is the edge of the world itself, and it has no wall.
The land runs out into open sea some way before it, so what you find at the end
of the world is more sea, running away past the point where the map stops. You
can look out over it, but you can't sail it.

What lives there
----------------

The wilderness is inhabited, and how dangerous it is depends on one thing: how
far the law reaches. Orderly country is quiet. Lawless country isn't, and the
worst of it is as dangerous as the deep dungeon.

Your town stands in the most orderly ground the world has to offer, and because
order spreads by contiguity, the country around it is orderly too. So the first
hour is survivable, and the danger climbs as you walk away from home. If you
want to know whether you've gone too far, the answer is usually that you have,
and that you noticed a little late.

How much lives there is a separate question, and depends on how much the land
can support: a lush valley teems, a waste is bare. More things are abroad at
night than by day.

The town's own people stay in the town. Beggars, drunks, urchins and the like
belong to the streets they were born on, and you won't meet them ten miles from
anywhere.

The sea is inhabited too, by its own kind. There are piranha and swordfish in
the shallows, then barracuda, hammerheads and white sharks, and out in the deep
the things that eat those: killer whales, giant squid and the krakens. They keep
to the water, and nothing else keeps to it with them.

The sea is never tame. The danger of open water doesn't fall away near an
orderly coast, because law is a measure of how well a country is policed and
nobody polices the sea. And it's never empty in the way a waste is: the
emptiness of the land is measured by what it can grow, which says nothing about
what swims.

None of it can reach you on the sand. The sea is dangerous to enter, but you can
stand beside it.

Nothing arrives next to you. The country is repopulated as you walk through it,
but never within eight grids of where you're standing, so whatever you meet in
the open you meet at a distance, and you get at least one look at it first.
That's why the wilderness never does what a dungeon level sometimes does and
puts something in your face unannounced.

What you leave behind
---------------------

Drop something in the wilderness and it stays where you dropped it. For a
while, anyway. The longer you leave it, the likelier it is that somebody has
come along and found it, and how long that takes depends on where you left it.
A sword dropped outside a busy town is gone by morning. One left in a waste may
lie there a good while.

It's a half-life, not a deadline, so most things go early and the occasional
one survives a remarkably long time. If you leave something valuable in the
wilderness and mean to come back for it, come back soon, and remember that
"soon" means something different in settled country.

What you're carrying is yours, of course, and so is anything you're standing on
when the world scrolls beneath you.

What you wounded
----------------

Ordinary monsters aren't remembered. Walk far enough away and the country is
repopulated from scratch, which comes to the same thing as the ones you left
having recovered and moved on, because that's what they'd have done.

Named monsters are remembered if you hurt them. Bring a unique to within an
inch of its life, walk away, and it's still out there: the same one, still
hurt, though it'll have healed by the time you find it again and it won't be
where you left it. It'll also be awake and no longer afraid, so the second
meeting is generally worse than the first.

One you never touched isn't remembered, and doesn't need to be. It's
indistinguishable from one the country rolls up fresh, and remembering it would
only pin it to wherever it last stood. If it had been following you, that's
your elbow.

While you're underground, nothing is holding a wounded unique's place in the
world, so it's free to turn up in the dungeon instead. If it does, it's gone
from the wilderness, and the world forgets it was ever there.

The world map
-------------

Press ``M`` on the surface and you get the overhead map of the world: one
character per block, drawn from the same terrain the ground is drawn from, so
what the map calls forest is what you walk into. ``@`` is you. Direction keys
scroll it; ``ESC`` closes it.

Places are picked out in colour, and the bottom of the screen names the colours
as it draws them, so you don't have to remember which is which: **village**,
**town**, **city** and **great city** each have their own, and a **dungeon**
mouth is light red. Blocks drawn in light white are neither. They're the margin
a town or a dungeon keeps clear around itself, which is why they're never built
right up against each other.

The line below the map tells you where you are in world coordinates and how
large the world is, which is the only place either figure is shown.

The world is 129 blocks across and a screen is eighty columns, so the map pans
instead of shrinking to fit. Squeezing a hundred and twenty-nine rows into
twenty-two would lose the coastlines, which are what you want to see.

It shows only what you've seen. A new character knows the country around their
town and nothing else, and the map fills in behind them as they travel. A block
is sixteen grids and you can see twenty, so walking through a block reveals it
and its neighbours. What you've uncovered is saved with the character.

In the dungeon, ``M`` is the usual level map. There's no sky to see the world
from down there.

What you know
-------------

You know your town. You've lived in it, and it's on your map from the first
turn, all of it.

You don't know the world. Angband hands you the whole town level as soon as
you arrive, which is fair when the town level is all there is. Doing the same
here would hand you the coastline, the forests and the mountain ranges before
you'd taken a step. So beyond the town's edge the ordinary rules apply: you
learn the world by looking at it, and daylight lets you see a long way across
open ground but tells you nothing about what's behind the trees.

Trees are the one piece of terrain here that Angband had no equivalent for: you
can walk into them, and you can't see through them. Standing in a wood you see
your own surroundings and very little else, which is what a wood is for.

The time of day counts too. Daylight is what reveals the country; at night you
see as far as your lamp and no further, so the world stops being a map and
becomes a corridor. The status line says ``Day`` or ``Night`` while you're
above ground. Angband never said, because in Angband the surface is one
town-sized level and the difference hardly signifies. Here it's the difference
between seeing where you're going and not.

Day and night
-------------

The whole cycle is 10,000 turns, half of it light and half of it dark, so night
lasts 5,000 turns from the sun falling to the sun rising. Get caught out at
random and you're looking at about 2,500.

That's 5,000 turns *of yours*, at normal speed. The world keeps its clock in
game turns (100,000 of them to the cycle), and a character at normal speed acts
once every ten. Being fast doesn't shorten the night; it shortens your share of
it, so at +10 speed the same night costs you around 2,500 moves. Being slow
costs you more.

The clock keeps running underground. Only the message and the relighting are
held back while you're below; time itself passes at the same rate wherever you
are. So a long dungeon trip changes what you climb out into, and surfacing into
darkness after going down in daylight is normal, not a fault.

Waiting it out isn't realistic: 5,000 turns of resting is not a plan. That's
what the inn is for. It sells you a bed only after dark, and takes you straight
to dawn for 25 gold. See :doc:`towns`.

Leaving the town
----------------

You leave through a gate. There are four, one on each side, and each is two
tiles wide with a door on it.

The town is laid out by Angband 4.2's own town code: the same clearing, the same
streets, ruins and rubble, and the same rock standing around it. Your home
village is the size of Angband's town, but it has only four shops: a general
store, an alchemist, a bookseller and your home (see :doc:`towns`). That rock
earns its keep in a world in a way it never had to as a level of its own. It's
what keeps the wilderness out of the market square, and it's what stops you
seeing half a county from the staircase.

What ZangbandTK adds is the gates. The clearing used to break through the rock
wherever it happened to reach the edge, leaving ragged holes several tiles
across that anything could wander in by. The rock is closed up everywhere but
four gateways, cut at the ends of streets rather than at corners.

Open a gate and walk through. It swings shut behind you shortly afterwards,
unless somebody is standing in the doorway, so the road home doesn't stay
propped open for whatever was following you.

Zangband's own towns were walled and moated, with locked gates. This game
doesn't copy that, because it's what Angband 2.8.1's town looked like (the
game Zangband was built on), and 4.2 replaced it with something better. What it
takes from Zangband is the part that's a Zangband idea: that the town stands in
a world you can walk out into.

Going down
----------

The town's down staircase works exactly as it does in Angband:
stand on it and press ``>``. It leads into the shallowest dungeon there is, so
a new character has somewhere to go from the first turn.

Dungeon mouths are the other way down. They're scattered across the world,
drawn as a ``>`` like any staircase, and each leads into a dungeon of its own
with its own range of depths. :doc:`dungeon` has the details, but the main
thing is that a dungeon has a bottom, and getting past it means finding another
one. Roads help: every mouth has a road to it from the nearest town, because a
dungeon nobody can find is a dungeon nobody uses.

Coming back up puts you where you went down. The world remembers where you left
it, so a character who walked three days into the hills before finding a way
underground comes back up in the hills, not in the town.

It remembers what you'd explored, too. The surface is taken down while you're
below and rebuilt from the seed when you return, and what you'd learned of it
is carried across, even if you save and reload while you're still underground.
A character who walks into a town, goes down the stairs and comes back up a
week later finds the town as they left it, not blank.

.. _where this is narrower than zangband:

Where this is narrower than Zangband
------------------------------------

Three kinds of townsfolk are missing, and always were. Of Zangband's six, the
three it gave any behaviour to are built (villagers, monsters and abandoned
towns), and ZangbandTK adds beasts as a fourth. The elf, dwarf and lizardfolk
types exist in Zangband's source only as unused constants, so there's nothing
to port and nothing was lost (DEC-31).

Each dungeon has its own depth range, floor, shape, inhabitants and kind of
treasure (see :doc:`dungeon`), but the inhabitants are named by the kinds of
creature that live there (trolls, wraiths, vortices), not by individual
monsters. Zangband set a habitat flag per dungeon on each of its nine hundred
monsters; this says the same thing in a dozen lines per dungeon, and covers the
monsters Angband brought as well, but it can't say that one particular creature
belongs in one particular place.

The terrain is coarser than Zangband's. Zangband distinguished about thirty
kinds of country, including impassable jungle, snow and lava; ZangbandTK
currently has seven. Snow in particular implies a climate axis the parameter
space doesn't have yet.
