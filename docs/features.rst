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

      Compatible with neither Angband nor Zangband, and never will be.
      Compatible across ZangbandTK versions: a character saved by an older
      build loads into a newer one.


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

- **Six services**: a magetower that carries you to places you have been for a
  fare by distance, an inn that sells a bed until morning, a healer, a
  magesmith, a recharger, and — in a great city only — a chaos tower that takes
  one mutation off you and lets you choose which. Which of them a place keeps
  follows from its size, and a town that has fallen keeps none.
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
- **16 ego types**, including ``(Vampiric)``, ``(Chaotic)`` and
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

What chaos makes of you
-----------------------

- **Ninety-six mutations**, a mechanism Angband has no equivalent of. They are
  not chosen and not worn: they happen to you, from a Chaos-Warrior's patron, a
  polymorph, a chaos attack, or simply being a Beastman.
- **Four kinds.** Activatable ones are powers you invoke and pay for;
  continuous ones are simply true of you; random ones fire on their own; melee
  ones add a blow to your attack round.
- **Rarely only what they are called.** Superhuman strength is +4 STR and also
  -1 INT and -1 WIS; iron skin is +25 AC and -3 DEX. The Zangband spoiler gives
  the headline of each and stops, and the headline is generally the good half.
- **Cancelling pairs**, so gaining one sheds whatever contradicts it — you do
  not get to be both puny and superhumanly strong.
- **Polymorph Self**, which sheds some of what you have and grows something
  else, and is the one power in the game whose outcome nobody can predict.

They are listed on the third page of your character sheet — press ``C``, then
``h`` twice. See :doc:`mutations` for the whole roster.

The realms of magic
-------------------

- **Seven realms** where Angband has four — Arcane, Life, Nature, Death, and the three
  Zangband adds: **Sorcery**, **Chaos** and **Trump**. Four of the seven are Angband's
  own, renamed where Zangband's word for the same thing is the one this game uses
  (:doc:`realms` says which, and why one of them is not quite Zangband's realm of the
  same name).
- **You choose at birth**, from what your class is entitled to, and the choice is
  permanent. A Mage may take any of them; a Chaos-Warrior takes Chaos and nothing else.
  Your realms decide which spellbooks you can open at all — a Priest of Death cannot read
  a Life book.
- **Six of the seven are playable** — a hundred and ninety-two workings in
  twenty-four books, every one of them Zangband's rather than Angband's. Sorcery is
  the realm of knowing and going: detection, teleportation, enchantment, *Globe of
  Invulnerability* at the end of it, and not one attack spell in it. Chaos is the
  destructive one, and it **backfires**: a failed Chaos spell may go off as something
  else, and how bad that is depends on how deep the spell was. *Magic Missile* never
  backfires; *Call the Void* almost always does. Death punishes a miscast too, and
  out of the Necronomicon it can cost you a statistic.
- **Arcane is bought outright** — all four of its books are sold in town, which is
  the bargain that makes the weakest realm worth taking.
- **Every class studies every realm Zangband allows it**, which is more than
  Angband allows: a Mage may take Death or Life, a Priest may take Arcane. Six
  realms are open to a Mage where Angband gives it one.
- **The Chaos-Warrior, the Warrior-Mage, the High-Mage and the Monk all cast**, which
  is what those classes were always for. A Warrior-Mage always studies Arcane and
  picks a second realm; a High-Mage picks one realm from all seven and gets no
  second, and is paid for it in better figures; a Monk picks one of Life, Nature
  and Death and keeps its fists.

**Trump arrived with pets** — fifteen of its thirty-two spells are summons whose
whole point is that the creature is *yours*, so it could not ship until monsters
could take sides. All seven realms are now playable.

Things that fight for you
-------------------------

The largest change to the game's assumptions, and the last big one (:doc:`pets`).
In Angband every monster is an enemy — not as a rule so much as an assumption
threaded through the targeting, the AI, the projections and the combat.

- **Three sides, not two.** Hostile, friendly and pet behave differently: a
  friendly creature will not attack you but takes no orders and costs nothing,
  where a pet does both.
- **Orders are a policy.** Press ``P`` — ``!`` in the roguelike keyset. Five
  settings put your animals anywhere from a square away to twenty-five, and two
  switches say whether they may open doors and whether they may pick things up.
  Every pet follows the same policy, and it survives a save.
- **They cost mana, not gold.** A few are free — one, plus one per so many
  character levels, and the divider is a property of your class — and past that
  allowance the *sum of your pets' levels* is withheld from your mana
  regeneration. The edge is sharp on purpose: one pet over the allowance turns
  the charge on for all of them at once.
- **A pet that kills earns you nothing.** Experience is for the killing blow, so
  a summoner watching its animals work does not level.
- **What a pet summons is also yours**, which compounds with the upkeep — the
  reason Zangband's own documentation warns about a pet that can make more pets.
- **They can turn on you.** Catch one in a ball spell, or put on something that
  aggravates, and it stops being yours. The game asks before you aim at one.
- **But changing its shape does not change its side.** Polymorph a pet and what
  it becomes is still yours — the new shape drawn from between its own depth and
  the depth you are standing on, so doing it deep is how a weak pet becomes a
  strong one, and where the dragons come from. Zangband's own rule, and the
  upkeep pays for it.
- **Four of them follow you downstairs**, and each has a one in twenty chance of
  leaving at every level change — a median of thirteen or fourteen descents, and
  with a full stable about one descent in five costs you one. Zangband deleted
  every pet at every staircase; this is the softer rule, and both numbers are in
  ``constants.txt``.

Where they come from: charms and summons across Life, Nature, Death and Chaos, a
Wand of Tame Monster, a wall of mould, and the whole of the Trump realm. Uniques,
quest monsters and anything whose mind cannot be confused refuse outright.

Pets and allies
---------------

- **A monster can be on your side.** Angband has no such idea — every monster
  is an enemy, and that is an assumption rather than a rule. Allegiance is a
  field on the monster now, with three states: hostile, friendly, and yours.
- **Nine orders**, given as a standing policy rather than one creature at a
  time, and **they follow you downstairs**. Zangband left them behind, which
  punishes you for using the feature.
- **They cost.** An allowance you keep for nothing, then a charge on the sum of
  your pets' levels above it — so a stable of weak animals is cheap and one deep
  ally is not. A pet leaves you about one time in twenty at a level change.
- **Ways to get one:** a realm spell that summons or charms, a Mindcrafter's
  *domination* (a mind attack until level 30, and wholesale from 30), a Chaos
  Lord handing you a servant, and anything your own pets summon. **Trump is the
  realm of pets** — twelve of its spells call something that serves you.
- **Walking into one changes places with it**; hurting one turns it against you,
  rather than asking you to confirm.

Two of Zangband's routes are still missing, and both want an object rather than
a mechanism: a thrown magical figurine, and a wand of charm monster. See
:doc:`pets`.

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


Where this differs from Zangband
================================

This is not a port. Zangband stopped in 2005 at 2.7.5-pre1 and Angband did not
stop, so every piece of Zangband that arrives here has to be expressed in a game
that has moved twenty years on. Most of it converts intact. Where it does not,
the reason is recorded — every judgement below has a numbered entry in the
project's decision log with the source line it came off and what the change
cost.

Five reasons cover almost all of it.

Angband 4.2 cannot say it
-------------------------

The commonest reason, and the least interesting: the mechanism exists in
Zangband and 4.2 has no vocabulary for it.

- **Chaos lost three damage types.** Zangband's ``hurt_types`` has four
  projections 4.2 has no counterpart for. Disintegration damaged *and* removed
  walls, which 4.2 splits into two effects; the other two were mapped onto the
  nearest thing that exists. Five spell radii that grew with the caster's level
  are frozen, because 4.2's radius is a constant in the data. Chaos still
  arrived whole — thirty-two spells, none dropped.
- **Nothing blesses a weapon.** 4.2 has a ``BLESSED`` object flag and no effect
  that confers it, so Life's *Bless Weapon* is listed and does nothing rather
  than being quietly replaced by a random brand, which is a different spell.
- **Nothing dispels demons by name.** 4.2 dispels undead, evil, or everything.
  *Exorcism* therefore hits undead by name and demons by being evil, which
  catches more than Zangband's version did.
- **A ring of glyphs is one glyph.** *Warding True* laid glyphs around the
  caster; 4.2's ``GLYPH`` places one, beneath you.
- **Polymorph Self keeps the mutations and drops the race change.** Zangband
  rewrote your race permanently. 4.2 has shapechanges, which are better, and a
  character whose race is mangled mid-game breaks things a savefile has to
  believe.

Where the gap was worth closing rather than working around, it was closed:
``FETCH`` did not exist in 4.2 and now does, because telekinesis, Sorcery and
Trump all needed it.

The original never finished it
------------------------------

Zangband has features that were written and then not wired up. You would not
have noticed playing it, because nothing showed.

- **Virtues.** Topi Ylinen wrote them in 1998. By 2.7.5-pre1 there were **168
  places that wrote a virtue and none that read one**, and the screen that would
  have displayed them was commented out with the note *"Display virtues option
  is always left out"*. Eight counters moving with everything a character did,
  for seven years, read by nothing. The selection is kept because that part did
  real work; the consumers are ours — the Lords of Chaos weigh them, and so does
  the dream at the inn.
- **The mutation regeneration penalty.** The spoiler describes it in detail. It
  is not in the source: ``count_mutations()`` has two callers and both are
  prerequisite checks. The documentation describes a version that shipped and
  then lost the feature, so this game does not have it either.

Pointing it back at Amber
-------------------------

Zangband began as a game built on Roger Zelazny's *Chronicles of Amber* and did
not stay one. It picked up Moorcock, then Lovecraft, then Tolkien, then
Warhammer. Steering it back is a project goal rather than a preference, and it
applies to content already imported as much as to what comes next.

- **A Chaos-Warrior serves a Lord of the Courts.** Zangband's sixteen patrons
  are Moorcock's Elric gods — Slortar, Xiombarg, Arioch — plus Khorne,
  Slaanesh, Nurgle and Tzeentch, who are Warhammer. Not one has any connection
  to Amber. Nine named Lords of the Courts of Chaos replace them.
- **The Unicorn of Amber** is in the game, and she is Amber's emblem: she makes
  you whole once and is then gone about her business.
- **Races are curated, not imported wholesale.** Four of Zangband's were turned
  down on the evidence rather than converted for completeness.
- **One realm is deliberately not Zangband's.** Zangband's Arcane is explicitly
  the weak generalist with no high-level spells. 4.2's arcane realm is a Mage's
  main line. Folding them together makes Arcane stronger here than it was
  there, and the alternative was two realms sharing one name.

Because it plays better
-----------------------

Some changes are neither conversion nor fidelity. They are the game being
improved.

- **Pets follow you downstairs.** Zangband left them behind. That was carried
  over at first and then reversed: a pet you have spent the game charming and
  feeding, abandoned by a staircase, is a punishment for using the feature.
- **A power paid for in blood is not also penalised for it.** Both games charge
  5% failure per point of mana you are short. Both also let a character with no
  mana pay the cost in hit points instead — and those are the same characters,
  so the surcharge fell only on people already bleeding for it. A Draconian's
  breath went from 95% to fail to 7%.
- **A blessed beast bounds out of earshot.** The white deer heals you once and
  leaves. It used to bound ten grids, which a creature moving at speed 130 and
  hearing at 40 undoes immediately — so it came back and stood there being
  refused. Reported from play twice.
- **A character who has cheated says so**, in red, on the status line. The game
  always knew; it just never mentioned it.
- **Mutations are on the character sheet.** They reached the character dump and
  nowhere on screen, which meant the only way to see the passive ones was to
  write a file and read it.

Nobody wrote down what they meant
---------------------------------

The last group is the honest one: the original does something, the reason is
not recorded anywhere, and a judgement had to be made.

- **Seven realms, mapped by content and not by name.** 4.2 has four realms and
  Zangband names seven. Four plus seven is eleven, and two of the names
  collide. The four that overlap are mapped by what they *hold* rather than what
  they are called.
- **Spell experience is per-book.** Zangband awards ``5 × book² × level`` and
  4.2 awards ``sexp × level``, so the mechanism transfers exactly — but it
  leaves a spread of 0.33× to 5.62× against 4.2's own curve, because Zangband
  ties the reward to which book a spell is in and 4.2 ties it to the spell. Kept
  as Zangband had it, with the outliers recorded for playtest rather than
  quietly corrected.
- **Classes Zangband never had borrow the figures of the class they match.**
  The Druid, Necromancer and Blackguard are Angband's own, so Zangband's spell
  table has no levels or mana for them at all.
- **Monster sleepiness was recovered, not assumed.** The two games use different
  scales and neither documents the relationship. It was derived by observing 434
  monsters the two games share, rather than by picking a factor.

Not yet
=======

Named here because a features page that lists only what works is a sales
brochure. Each of these has a manual chapter already scaffolded, which is filled
in when the milestone lands.

.. list-table::
   :header-rows: 1
   :widths: 26 12 22 40

   * - Feature
     - Milestone
     - Where it stands
     - Chapter
   * - Nightmare mode
     - M11
     - Not started.
     - :doc:`nightmare`

The world is finished — towns, services, roads, dungeons and quests all landed,
which was Phase 2's world work — and the character is most of the way there.
Nine races, racial powers and **every class Zangband has** are in — the Monk, the
Mindcrafter, the Chaos-Warrior, the Warrior-Mage and the High-Mage, which closes
that requirement. **Virtues have landed** — eight drawn from eighteen at birth,
moved by how you play, and read by the Lords of Chaos and by the dream at the inn
— and so have **mutations**, all ninety-six of them, which completes M8.

**The realms have arrived too**: all seven, chosen at birth, each carrying
Zangband's own thirty-two spells in four books. Trump came last, with pets,
because fifteen of its spells turn on a creature being *yours*. M9 is closed.

A **Tcl/Tk front end** is planned for a later phase, reviving the original's
interface on Tcl/Tk 9. There is an irony in it: the original ZangbandTK
supported Windows and X11 and never supported macOS at all, so the Tcl/Tk front
end will be the new port when it arrives, not the other way round.
