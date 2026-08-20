===========
Release log
===========

.. note::

   **3.1.1 is the first release**, tagged on 18 August 2026 — a macOS disk image
   and a source archive, on the `Releases page`_. What follows is the development
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

M5 in progress: towns and roads — 19 to 20 August 2026
------------------------------------------------------

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
