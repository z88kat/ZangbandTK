========
Features
========

ZangbandTK is Angband 4.2.6 with Zangband's character rebuilt on top of it. This
page is the honest inventory: what is in the game now, and what is not.

.. grid:: 1 2 2 2
   :gutter: 3

   .. grid-item-card:: Base

      Angband 4.2.6

   .. grid-item-card:: Platforms

      macOS, Windows, Linux, DOS, Nintendo DS, 3DS and the browser.

      Developed and played on macOS; the rest are built and smoke-tested by CI
      but not played through. See :doc:`download`, or :doc:`play` to start
      without installing anything.

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
  country around them, and named from Amber (:doc:`towns`). Some have fallen —
  held by monsters, taken back by animals, or simply standing empty — and the
  sidebar says which, because a fallen town keeps its shops and none of its
  services.
- **Roads** joining the towns *and every dungeon mouth*, routed round the
  mountains and out of the swamps, so following one always gets you somewhere.
- **Thirteen dungeons**, Amber's own places, each covering its own range of
  depths and ending at the bottom of it -- so going deeper means crossing the
  world to find one that reaches deeper (:doc:`dungeon`).
- Each dungeon has **its own inhabitants** and **its own kind of treasure**: the
  Caverns of Kolvir are trolls and giants and tools, Tir-na Nog'th is wraiths
  and vortices and three times the usual magic.

Towns worth walking to
----------------------

A town is somewhere to do things, not a row of shop doors (:doc:`towns`).

- **Five services**: a magetower that carries you to places you have been for a
  fare by distance, an inn that sells a bed until morning, a healer, a magesmith
  and a recharger. Which of them a place keeps follows from its size, and a town
  that has fallen keeps none.
- **A night at the inn may show you something** — a place you have not found,
  put on your map, or a bad dream about something you have met. Which, depends on
  how well governed the town is.
- **Shops come at a standard as well as a trade**: a plain Weapon Smiths, or an
  Advanced, Expert or Arcane one, deeper-stocked and better enchanted. Seven in
  ten are plain and one in fifty is arcane, so the good ones are worth travelling
  for.
- **One house.** Every town has a home and they are all the same home: what you
  leave in your village is on the shelf in a city four days away.

Quests
------

Work you are offered, accept, carry about and hand back (:doc:`quests`).

- **Six kinds**: a bounty on a creature, a killing at a named depth of a named
  dungeon, a killing in the open country, carrying word to another town, going to
  look at a place nobody here has been, and fetching a particular thing.
- **Quest-giving is a property a building carries**, not a building of its own.
  The inn has it; moving it to the magetower is a line of code.
- Press ``J`` for what you have taken on. Give up anything you cannot finish by
  walking back in and handing it back.
- **The game ends in Amber**: Oberon, and then the Serpent of Chaos, at the
  bottom of the Courts of Chaos — not Sauron and Morgoth.

Monsters, objects and magic
---------------------------

- **387 imported monsters**, including the princes of Amber and the Mythos
  deities. 1013 in total.
- **51 artifacts**, including Grayswandir and Frakir.
- **17 ego types**, including ``(Vampiric)``, ``(Chaotic)`` and
  ``(Trump Weapon)``.
- **Three weapon mechanics** Angband has no equivalent of: vampiric, vorpal and
  chaotic.
- **The Ancient and Foul Curse**, with its cascade intact.
- Random object abilities, pit themes and vaults.
- **The Unicorn of Amber**, who makes you whole once and is then gone about her
  business, and a **white deer** that heals you and bounds away. Not everything
  in the world is there to be fought.
- **The lotus**, a mushroom that takes everything you know — the map, the world,
  the monsters, what your potions are, the spells you had learned — five turns
  after you eat it.

See :doc:`monsters` and :doc:`objects` for the detail.

Races, classes and racial powers
--------------------------------

- **Twenty races**, up from Angband's eleven. Amberite, Draconian, Vampire,
  Mindflayer, Golem, Sprite, Half-Titan, Yeek and Beastman. Curated rather than
  imported wholesale, and four of Zangband's were turned down on the evidence.
- **Experience cost as a real dial.** Angband flattened nearly every race to 120
  per cent; Zangband ran 100 to 255 and priced races with it. Zangband's figures
  are kept, so a Half-Titan is genuinely slow to level.
- **Racial powers**, on ``N`` — a mechanism Angband has no equivalent of. Eight
  of the nine new races can do something no class teaches, from a Vampire
  drinking blood at level 5 to an Amberite walking the Pattern at 40. Paid for
  in mana, or in hit points where there is none.
- **The Monk**, and **martial arts** with it: seventeen unarmed techniques,
  eight strikes a turn at the top, and the only class in the game that is
  punished for wearing armour. Angband's answer for an empty weapon slot is one
  point of damage a blow; measured over a hundred blows, a Monk does about fifty
  times as much.
- **The Chaos-Warrior**, which is the only class in the game that belongs to
  somebody. Sworn at birth to one of nine Lords of the Courts of Chaos, and every
  level gained the Lord decides how it feels — a healing, a raised stat, an object
  out of nothing, or a cursed weapon and monsters on top of you. Level 13 is
  three times as dangerous as any other, and nothing in the game says so.
- **The Mindcrafter**, and **psionics**: twelve powers that arrive by level
  rather than out of a book, most of which grow into something else as the
  character does. With them comes a kind of damage Angband has no equivalent of
  — psionic force, which asks whether there is a mind there to hurt and does
  nothing at all to a golem.

See :doc:`characters` for what is different, and :doc:`birth` for the tables.

Tunable without a rebuild
-------------------------

The balance dials live in ``lib/gamedata/constants.txt`` and take effect on
restart:

.. code-block:: text

   lethality:hit-points:73      # percent of base monster hit points
   lethality:armor-class:50     # percent of base monster armour class
   world:blessing-turns:500     # how long a blessed beast's gift keeps others away
   world:blessing-bound:100     # how far it bounds after being touched, in grids
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
   * - Mutations
     - M8
     - :doc:`mutations`
   * - Magic realms
     - M9
     - :doc:`realms`
   * - Two more classes
     - M9
     - :doc:`birth`
   * - Pets and allies
     - M10
     - :doc:`pets`
   * - Nightmare mode
     - M11
     - :doc:`nightmare`

The world is finished — towns, services, roads, dungeons and quests all landed,
which was Phase 2's world work — and the character is under way. Nine races,
racial powers, the Monk, the Mindcrafter and the Chaos-Warrior are in. What is
not: the two classes defined by which magic realms they may choose, which wait
for the realms themselves to arrive. **Virtues have landed** — eight drawn from
eighteen at birth, moved by how you play, and read by the Lords of Chaos and by
the dream at the inn. Next come mutations, which Angband has no mechanism for.

A **Tcl/Tk front end** is planned for a later phase, reviving the original's
interface on Tcl/Tk 9. There is an irony in it: the original ZangbandTK
supported Windows and X11 and never supported macOS at all, so the Tcl/Tk front
end will be the new port when it arrives, not the other way round.
