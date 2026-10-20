Towns and Services
==================

.. note::

   Milestone M5 is complete, and this chapter covers it: WLD-10 to WLD-12,
   WLD-14 to WLD-18, and WLD-16a to WLD-16c.

   WLD-16d makes quest-giving a property any building can have, instead of a
   building type of its own. It came with the quests in M6, in 3.23.0, and
   `Work`_ below describes it.

There are a dozen places in the world. They differ in how much ground they
cover, how many trades they keep, and which trades those are, so walking to a
town you haven't seen before is worth the road.

Where places stand
------------------

A town needs settled country. The world map has a population and a law value
for every block of it, and places are sited where both run high and the ground
is dry: never in water, never on a mountain, and never within twelve blocks of
another town. Lawless country is where the ruins and the monsters are, and it's
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

The village you begin in is always the smallest place in the world, and every
other place is larger than it.

Which shops a place keeps
-------------------------

Not every place has all ten shops. How many it keeps follows from its size
band; *which* ones it keeps follows from the country around it:

- the armoury and the weaponsmith follow people and order;
- the bookseller gathers where it's safe to gather, so lawless country
  sometimes has none at all;
- the alchemist follows population;
- the magic shop wants both people and law, which is why it's a city trade;
- the black market keeps out of the light, and appears where law is thin and
  people are many. That's rare, and worth the walk.

There's one more on top of whatever else a place keeps: the **Statue Store**,
which Zangband built where its people were thickest. Every great city has one,
and so does a crowded city, which makes about three in a world. It sells and
buys statues: likenesses of monsters in wood, clay, stone, iron, copper,
silver, gold, ivory and mithril, which do nothing and are worth what their
material is worth. The cheap ones come by the handful. The general store and
the black market will buy a statue too, which helps, since a stone one weighs
fifty pounds.

The **Figurine Store** works the same way. Zangband built it where people and
magic both ran high, so a city or a great city whose country is magical keeps
one. There are two or three in a world, often in the same city as a Statue
Store. It sells and buys magical figurines, which are monsters in miniature
that become your pets when thrown (:doc:`pets`), and its figurines hold
monsters up to about the twenty-sixth level.

Two towns of the same size needn't have the same shops. The choice is made
once, when the world is generated, from the world seed and the town's own
position, so a given world always has the same places in it however you reach
them.

How good a shop is
------------------

Every shop has a standard as well as a trade, and the standard is on the sign:
a plain **Weapon Smiths**, or an **Advanced**, **Expert** or **Arcane** one.
The three rungs above plain make a difference:

- the goods are drawn from the deeper end of what that trade sells;
- there's better magic on them, and that's the biggest difference: an arcane
  shop's shelves have roughly three times the plusses of a plain one's;
- and the shelves are fuller, so there's more to choose from.

Which rung a shop gets comes from the country the town stands in, scored once
when the world is made. Magic mostly decides it. For a magic shop or a
bookseller it's almost entirely magic; for arms and armour it's people and
order first with magic behind them, since the top of that ladder is enchanted
steel. A general store climbs on people alone. There's no arcane bread.

The high rungs are rare. Measured across a great many worlds, about two shops
in three are plain and one in forty is arcane, so the top of the ladder is
somewhere you travel to.

The Statue Store only ever stands in a crowded place, so its rung comes from
the country's magic. About half are plain, and the Expert and Arcane ones
(Zangband's *Unusual* Statue Store) are where the silver, gold, ivory and
mithril are. The Figurine Store stands only where it's crowded and magical
both, so its standard is mostly the luck of the place: about half are plain,
and the Expert and Arcane ones (Zangband's *Expensive* Figurine Store) reach
monsters of the forties and fifties.

There are two exceptions. The black market has no rung, because it's already
the top of every ladder: it'll sell you anything, pitched at how deep you've
been. And your home village is plain, whatever the country around it says. A
character who happened to start next to an arcane weaponsmith would be playing
a different game, and wouldn't have chosen it.

A trade's shelves are restocked when you carry your custom to a *different*
town, at that town's standard. Walking out of a shop and back in doesn't change
what's in it.

Your home village
-----------------

The village you start in is the one exception (WLD-12). Its shops are fixed: a
general store, an alchemist, a bookseller and your home. That's what nobody can
begin without: food and light, a potion of cure light wounds, somewhere to
leave what won't fit in your pack, and the next book of your magic. A caster
starts with the first book of each realm it studies, and the bookseller has the
second.

It has no armoury and no weaponsmith. Those are in the next town along, and
finding them is the first thing the world asks of you.

Your house
----------

There's one house, and every town's front door opens onto it. Every town in the
world keeps a home, whatever else it does or doesn't have, and they're all the
same house. Leave a sword in your village, walk four days to a great city, open
the door there, and the sword is on the shelf.

This is the one place where a shop and your house behave differently. A
*shop's* shelves are restocked when you carry your custom to another town,
because a weaponsmith in another town is another weaponsmith. Your house isn't
another house. A house per town would strand your spare gear in whichever
village you happened to be standing in when you outgrew it, days of walking
away, with nothing in the game to remind you which village it was. So the house
travels with you, and the only cost of walking away from it is the walk back.

What they are called
--------------------

Every town has a name, and the status line shows it while you're inside its
walls. Angband showed ``Town`` there, which in this game was wrong nearly
everywhere it appeared: depth zero is the whole world, so it said the same
thing a thousand grids out in open country as it did in a market square.
Outside a town it says ``wilderness``; inside one it says the name.

The name tells you something. A place in governed country takes its name from
one list, and a place that has fallen or stands empty takes it from another, so
*Sawall* isn't somewhere to go shopping. No two towns in a world share a name,
and a world always calls its towns the same thing, since the names come from
its seed and aren't stored in the savefile.

.. note::

   Zangband generated its town names: an elvish name from Angband's syllable
   tables with a size suffix hung on it (``-ville`` for a hamlet, then ``Dun``,
   ``-ton``, ``-ford``). That's a name generator for Middle-earth, and DEC-30
   points this game at Amber, so the names are curated in
   ``lib/gamedata/town.txt`` instead.

   That file records where they came from. The agreed reference for the
   setting has almost no place names (Amber, Castle Amber, Tir-na Nog'th, the
   Forest of Arden, Avalon, the Courts of Chaos), and four of those are
   already dungeons. So only *Avalon* is confirmed from it. The rest are
   believed to be Zelazny's, from the Chronicles, but haven't been checked
   against the books. Nothing in the list is invented. If one of them is wrong
   it's wrong in that one file, and deleting the line fixes it.

Who lives there
---------------

A town isn't always people.

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

Law mostly decides it: a town in country nobody polices has more often than not
been taken. Population comes second, and land that can barely support a town is
land the animals get back. A larger town holds out longer, so a great city is
much less likely to have fallen than a hamlet. That's the obvious way round,
and it's also the one that costs you least: a village of four trades is a
nuisance to lose, but the only great city within reach takes the magic shop and
the black market with it.

Your home village always has people in it. That isn't left to the dice,
because the opening shouldn't depend on luck.

The sidebar tells you. Under the place's name it says what kind of place it is
(``village``, ``town``, ``city``, ``great city``), and if it has fallen it says
so in red: ``city, taken``, ``town, wild``, ``village, empty``. Read it before
you walk the streets looking for an inn that was never there.

A town that has been taken still trades. Its shops are doors in walls rather
than shopkeepers, so you can walk in past whatever is holding the place. That's
either a convenience or an oddity, depending on how you look at it.

Services
--------

Services are buildings with something behind the door, as against shops, which
sell things. There are six. The magetower is a ``9``; the other five are each a
``+`` in a colour of its own. All of them charge.

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
     - A bed until morning, for 25 gold. Only sells you one at night;
       otherwise the innkeeper will tell you to travel while it's light. And
       you may dream. See below.
   * - Healer
     - town and up
     - Binds wounds (4 gold a hit point), cures poison, cuts, stunning,
       blindness and confusion (60), restores drained stats (400), restores lost
       levels (400). Only offers what you need.
   * - Magesmith
     - city and up
     - Puts magic on a weapon or a suit of armour, 250 gold a go. Asks which
       first: a weapon gets both its to-hit and its to-damage, which is why it
       costs twice what a scroll of one or the other does.
   * - Recharger
     - city and up
     - Puts charges back in a wand or a staff, 120 gold, at the same strength
       as a scroll of Recharging. It can still fail and destroy the item.
       That's the ordinary recharging risk, not a swindle.
   * - Chaos tower
     - great city only
     - Takes one mutation off you, 2500 gold, and you choose which. It's the
       rarest service in the game, since it's the only way to pick: a potion
       of New Life takes the lot. See :doc:`mutations`.

Work
----

Some buildings hand out work. Walk into an inn and the innkeeper asks what
you're after: work, or a bed for the night. Ask about work and you may be
offered a job: kill so many of a creature, come back, and be paid.

Quest-giving is a property a building has, not a building of its own. There's
no "quest giver's hut" in the game; the inn just has the property at the
moment, and it could as easily be the magetower commissioning a retrieval. That
way new sources of work cost a line rather than a new building.

The inn has it because it's where people who have been somewhere else are
sitting. And since a town that has fallen keeps no services at all, the work
dries up exactly where you'd expect it to, without any rule needing to say so.
Every inn is hiring; a village has no inn, and nor does a town that has fallen.

You can carry eight jobs at once, and a ninth is refused. Any building that
hires will tell you how far along you are, or pay you when you're done: twenty
gold for each creature asked for, multiplied by how deep that creature lives,
and never less than twenty in total. See :doc:`quests` for the rest of the
terms.

What you dream at the inn
-------------------------

Some nights you dream, and what you dream depends on where you're sleeping.

A true dream shows you a place. The nearest town or dungeon mouth you haven't
found yet appears on the world map, and you wake knowing its name and where it
stands. That's the reason to pay for a bed on a night you could have walked
through.

For a town, that's all it does. The magetower only carries you to towns you've
stood in, and a dream isn't a visit, so you still have to make the walk, but
now you know which way to walk, which is the hard part. A dungeon mouth is
different. The mages only need to have seen one, and a dreamed one has been
seen, so it goes straight onto their list.

A dark dream sets something hunting you. It'll be something you've met:
whatever you've seen and remember, leaning towards the worst of it. If you hold
your nerve you merely remember the thing in the morning. If you don't, you wake
frightened or confused, and it wears off. A character who has met nothing
memorable yet has nothing to dream about.

Which you get depends on the law of the town. A lawful city is a good place to
sleep and a frontier town isn't:

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

What you bring to the bed counts too. The table above is the law of the place,
and your virtues shift it from there. Enlightenment and Knowledge make a true
dream likelier, since a vision is clearer to somebody who has spent their life
looking, and Unlife and Chance make a dark one likelier. They're added on top
of the chance, and twenty points of virtue are worth one percentage point, so
where you sleep still counts for most. But a scholarly character and a
necromancer sleeping in the same bed aren't having the same night. See
:doc:`virtues`.

Nothing in a town is free. The prices are in ``constants.txt`` and are pitched
so a character of level five feels them and one of level thirty doesn't. The
magetower's fare has the same shape: a service is a use for gold, and gold
stops being scarce.

.. note::

   The healer's "lost levels" come from attacks that drain experience instead
   of hit points: undead touches, nether, a few traps. The message is *"You
   feel your life draining away"*, or *"slipping away"* if you have Hold Life,
   which halves it.

   Since your level is computed from your experience, draining it can take a
   level off you, and with it hit points, spell points and access to spells.
   The game remembers the most experience you've ever had, so the loss is
   recoverable: the healer puts you back to your peak, as a Potion of Restore
   Life Levels does. What it can't give back is experience you never earned.

A town that has fallen has none of these services. Nobody is running a teleport
network or an inn out of somewhere held by monsters, and there's nobody in an
abandoned town to run anything.

.. note::

   The inn earns its keep here in a way it wouldn't in Angband. Resting
   duplicates a command the game already has, but daylight is what reveals the
   overworld, so a character who arrives somewhere at dusk either gropes about
   by lamplight or waits, and waiting a hundred turns at a time with ``R`` is
   bookkeeping.

   Zangband's inn also had a nightmare vision, and the dream described above is
   what became of it. Zangband's was a sanity blast: it drained wits, inflicted
   amnesia and sometimes granted a mutation, in service of the game's Lovecraft
   material, which this game is moving away from. The idea was worth keeping
   and the machinery behind it wasn't (DEC-32, DEC-33).

The magetower
-------------

Some towns keep a tower of the mages, drawn ``9``. Walk into it and they'll
carry you to somewhere you already know, for a fee.

Anything above a village keeps one, unless it has fallen. Nobody runs a
teleport network out of somewhere held by monsters, and there's nobody in an
abandoned town to run one. So towns, cities and great cities have one, and
other villages don't.

Your home village always has one, whatever its size, because every journey
begins at home and a network you can't leave from has one node fewer than it
needs.

In practice that's most of the world's towns: eleven of twelve on a world
measured, the twelfth being abandoned.

.. note::

   Towers were first placed by scoring population and law, the same way the
   trades are, and the scoring was worse than useless because it wasn't
   legible. Measured on a real world, every one of its band-one *towns* scored
   just under the threshold, so a town never had a tower and only cities did,
   and nothing told the player that. It was reported by someone who walked
   into two towns in a row and found no tower in either.

   A rule you can hold in your head beats variation you can't see the shape
   of. You lose the sense that a magetower is a city's privilege, and you gain
   being able to plan a journey.

Your home village is always a destination, from the first turn. Beyond that
there are two different bars:

- a town has to have been *stood in*. Seeing it across a field isn't having
  been there, and the first crossing of the world should stay worth making.
- a dungeon mouth only has to have been *seen*. It's a staircase in a field
  with nothing to be inside of.

The fare is gold, by the distance: ``wild:travel-cost`` in ``constants.txt``
is the fare per block, 15 by default. A twenty-block hop is 300 gold, which is
real money at level five and nothing at level thirty. The network is a
convenience you grow into, not a way past the early game. You can't travel on
credit; if you're short, the mages tell you the price and you walk.

You have to be standing in a tower to use one, so a village with no tower is a
place you walk out of. Getting back from a dungeon mouth is your problem, and
that's what Word of Recall is for.

Walls and gates
---------------

Every place is walled, with gates no more than two grids wide. A gate has a door
on each grid; you open it to pass through, and it swings shut behind you after a
while. Monsters don't walk in through an open gate as a matter of course, but a
gate you leave standing open is a gate.

Roads
-----

The towns are joined by roads, and following one is how you find the next place.

A road takes the cheapest way across the country, so it runs down the valleys,
keeps out of the swamp and goes round a mountain instead of over it. It keeps
to the land, too. If two towns end up on either side of an inland sea a
causeway is built, but that's the exception. Roads don't take short cuts
across bays.

Every town and every dungeon is on the network. The roads are laid as a
spanning tree over the towns first, so there's always a road out of your home
village and following it reaches every other town. Any two towns within thirty
blocks of each other then get a road directly between them, which turns the
tree into a network in settled country. Finally every dungeon mouth gets a spur
to the nearest town.

Measured before the spurs existed, six of the thirteen dungeon mouths happened
to sit on a road and the rest were between eleven and sixty-two blocks from
one: up to a thousand grids of open country to search with nothing to follow.
Better siting can't fix that, because a dungeon stands in the kind of country
it belongs in, and the deep ones belong a long way from any town. So the road
goes to them instead.

A road is two grids wide. One grid is too few: where a road turns a right angle
in the block you're standing in, a single-grid corner is one square of floor at
right angles to the way you're going, and the road reads as though it stopped.
The first report of this was a road that "appears to end at the beach" after a
long walk, when in fact it had turned. Two grids makes a corner a two-by-two
block of paving, which reads as a turn. Three was tried first and looked like a
motorway.

About three blocks in a hundred have a road through them.

If you're lost, find a road and walk along it. It goes somewhere: a town, or a
way down.

Finding your way
----------------

Press ``M`` on the surface for the world map. It shows only the country you've
travelled near, and colours each place by its band (village, town, city, great
city), so you can tell from across the map which way to walk for a magic shop.
Roads you've seen are drawn on it too.

What you've explored is remembered while you're down in the dungeon, so coming
back up puts you in a town you already know.
