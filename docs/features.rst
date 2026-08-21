========
Features
========

ZangbandTK is Angband 4.2.6 with Zangband's character rebuilt on top of it. This
page is the honest inventory: what is in the game now, and what is not.

.. grid:: 1 2 2 2
   :gutter: 3

   .. grid-item-card:: Base

      Angband 4.2.6

   .. grid-item-card:: Platform

      macOS on Apple Silicon

   .. grid-item-card:: Playable

      Yes

   .. grid-item-card:: Savefiles

      Compatible with neither Angband nor Zangband, and never will be


In the game now
===============

Zangband's lethality
--------------------

Every monster carries **73% of Angband's hit points** and **50% of its armour
class**. Monsters die sooner and are easier to hit, so fights resolve in fewer
turns — in whichever direction they were going.

Those numbers are measured, not invented. Across the 450 monsters that Zangband
2.7.5 and Angband 2.8.1 have in common, Zangband's carried a median 0.73× the
hit points and 0.50× the armour class of the release it forked from. See
:doc:`balance` for the full account.

A wilderness
------------

Zangband's defining feature, and the largest piece of work in the game so far. A
world **2064 grids square**, generated from a seed and never stored, with a dozen
towns standing in it, roads between them, and thirteen dungeons opening off it.

- Terrain follows from height, population and law; danger follows from law alone.
- The surface is one continuous sheet that scrolls as you walk, not a set of
  discrete levels.
- Deep water can be waded into, and drowned in.
- Rivers run to the sea and lakes sit in the hollows.
- The world ends in open sea rather than in a wall.
- What you drop in the country stays where you left it until somebody finds it,
  and the world forgets what you left in time.
- A unique you wounded and walked away from is still out there, still wounded.
- **A dozen towns** in four sizes, keeping different trades depending on the
  country around them (:doc:`towns`).
- **Roads** joining the towns, routed round the mountains and out of the swamps,
  so following one gets you somewhere.
- **Thirteen dungeons**, Amber's own places, each covering its own range of
  depths and ending at the bottom of it -- so going deeper means crossing the
  world to find one that reaches deeper (:doc:`dungeon`).
- Each dungeon has **its own inhabitants** and **its own kind of treasure**: the
  Caverns of Kolvir are trolls and giants and tools, Tir-na Nog'th is wraiths
  and vortices and three times the usual magic.

Monsters, objects and magic
---------------------------

- **389 imported monsters**, including the princes of Amber and the Mythos
  deities. 1013 in total.
- **51 artifacts**, including Grayswandir and Frakir.
- **18 ego types**, including ``(Vampiric)``, ``(Chaotic)`` and
  ``(Trump Weapon)``.
- **Three weapon mechanics** Angband has no equivalent of: vampiric, vorpal and
  chaotic.
- **The Ancient and Foul Curse**, with its cascade intact.
- Random object abilities, pit themes and vaults.

See :doc:`monsters` and :doc:`objects` for the detail.

Tunable without a rebuild
-------------------------

The balance dials live in ``lib/gamedata/constants.txt`` and take effect on
restart:

.. code-block:: text

   lethality:hit-points:73      # percent of base monster hit points
   lethality:armor-class:50     # percent of base monster armour class
   melee:vorpal-chance:6        # a vorpal weapon cuts deep on one blow in this many
   melee:vorpal-multiplier:2
   melee:chaotic-chance:7       # a chaotic weapon discharges on one blow in this many

Setting both lethality values to ``100`` gives behaviour identical to vanilla
Angband 4.2 — a supported configuration, and a useful comparison.


Not yet
=======

Named here because a features page that lists only what works is a sales
brochure. Each of these has a manual chapter already scaffolded, which is filled
in when the milestone lands.

.. list-table::
   :header-rows: 1
   :widths: 30 15 55

   * - Feature
     - Milestone
     - Chapter
   * - Multiple towns and services
     - M5
     - :doc:`towns`
   * - Quests
     - M6
     - :doc:`quests`
   * - Mutations
     - M8
     - :doc:`mutations`
   * - Virtues
     - M8
     - :doc:`virtues`
   * - Magic realms
     - M9
     - :doc:`realms`
   * - Pets and allies
     - M10
     - :doc:`pets`
   * - Nightmare mode
     - M11
     - :doc:`nightmare`

Multiple dungeons have arrived (:doc:`dungeon`), each with its own inhabitants
and its own kind of treasure. What is left of M5 is the towns' side of the same
idea — the six kinds of townsfolk — and the buildings and services that stand in
them.

A **Tcl/Tk front end** is planned for a later phase, reviving the original's
interface on Tcl/Tk 9. There is an irony in it: the original ZangbandTK
supported Windows and X11 and never supported macOS at all, so the Tcl/Tk front
end will be the new port when it arrives, not the other way round.
