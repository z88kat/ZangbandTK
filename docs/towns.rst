Towns and Services
==================

.. note::

   **Partly written.** The places themselves are in (WLD-10, WLD-11 size bands,
   WLD-11a, WLD-12); the inhabitant types and the Zangband buildings are filled
   in by the rest of milestone **M5**. Per DEC-17 a milestone is not complete
   until its manual chapter is.

   Requirements still to cover here: WLD-11 inhabitant types, WLD-14 to WLD-18,
   WLD-16a to WLD-16d.

The world holds a dozen places, not one. They differ in how much ground they
cover, how many trades they keep, and which trades those are, so that walking to
a town you have not seen before is worth the road.

Where places stand
------------------

A town needs settled country. The world map carries a population and a law value
for every block of it, and places are sited where both run high and the ground is
dry -- never in water, never on a mountain, and never within twelve blocks of
another town. Lawless country is where the ruins and the monsters are, and it is
mostly empty of towns for that reason.

The size bands
--------------

Four sizes, keyed on the population of the country around them:

.. list-table::
   :header-rows: 1
   :widths: 18 16 66

   * - Band
     - Size
     - What it is
   * - Village
     - 66 x 22
     - A hamlet. Three or four trades, no more.
   * - Town
     - 88 x 26
     - Five or six trades, and always more than the village you started in.
   * - City
     - 110 x 30
     - Six or seven. The magic shop is usually here.
   * - Great city
     - 132 x 34
     - Seven or eight. The black market, when there is one, is here.

The village you begin in is the smallest place in the world, deliberately. It is
the reference the others are larger than.

Which shops a place keeps
-------------------------

Not every place carries all eight shops. How many it keeps follows from its size
band; *which* ones it keeps follows from the country around it:

- the **armoury** and the **weaponsmith** follow people and order;
- the **bookseller** gathers where it is safe to gather, so lawless country
  sometimes has none at all;
- the **alchemist** follows population;
- the **magic shop** wants both people and law, which is why it is a city trade;
- the **black market** keeps out of the light, and appears where law is thin and
  people are many -- which is rare, and worth the walk.

Two towns of the same size need not hold the same shops. The choice is made once,
when the world is generated, from the world seed and the town's own position, so
a given world always has the same places in it however you reach them.

Your home village
-----------------

The village you start in is the one exception (WLD-12). Its shops are fixed
rather than drawn: a **general store**, an **alchemist**, a **bookseller** and
your **home**. That is what nobody can begin without -- food and light, a potion
of cure light wounds, somewhere to leave what will not fit in your pack, and a
spellbook, since no class starts with one.

It has no armoury and no weaponsmith. Those are in the next town along, and
finding them is the first thing the world asks of you.

What they are called
--------------------

Every town has a name, and the status line shows it while you are inside its
walls — where Angband showed ``Town``, which here was wrong nearly everywhere it
appeared: depth zero is the whole world, so it said the same thing a thousand
grids out in open country as it did in a market square. Outside a town it now
says ``wilderness``; inside one it says the name.

The name tells you something. A place in governed country takes its name from one
list, a place that has fallen or stands empty from another — so *Sawall* is not
somewhere to go shopping. No two towns in a world share a name, and a world always
calls its towns the same thing, since the names come from its seed rather than
from the savefile.

.. note::

   **On the names themselves.** Zangband generated them: an elvish name from
   Angband's syllable tables with a size suffix hung on it — ``-ville`` for a
   hamlet, then ``Dun``, ``-ton``, ``-ford``. That is a name generator for
   Middle-earth, and DEC-30 points this game at Amber, so the names are curated
   in ``lib/gamedata/town.txt`` instead.

   The sourcing is recorded honestly in that file. The agreed reference for the
   setting yields almost no place names — Amber, Castle Amber, Tir-na Nog'th, the
   Forest of Arden, Avalon, the Courts of Chaos, and four of those are already
   dungeons. So only *Avalon* is confirmed from it. The rest are believed to be
   Zelazny's, from the Chronicles, but have not been checked against the books.
   Nothing in the list is invented. If one of them is wrong it is wrong in that
   one file, and deleting the line is the whole fix.

Who lives there
---------------

A town is not always people.

.. list-table::
   :header-rows: 1
   :widths: 20 14 66

   * - Inhabitants
     - How often
     - What you find
   * - Villagers
     - about 3 in 5
     - Beggars, drunks, merchants, mercenaries, the odd rogue. An ordinary town.
   * - Beasts
     - about 1 in 4
     - Emptied once, and the animals moved back in. The shops still stand.
   * - Monsters
     - about 1 in 10
     - Taken, and still held. Fewer of them than a town has people, and
       dangerous in proportion to the country around it.
   * - Abandoned
     - about 1 in 20
     - Nobody at all. The streets and the shops are exactly as they were.

Law decides it, mostly — a town in country nobody polices has more often than
not been taken — with population second: land that can barely support a town is
land the animals get back. A **larger town holds out longer**, so a great city is
much less likely to have fallen than a hamlet, which is both the obvious reading
and the one that costs you least: a village of four trades is a nuisance to lose,
the only great city within reach takes the magic shop and the black market with
it.

Your home village always has people in it. That is fixed rather than rolled: the
opening should not depend on luck.

A town that has been taken still trades. Its shops are doors in walls rather than
shopkeepers, so you can walk in past whatever is holding the place — which is
either a convenience or an oddity depending on how you look at it.

The magetower
-------------

Some towns keep a tower of the mages, drawn ``9``. Walk into it and they will
carry you to somewhere you already know, for a fee.

**Where they are.** **Your home village always has one**, whatever its size says.
Every journey begins at home, and a network you cannot leave from has one node
fewer than it needs — without it you would have to walk to a city before you
could travel anywhere at all, every time.

Elsewhere: never in a village, and never in a town that has fallen, since nobody
runs a teleport network out of somewhere held by monsters. Otherwise it is scored
on population and law the way the trades are, so it is the cities that have them —
about a third of towns.

**Where it will take you.** Your home village always counts, from the first turn.
Beyond that, two different bars, deliberately:

- a **town** has to have been *stood in*. Seeing it across a field is not having
  been there, and the first crossing of the world should stay worth making.
- a **dungeon mouth** only has to have been *seen*. It is a staircase in a field
  with nothing to be inside of.

**What it costs.** Gold, by the distance: ``wild:travel-cost`` in
``constants.txt`` is the fare per block, 15 by default. A twenty-block hop is 300
gold, which is real money at level five and nothing at level thirty — which is
the right shape. The network is a convenience you grow into, not a way past the
early game. You cannot travel on credit; if you are short, the mages tell you the
price and you walk.

You have to be standing in a tower to use one, so a village with no tower is a
place you walk out of. Getting back from a dungeon mouth is your problem — that
is what Word of Recall is for.

Walls and gates
---------------

Every place is walled, with gates no more than two grids wide. A gate has a door
on each grid; you open it to pass through, and it swings shut behind you after a
while. Monsters do not walk in through an open gate as a matter of course, but a
gate you leave standing open is a gate.

Roads
-----

The towns are joined by roads, and following one is how you find the next place.

A road is routed rather than drawn: it takes the cheapest way across the
country, so it runs down the valleys, keeps out of the swamp and goes round a
mountain instead of over it. It keeps to the land, too -- if two towns end up on
either side of an inland sea a causeway is built, but that is the exception, not
a short cut across every bay.

Every town **and every dungeon** is on the network. The roads are laid as a
spanning tree over the towns first, so there is always a road out of your home
village and following it reaches every other town. Any two towns within thirty
blocks of each other then get a road directly between them, so settled country
ends up with a network rather than a single thread. Finally every dungeon mouth
gets a spur to the nearest town.

That last part is not decoration. Measured before it existed, six of the thirteen
dungeon mouths happened to sit on a road and the rest were between eleven and
sixty-two blocks from one — up to a thousand grids of open country to search with
nothing to follow. Better siting cannot fix it: a dungeon stands in the kind of
country it belongs in, and the deep ones belong a long way from any town. So the
road goes to them instead.

About three per cent of the world is road.

If you are lost, find a road and walk along it. It goes somewhere — a town, or a
way down.

Finding your way
----------------

Press ``M`` on the surface for the world map. It shows only the country you have
travelled near, and colours each place by its band -- village, town, city, great
city -- so you can tell from across the map which way to walk for a magic shop.
Roads you have seen are drawn on it too.

What you have explored is remembered while you are down in the dungeon, so
coming back up puts you in a town you know rather than one you have to learn
again.
