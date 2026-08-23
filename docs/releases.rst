===========
Release log
===========

.. note::

   **3.1.1 is the first release**, tagged on 18 August 2026 — a macOS disk image
   and a source archive, on the `Releases page`_. It is marked a pre-release, as
   every release will be while the game is early. What follows is the development
   log, grouped by the milestones the work is organised into, newest first, with
   everything done since that tag under *Unreleased*.

   This page covers ZangbandTK only. For Angband's own long history, which this
   game is built on, see :doc:`version`.

.. _Releases page: https://github.com/z88kat/ZangbandTK/releases

Development is tracked by milestone. **M0 to M4 are complete** and M5 is under
way; see :doc:`features` for what that adds up to in the game, and for what the
rest of M5 onward will bring.

Version numbers move with the work — patch for a fix, minor for a feature,
bumped in the commit that does it — so a build can be identified from its title
bar. They begin at 3.0.0, continuing Zangband's own line from 2.7.5-pre1 rather
than the Angband 4.2.6 the code sits on.


Unreleased
==========

M5 in progress: towns, roads and dungeons — 19 to 23 August 2026
----------------------------------------------------------------

- **3.15.0** — PLR-40, and the first thing in this project that is not a port of
  anything: **the lotus**. A mushroom that looks like any other until you have
  eaten one, and five turns after you do, you forget everything — the map under
  your feet, the world map and every place on it, every monster you had learned,
  what all your potions are, and every spell you knew. It takes nothing you
  cannot earn back and nothing comes back quickly, so the cost is hours rather
  than a character. You always still know where home is, because the magetower
  only travels to places you have found and a character who had forgotten
  everywhere would have nowhere to go. The idea is Zelazny's: the first Amber
  novel opens on a man with no memory who knows only that there is a place called
  Amber and that he belongs to it. Also fixes a test that failed about one run in
  five for a correct reason — it walked the window and asserted the other axis
  held still, which is only true if the character did not start near the window's
  edge, and block alignment decides that.
- **3.14.1** — The manual now says that there is only one house. Every town keeps
  a home and they are all the same one, so what you leave in your village is on
  the shelf when you open the door in a city four days' walk away. Asked from
  play, which is fair warning that the machinery reads the other way: a *shop* in
  another town is another shop and is stocked afresh. A house per town would
  strand your spare gear wherever you happened to be standing when you outgrew
  it, so the house travels with you and the only cost of leaving is the walk back.
- **3.14.0** — WLD-15 and WLD-16a: shops come at a standard as well as a trade.
  A plain *Weapon Smiths*, or an *Advanced*, *Expert* or *Arcane* one, written
  above the door. A better shop draws its goods from the deeper end of what that
  trade sells, puts better magic on them — about three times the plusses at the
  top rung — and keeps a fuller shelf. Which rung a town's shop gets is scored
  from the country around it, and mostly from magic, which is a new fourth
  parameter on the world map: population and law were already there, but towns
  are *sited* on law, so law had nothing left to say. Seven shops in ten are
  plain and one in fifty is arcane. Zangband hand-authored 113 building types to
  get this, about 73 of them rungs on a ladder; here it is three records in
  ``quality.txt`` and a score. One thing you may notice as a side effect: a
  trade's shelves are restocked when you carry your custom to a different town,
  so towns no longer all show the same stock. Walking out and back in does not
  re-roll it.
- **3.13.2** — Roads are three grids wide. One grid was invisible where a road
  turned a corner in the block you were standing in, and a road that turns read
  as a road that ends — reported from play as a road that "appears to end at the
  beach" after a long walk. Corners are squared off so a turn looks like a turn.
  Also: a service building could be promised and then demolished. The generator
  places services on lots off the streets and the ruin pass that follows skips
  lots that already have a building — but it only recognised shops, so a ruin
  could be built on top of a freshly built magetower. That is what made the
  magetower go missing from towns that said they had one: about one service in
  ten, and one in six in a village. Services also now have a street cut up to
  them, which previously only shops did.
- **3.13.1** — The healer's "restore your memory" is now "restore your lost
  levels", which is what it actually does — Angband calls it life force, and
  memory was another game's idiom. The manual explains what drains it and what
  losing it costs you.
- **3.13.0** — WLD-16c finished: the inn, the healer, the magesmith and the
  recharger join the magetower. A town has an inn, a healer and a magetower; a
  city adds the magesmith and the recharger; a town that has fallen has none of
  them. All of them charge, with the prices in ``constants.txt``. The inn is the
  one that earns its keep here more than it would in Angband — daylight is what
  reveals the overworld, so a bed until morning beats waiting a hundred turns at
  a time.
- **3.12.2** — The magetower's menu had to be dismissed once for every shop you
  had visited that game. Its handler was registered where the game re-registers
  it on leaving a store, and nothing removes handlers before adding them, so they
  stacked.
- **3.12.1** — Every town above a village keeps a magetower, unless it has
  fallen. It was scored on population and law before, and the scoring turned out
  to put every *town* just under the threshold — so only cities had one, and
  nothing said so. Eleven of twelve towns on a measured world now have one.
- **3.12.0** — The status line says ``Day`` or ``Night`` above ground. Angband
  never said, because its surface is one town-sized level; here daylight is what
  reveals the country, and at night you see as far as your lamp — which is
  indistinguishable from a broken map if nothing tells you the hour. Both cheats
  and the debug commands are now documented, in the manual and in the in-game
  help as page (d).
- **3.11.2** — Your home village always has a magetower now: travel was
  unusable from the one place every journey starts. The status line also refreshes
  the place name as you walk out of a town, where before it kept showing the town
  you had left. And the gold cheat is in the cheat options screen (``=`` then
  ``x``) as well as the debug menu.
- **3.11.1** — Three things found by playing. Roads reached towns but often
  stopped at a blank wall with the gate up to twenty-six grids along it, which is
  a dead end from where you are standing; the gate is now cut where the road
  arrives, and the last stretch of road is drawn up to it. The magetower did not
  offer your own village, because the visited flag was only set by taking a step
  and your steps were taken before the flag existed. And ``^A`` ``$`` puts gold
  in your pocket, for testing things that cost money.
- **3.11.0** — The magetower (WLD-16c), and with it the mechanism services are
  built on (WLD-18): a building is a door with behaviour behind it. Walk into a
  tower and the mages will carry you to a town you have stood in or a dungeon
  mouth you have seen, for a fare that rises with the distance. About a third of
  towns keep one — never a village, never a town that has fallen. The savefile's
  wilderness block goes to version 5; all twenty existing saves still load.
- **3.10.0** — Towns have names, and the status line shows the one you are
  standing in. Where Angband said ``Town`` at depth zero — which here meant the
  whole world, market square and empty moorland alike — it now names the place, or
  says ``wilderness``. A place in governed country and a place that has fallen
  draw their names from different lists, so what somewhere is called tells you
  something before you arrive.
- **3.9.0** — Towns differ in who lives in them (WLD-11): about three in five
  have people, one in four has been emptied and taken back by animals, one in ten
  is held by monsters, and one in twenty stands empty with its shops intact. Law
  decides it, and a large town holds out longer than a hamlet. Your home village
  always has people in it. The download page also caught up with the fact that
  there are Windows, Linux and Nintendo builds now, not just a macOS image.
- **3.8.0** — Every dungeon mouth now has a road to its door. Six of the thirteen
  happened to sit on one; the rest were between eleven and sixty-two blocks away,
  which is up to a thousand grids of open country to search with nothing to
  follow. Better siting could not fix it — the deep dungeons belong a long way
  from any town — so the road goes to them instead.
- **3.7.1** — A code review of the whole milestone, and it found that **eleven of
  the thirteen dungeons could not be entered**: the depth clamp treated "above
  the top of this dungeon" as one case when it is two, so stepping onto the mouth
  of the Abyss asked for depth 1, found it above the top of 90, and returned the
  surface. Also: a dungeon's inhabitants followed the player home into the open
  country, the object theme was defeating Angband's rejection of unreadable
  spellbooks, and the fix for walls behind trees only worked when touching them.
  Fifteen findings in all.
- **3.7.0** — Each dungeon is now home to its own kinds of creature (CNT-05).
  The Caverns of Kolvir run to trolls and giants, Tir-na Nog'th to wraiths and
  vortices, Arden to animals and trees. A stranger from elsewhere turns up now
  and then, at rather less than a fifth of what is met.
- **3.6.0** — Each dungeon yields its own kind of treasure (CNT-12). Rebma is
  rich, Garnath is not, the Grove of the Unicorn runs to magic and the Caverns of
  Kolvir to tools. What a dungeon yields changes; how much does not.
- **3.5.1** — The in-game manual learned the dungeons: a page of its own listing
  all thirteen with their depths, the rule that a dungeon ends at its bottom, and
  how recall behaves per dungeon. The symbol list gained the ``>`` of a dungeon
  mouth out in the open.
- **3.5.0** — Thirteen dungeons (WLD-14), Amber's own places, each covering a
  range of depths and ending at the bottom of it. Each remembers how far down it
  has been explored, so recall returns you to the dungeon you were last in. Each
  has its own floor and its own preferred layout. The savefile's wilderness block
  goes to version 4; older saves still load.
- **3.4.1** — A town wall you were standing next to was invisible if you were
  standing in trees. Angband lights a wall only if the grid between it and you
  carries light onto its face, which assumes anything blocking sight is a wall
  nobody can stand in; ZangbandTK's trees are passable and block sight, so the
  grid you occupied was judged to block the light. Only the stretches of wall
  with grass in front of them lit up, which is what made it puzzling.
- **3.4.0** — What you know of the surface now survives a save made while you are
  underground, not just a dungeon trip within one session. The savefile's
  wilderness block goes to version 3; older saves still load.
- **3.3.1** — The in-game help caught up with the game: it points at
  zangbandtk.com rather than Angband's manual, ``M`` is described as showing the
  world map out of doors, and the symbol list learned the overworld — grass,
  trees, water, mountainside, road — along with a note that not every town holds
  all eight shops.
- Also in 3.3.1: the overworld window scrolls rather than jumping the view
  across. A rebuild re-anchored both axes when only one had asked for it, and
  threw the panel away and chose it afresh, so a long walk west appeared to drop
  the character a dozen rows they had never walked.
- **3.3.0** — Roads (WLD-08). The towns are joined by routed roads that follow
  the valleys and go round the mountains; every town is reachable from the
  starting village along them. The overworld map is also remembered across a trip
  to the dungeon, which it was not before: coming back up put you in a town you
  had to learn again.
- **3.2.1** — Three faults that made a town away from home unusable. Larger towns
  came out as empty fields inside their walls, because every building lot was
  clamped to the starting village's size; no town was drawn at all once the window
  no longer covered home; and about one town in fifty lost its down staircase to
  the gate-cutting.
- **3.2.0** — A dozen places in four sizes (WLD-10, WLD-11, WLD-11a, WLD-12),
  keeping different trades according to the country around them. The starting
  village became the smallest place in the world, deliberately: see DEC-29.
- Releases gained a **Windows zip** beside the disk image: a 32-bit build,
  packaged with the manual and a ``README.txt`` for SmartScreen. Windows had been
  dropped from the release as untested; the mingw cross build, MSBuild, MSYS2 and
  Cygwin all pass now.

3.1.1 — the first release, 18 August 2026
=========================================

Tagged at the end of M5's first day, so everything below it shipped in it: M0 to
M4 complete, and M5 begun. CI builds the release from the tag.

M5 begins: the world map and the front door — 18 August 2026
------------------------------------------------------------

- **3.1.1** — Four faults: the Visual Studio project had never been told about
  the wilderness source, so the one build system that lists its sources by hand
  would not link; no savefile would open, because three additions to the
  wilderness block had all been left at version 1, so its shape could not be told
  from its version; and the world map and the menu each had one of their own.
- **3.1.0** — An overhead map of the world (WLD-25). ``M`` out of doors draws the
  whole world, one character to a block, from the same table the ground is drawn
  from, so what the map calls forest is what you walk into. It pans rather than
  scales, because squeezing 129 rows into 22 would lose the coastlines. In the
  dungeon ``M`` is still the level map.
- **3.0.0** — The game's own name and version on the title screen and in the
  About panel, in place of Angband's and in place of the ``git describe`` string
  the build systems had been handing it, and an icon of its own, cleaned up from
  Zangband's.
- **The town has gates.** The boundary is sealed and four gateways are cut, one
  per side, each two tiles wide with doors that swing shut, in place of the ragged
  holes Angband's starburst clearing happened to leave.
- **Releases are cut from a tag.** CI builds a disk image whose bundle is signed
  ad-hoc and verifies that signature before publishing, with the source archive
  beside it. Ad-hoc signing is not notarization, so the first launch has to be
  allowed by hand once — :doc:`download` says how.
- Fixes: the 14 issues a code review turned up before M5 started, and a window
  scroll that moved the player with it.
- The players' guide became this game's rather than Angband's, and the unused
  Read the Docs configuration went.

Documentation and site — 17 August 2026
---------------------------------------

- The manual became ZangbandTK's rather than Angband's, and moved off
  sphinx-better-theme (unmaintained since 2013) onto pydata-sphinx-theme.
- The site is published to `zangbandtk.com <https://zangbandtk.com/>`_ from the
  Sphinx sources on every push, replacing a documentation build that was never
  deployed anywhere.
- The manual's chapters were grouped rather than left as 32 top-level entries.
- Renaming was scoped to the site's identity. Angband is still named throughout
  the manual body where the reference is correct, and the credits and licence
  pages are unchanged.

M4: the wilderness — 16 to 17 August 2026
-----------------------------------------

Zangband's defining feature, and the largest piece of work so far.

- **The world map**: fractal terrain over a parameter space, 2064 grids square,
  generated from a seed and never stored.
- **One continuous surface**, not a set of levels, scrolling as the player walks.
- **The town stands in the world**, with roads out of it, and you can walk out.
- **Rivers run to the sea and lakes sit in the hollows** (WLD-08).
- **Deep water is crossed, not walled off** — it can be waded and drowned in —
  and the world is not known in advance.
- **The world ends in sea**, not in a wall.
- **The wilderness is inhabited**, and the world is Zangband-sized (CNT-05).
- **The world remembers what you left, and forgets it in time** (WLD-04). A
  unique you wounded and left is still out there (WLD-04b).
- Zangband's own wilderness glyphs, rather than Angband's.
- Fixes: the crash on quit, the missing townspeople, and 4.2's town rock kept.
  Farmer Maggot turned up three faults, one of them serious.
- WLD-05 and WLD-06 were withdrawn, along with the code WLD-05 had asked for.

M3: curses and vaults — 15 August 2026
--------------------------------------

- **The Ancient and Foul Curse**, with its cascade intact, and random object
  abilities.
- Pit themes and vaults — including a finding about Zangband's vaults.

M2: the bestiary and the loot — 15 August 2026
----------------------------------------------

- **389 monsters** imported from Zangband, including the princes of Amber and the
  Mythos deities, bringing the total to 1013.
- **51 artifacts**, all of them, including Grayswandir and Frakir.
- **18 ego types**, all of them, including ``(Vampiric)``, ``(Chaotic)`` and
  ``(Trump Weapon)``.
- **Vampiric, vorpal and chaotic weapons** — three mechanics Angband has no
  equivalent of.
- Manual chapters for monsters and objects.

M1: lethality — 15 August 2026
------------------------------

- **The lethality scalar**: every monster at 73% of Angband's hit points and 50%
  of its armour class, the measured difference between Zangband 2.7.5 and the
  Angband it forked from. This was the first build that played as something other
  than Angband.

M0: foundations — 15 August 2026
--------------------------------

- Project identity, the ``tools/zconv`` data conversion tooling, and the manual
  scaffolding.
- The project was renamed to ZangbandTK on 16 August 2026.
