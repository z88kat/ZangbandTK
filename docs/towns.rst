Towns and Services
==================

.. note::

   **Written.** Milestone **M5** is complete and this chapter covers it: WLD-10
   to WLD-12, WLD-14 to WLD-18, and WLD-16a to WLD-16c.

   One requirement is deliberately outstanding. **WLD-16d** makes quest-giving a
   property any building may carry rather than a building type of its own; it is
   designed and nothing here forecloses it, but there are no quests to give until
   milestone **M6**, so there is nothing yet to describe.

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

How good a shop is
------------------

A shop is not only a trade, it is a trade at a standard, and the standard is on
the sign: a plain **Weapon Smiths**, or an **Advanced**, **Expert** or **Arcane**
one. Three rungs above plain, and they mean something —

- the goods are drawn from the deeper end of what that trade sells;
- there is better magic on them, which is where the real difference lies: an
  arcane shop's shelves carry roughly three times the plusses of a plain one's;
- and the shelves are fuller, so there is more to choose from.

Which rung a shop gets comes from the country the town stands in, scored once when
the world is made. Magic is what mostly decides it — for a magic shop or a
bookseller almost entirely, and for arms and armour it is people and order first
with magic behind them, since the top of that ladder is enchanted steel. A general
store climbs on people alone; there is no arcane bread.

It is deliberately rare. Measured across a great many worlds, seven shops in ten
are plain and one in fifty is arcane, so the top of the ladder is somewhere you
travel to rather than something your nearest city happens to have.

Two exceptions. The **black market** has no rung, because it is already the top of
every ladder — it will sell you anything, pitched at how deep you have been. And
your **home village** is plain, whatever the country around it says: a character
who happened to start next to an arcane weaponsmith would be playing a different
game, and would not have chosen it.

One consequence worth knowing: a trade's shelves are restocked when you carry your
custom to a *different* town, at that town's standard. Walking out of a shop and
back in does not change what is in it.

Your home village
-----------------

The village you start in is the one exception (WLD-12). Its shops are fixed
rather than drawn: a **general store**, an **alchemist**, a **bookseller** and
your **home**. That is what nobody can begin without -- food and light, a potion
of cure light wounds, somewhere to leave what will not fit in your pack, and a
spellbook, since no class starts with one.

It has no armoury and no weaponsmith. Those are in the next town along, and
finding them is the first thing the world asks of you.

Your house
----------

**There is one house, and every town's front door opens onto it.** Every town in
the world keeps a home, whatever else it does or does not carry — but they are all
the same house. Leave a sword in your village, walk four days to a great city,
open the door there, and the sword is on the shelf.

This is deliberate, and it is the one place where a shop and your house behave
differently. A *shop's* shelves are restocked when you carry your custom to
another town, because a weaponsmith in another town is another weaponsmith. Your
house is not another house. The alternative — a house per town — would strand your
spare gear in whichever village you happened to be standing in when you outgrew
it, days of walking away, with nothing in the game to remind you which village it
was.

So the house travels with you, and the only cost of walking away from it is the
walk back.

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

**The sidebar tells you.** Under the place's name it says what kind of place it
is — ``village``, ``town``, ``city``, ``great city`` — and if it has fallen it
says so in red: ``city, taken``, ``town, wild``, ``village, empty``. Worth
reading before you walk the streets looking for an inn that was never there.

A town that has been taken still trades. Its shops are doors in walls rather than
shopkeepers, so you can walk in past whatever is holding the place — which is
either a convenience or an oddity depending on how you look at it.

Services
--------

Buildings with something behind the door, as against shops, which sell things.
Each is a ``+`` in a colour of its own, and each charges.

.. list-table::
   :header-rows: 1
   :widths: 20 16 64

   * - Building
     - Found in
     - What it does
   * - Magetower
     - town and up
     - Carries you to somewhere you already know. See below.
   * - Inn
     - town and up
     - A bed until morning, for 25 gold. Only sells you one at night — the
       innkeeper will tell you to travel while it is light otherwise. And you
       may dream. See below.
   * - Healer
     - town and up
     - Binds wounds (4 gold a hit point), cures poison, cuts, stunning,
       blindness and confusion (60), restores drained stats (400), restores lost
       levels (400). Only offers what you actually need.
   * - Magesmith
     - city and up
     - Puts magic on a weapon or a suit of armour, 250 gold a go. Asks which
       first: a weapon gets both its to-hit and its to-damage, which is why it
       costs twice what a scroll of one or the other does.
   * - Recharger
     - city and up
     - Puts charges back in a wand or a staff, 120 gold, at the same strength
       as a scroll of Recharging. It can still fail and destroy the item — that
       is the ordinary recharging risk, not a swindle.

Work
----

Some buildings hand out work. Walk into an inn that is hiring and you are
offered a job before you are offered a bed: kill so many of a creature, come
back, and be paid.

Quest-giving is a **property a building carries**, not a building of its own.
Nothing in the game has a "quest giver's hut"; the inn simply has the property
at the moment, and it could as easily be the magetower commissioning a
retrieval. That is deliberate — it means new sources of work cost a line rather
than a new building.

The inn has it for a reason worth knowing: it is where people who have been
somewhere else are sitting. And since a town that has fallen keeps no services
at all, the work dries up exactly where you would expect it to, without any
rule needing to say so. About half the towns in a world are hiring.

You can carry several jobs at once. Any building that hires will tell you how
far along you are, or pay you when you are done.

What you dream at the inn
-------------------------

A bed is not only a way of skipping the night. Some nights you dream, and what
you dream depends on where you are sleeping.

**A true dream shows you a place.** The nearest town or dungeon mouth you have
not yet found appears on the world map, and you wake knowing its name and where
it stands. This is the reason to pay for a bed on a night you could have walked
through.

It shows you where the place is. It does not take you there — the magetower
carries you only to places you have actually stood in, and a dream is not a
visit. You still have to make the walk, but you now know which way to walk, which
is the hard part.

**A dark dream sets something hunting you.** It will be something you have met:
whatever you have seen and remember, leaning towards the worst of it. If you
hold your nerve you merely remember the thing in the morning. If you do not, you
wake frightened or confused, and it wears off. A character who has met nothing
memorable yet has nothing to dream about.

**Which you get depends on the law of the town.** A lawful city is a good place to
sleep and a frontier town is not:

.. list-table::
   :header-rows: 1
   :widths: 40 30 30

   * - Where you sleep
     - A true dream
     - A dark dream
   * - A town at the edge of what is governed
     - about 1 night in 11
     - about 1 night in 4
   * - A well-ordered city
     - about 1 night in 4
     - almost never

Most nights, in either, are just a night. And a town that has fallen has no inn
at all, so the worst country never gets the chance to give you the worst dreams.

Nothing in a town is free. The prices are in ``constants.txt`` and are pitched so
a character of level five feels them and one of level thirty does not, which is
the same shape as the magetower's fare: a service is a use for gold, and gold
stops being scarce.

.. note::

   **On "lost levels".** Some attacks drain experience rather than hit points —
   undead touches, nether, a few traps. The message is *"You feel your life
   draining away"*, or *"slipping away"* if you have Hold Life, which halves it.

   Since your level is computed from your experience, draining it can take a
   level off you, and with it hit points, spell points and access to spells.
   The game remembers the most experience you have ever had, so the loss is
   recoverable: the healer puts you back to your peak, as a Potion of Restore
   Life Levels does. What it cannot give back is experience you never earned.

A town that has fallen has none of them. Nobody is running a teleport network or
an inn out of somewhere held by monsters, and there is nobody in an abandoned
town to run anything.

.. note::

   **The inn earns its keep here in a way it would not in Angband.** Resting
   duplicates a command the game already has — but daylight is what reveals the
   overworld, so a character who arrives somewhere at dusk either gropes about by
   lamplight or waits, and waiting a hundred turns at a time with ``R`` is not
   waiting, it is bookkeeping.

   Zangband's inn also carried a nightmare vision. It is **not** built yet, and
   when it is it will not be Zangband's: what that actually did was a sanity
   blast — draining wits, inflicting amnesia, sometimes granting a mutation — in
   service of the game's Lovecraft material, which this one is deliberately
   moving away from. The dream is worth keeping and the machinery behind it is
   not.

The magetower
-------------

Some towns keep a tower of the mages, drawn ``9``. Walk into it and they will
carry you to somewhere you already know, for a fee.

**Where they are.** Anything above a village keeps one, unless it has fallen —
nobody runs a teleport network out of somewhere held by monsters, and there is
nobody in an abandoned town to run one. So: towns, cities and great cities yes;
other villages no.

**Your home village always has one** whatever its size, because every journey
begins at home and a network you cannot leave from has one node fewer than it
needs.

In practice that is most of the world's towns — eleven of twelve on a world
measured, the twelfth being abandoned.

.. note::

   This was scored on population and law at first, in the same style as the
   trades, and the scoring was worse than useless because it was not legible.
   Measured on a real world, every one of its band-one *towns* scored just under
   the threshold, so a town never had a tower and only cities did — and nothing
   told the player that. Walking into two towns in a row and finding no tower in
   either is how it was reported.

   A rule you can hold in your head is worth more than variation you cannot see
   the shape of. What is lost is the sense that a magetower is a city's
   privilege; what is gained is being able to plan a journey.

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

A road is three grids wide. That is not for looks: a road one grid wide can be
walked straight past. Where one turns a right angle in the block you are standing
in, a single-grid corner is one square of floor at right angles to the way you
are going, and the road reads as though it stopped — the first report of this was
a road that "appears to end at the beach" after a long walk, when in fact it
turned. Three grids wide, with the corners squared off, and a turn looks like a
turn.

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
