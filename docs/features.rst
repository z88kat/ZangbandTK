========
Features
========

ZangbandTK is Angband 4.2.6 with Zangband's character rebuilt on top of it. This
page lists what's in the game now, and what isn't.

.. grid:: 1 2 2 2
   :gutter: 3

   .. grid-item-card:: Base

      Angband 4.2.6

   .. grid-item-card:: Platforms

      macOS, Windows, Linux, DOS, Nintendo DS, 3DS and the browser.

      Developed and played on macOS. CI builds the rest and smoke-tests all
      but the Nintendo ones, but none of them is played through. See
      :doc:`download`, or :doc:`play` to start without installing anything.

   .. grid-item-card:: Playable

      Yes

   .. grid-item-card:: Savefiles

      Not compatible with Angband or Zangband, and never will be. Compatible
      across ZangbandTK versions: a character saved by an older build loads
      into a newer one.


In the game now
===============

Zangband's lethality
--------------------

Every monster has 73% of Angband's hit points and 50% of its armour class.
Monsters die sooner and are easier to hit, so fights are over in fewer turns,
whichever way they were going.

The numbers were measured from Zangband itself. Across the 450 monsters that
Zangband 2.7.5 and Angband 2.8.1 have in common, Zangband's had a median 0.73×
the hit points and 0.50× the armour class of the release it forked from.
:doc:`balance` has the full story.

A wilderness
------------

The wilderness was Zangband's defining feature, and it's the largest piece of
work in the game so far. The world is 2064 grids square, generated from a seed
and never stored, with a dozen towns standing in it, roads between them, and
thirteen dungeons opening off it.

- Terrain follows from height, population and law; danger follows from law alone.
- The surface is one continuous sheet that scrolls as you walk, not a set of
  discrete levels.
- Deep water can be waded into, and drowned in.
- Rivers run to the sea and lakes sit in the hollows.
- The world ends in open sea.
- What you drop in the country stays where you left it until somebody finds it,
  and the world forgets what you left in time.
- A unique you wounded and walked away from is still out there, still wounded.
- A dozen towns in four sizes, named from Amber, with different trades
  depending on the country around them (:doc:`towns`). Some have fallen: held
  by monsters, taken back by animals, or just standing empty. The sidebar tells
  you which, because a fallen town keeps its shops and none of its services.
- Roads join the towns *and every dungeon mouth*, routed round the mountains
  and out of the swamps, so following one always gets you somewhere.
- Thirteen dungeons, Amber's own places, each covering its own range of depths
  and ending at the bottom of it. Going deeper means crossing the world to find
  one that reaches deeper (:doc:`dungeon`).
- Each dungeon has its own inhabitants and its own kind of treasure. The
  Caverns of Kolvir are trolls and giants and tools; Tir-na Nog'th is wraiths
  and vortices and three times the usual magic.

Towns worth walking to
----------------------

A town is more than a row of shop doors (:doc:`towns`).

- There are six services. A magetower carries you to places you've been, for a
  fare by distance, and an inn sells a bed until morning. There's a healer, a
  magesmith and a recharger, and in a great city only, a chaos tower that takes
  one mutation off you and lets you choose which. Which of them a place has
  depends on its size, and a town that has fallen has none.
- A night at the inn may show you something: a place you haven't found, put on
  your map, or a bad dream about something you've met. Which one depends on how
  well governed the town is.
- Shops come at a standard as well as a trade: a plain Weapon Smiths, or an
  Advanced, Expert or Arcane one, deeper-stocked and better enchanted. About two
  in three are plain and one in forty is arcane, so the good ones are worth
  travelling for.
- Every great city and the most crowded cities have a Statue Store, as Zangband
  had. It sells likenesses of monsters in wood, clay, stone, iron, copper,
  silver, gold, ivory and mithril, which do nothing and are worth their
  material. Statues turn up in the dungeon from level 9, and the general store
  and the black market will buy them too.
- The cities whose country is magical have a Figurine Store, which sells
  magical figurines. Each is a monster that becomes your pet when you throw it.
- There's one house. Every town has a home and they're all the same home, so
  what you leave in your village is on the shelf in a city four days away.

Quests
------

Work you're offered, accept, carry about and hand back (:doc:`quests`).

- Six kinds: a bounty on a creature, a killing at a named depth of a named
  dungeon, a killing in the open country, carrying word to another town, going to
  look at a place nobody here has been, and fetching a particular thing.
- Quest-giving is a property a building has, not a building of its own. The inn
  has it, and moving it to the magetower is a line of code.
- Press ``J`` for what you've taken on. Give up anything you can't finish by
  walking back in and handing it back.
- Unlike Angband, the game doesn't end with Sauron and Morgoth. It ends in
  Amber: Oberon, and then the Serpent of Chaos, at the bottom of the Courts of
  Chaos.

Monsters, objects and magic
---------------------------

- 387 imported monsters, including the princes of Amber and the Mythos
  deities, out of 1013 in total.
- 51 artifacts, including Grayswandir and Frakir.
- 16 ego types, including ``(Vampiric)``, ``(Chaotic)`` and
  ``(Trump Weapon)``.
- Three weapon mechanics Angband has no equivalent of: vampiric, vorpal and
  chaotic.
- The Ancient and Foul Curse, with its cascade intact.
- Random object abilities, pit themes and vaults.
- The Unicorn of Amber, who makes you whole once and is then gone about her
  business, and a white deer that heals you and bounds away. Not everything in
  the world is there to be fought.
- The lotus, a mushroom that takes everything you know five turns after you eat
  it: the map, the world, the monsters, what your potions are, and the spells
  you'd learned.

See :doc:`monsters` and :doc:`objects` for the detail.

Races, classes and racial powers
--------------------------------

- Twenty-eight races, up from Angband's eleven, among them the Amberite,
  Draconian, Vampire, Mindflayer, Golem, Sprite, Half-Titan, Yeek and Beastman.
  They're curated, not imported wholesale, and four of Zangband's were turned
  down on the evidence. The list is ``lib/gamedata/p_race.txt``.
- Experience cost varies a lot by race. Angband flattened nearly every race to
  120 per cent; Zangband ran 100 to 255 and priced races with it. This game
  uses Zangband's figures, so a Half-Titan is slow to level.
- Racial powers, on ``N``, which Angband doesn't have. Twenty-two of the
  twenty-eight races can do something no class teaches, from a Vampire drinking
  blood at level 5 to an Amberite walking the Pattern at 40. They're paid for in
  mana, or in hit points where there's none.
- The Monk, and martial arts with it: seventeen unarmed techniques, eight
  strikes a turn at the top, and the only class in the game that's punished for
  wearing armour. Angband's answer to an empty weapon slot is one point of
  damage a blow. Measured over a hundred blows, a Monk does about fifty times as
  much.
- The Chaos-Warrior, the only class in the game that belongs to somebody. It's
  sworn at birth to one of nine Lords of the Courts of Chaos, and at every level
  gained the Lord decides how it feels: a healing, a raised stat, an object out
  of nothing, or a cursed weapon and monsters on top of you. Level 13 is three
  times as dangerous as any other, and nothing in the game warns you.
- The Mindcrafter, and psionics: twelve powers that arrive by level instead of
  out of a book, most of which grow into something else as the character does.
  With them comes a kind of damage Angband doesn't have, psionic force, which
  asks whether there's a mind there to hurt and does nothing at all to a golem.
- Book burning. A Warrior is paid experience for burning any rare spellbook,
  a Paladin of Life for any but Life's, and a Paladin of Death for Life's
  alone. Zangband paid for it when you destroyed one. Angband can't destroy
  anything, so the burning is a choice in the ignore menu.

See :doc:`characters` for what's different, and :doc:`birth` for the tables.

What chaos makes of you
-----------------------

- Ninety-six mutations, which Angband has nothing like. They aren't chosen and
  they aren't worn: they happen to you, from a Chaos-Warrior's patron, a
  polymorph, a chaos attack, or just being a Beastman.
- There are four kinds. Activatable ones are powers you invoke and pay for,
  continuous ones are always in effect, random ones fire on their own, and
  melee ones add a blow to your attack round.
- They're rarely only what they're called. Superhuman strength is +4 STR and
  also -1 INT and -1 WIS; iron skin is +25 AC and -3 DEX. The Zangband spoiler
  gives the headline of each and stops, and the headline is usually the good
  half.
- Cancelling pairs: gaining one sheds whatever contradicts it, so you don't get
  to be both puny and superhumanly strong.
- Polymorph Self, which sheds some of what you have and grows something else,
  and is the one power in the game whose outcome nobody can predict.

They're listed on the third page of your character sheet: press ``C``, then
``h`` twice. See :doc:`mutations` for the whole roster.

The realms of magic
-------------------

- Seven realms where Angband has four: Arcane, Life, Nature, Death, and the
  three Zangband adds, Sorcery, Chaos and Trump. Four of the seven are Angband's
  own, renamed where this game uses Zangband's word for the same thing.
  :doc:`realms` says which, and why one of them isn't quite Zangband's realm of
  the same name.
- You choose at birth, from what your class is entitled to, and the choice is
  permanent. A Mage may take any of them; a Chaos-Warrior takes Chaos and
  nothing else. Your realms decide which spellbooks you can open at all, so a
  Priest of Death can't read a Life book.
- All seven are playable: two hundred and twenty-four workings in twenty-eight
  books, every one of them Zangband's. Sorcery is the realm of knowing and
  going: detection, teleportation, enchantment, *Globe of Invulnerability* at
  the end of it, and not one attack spell in it. Chaos is the destructive one,
  and it backfires. A failed Chaos spell may go off as something else, and how
  bad that is depends on how deep the spell was. *Magic Missile* never
  backfires; *Call the Void* almost always does. Death punishes a miscast too,
  and out of the Necronomicon it can cost you a statistic.
- Arcane is bought outright. All four of its books are sold in town, and that's
  the bargain that makes the weakest realm worth taking.
- Every class studies every realm Zangband allows it, which is more than
  Angband allows: a Mage may take Death or Life, and a Priest may take Arcane.
  Six realms are open to a Mage where Angband gives it one.
- The Chaos-Warrior, the Warrior-Mage, the High-Mage and the Monk all cast,
  which is what those classes were always for. A Warrior-Mage always studies
  Arcane and picks a second realm. A High-Mage picks one realm from all seven
  and gets no second, and is paid for it in better figures. A Monk picks one of
  Life, Nature and Death and keeps its fists.

Trump arrived with pets. Fifteen of its thirty-two spells are summons that
depend on the creature being *yours*, so the realm couldn't ship until monsters
could take sides. It was the last of the seven to go in.

Things that fight for you
-------------------------

This is the largest change to the game's assumptions, and the last big one
(:doc:`pets`). In Angband every monster is an enemy. That's less a rule than an
assumption, threaded through the targeting, the AI, the projections and the
combat.

- There are three sides: hostile, friendly and pet, and they behave
  differently. A friendly creature won't attack you but takes no orders and
  costs nothing, where a pet does both.
- Orders are a policy. Press ``P`` (``!`` in the roguelike keyset). Five
  settings put your animals anywhere from a square away to twenty-five, and two
  switches say whether they may open doors and whether they may pick things up.
  Every pet follows the same policy, and it survives a save.
- They cost mana, not gold. A few are free: one, plus one per so many character
  levels, where the divider is a property of your class. Past that allowance
  the *sum of your pets' levels* is withheld from your mana regeneration. The
  edge is meant to be sharp: one pet over the allowance turns the charge on for
  all of them at once.
- A pet that kills earns you nothing. Experience is for the killing blow, so a
  summoner watching its animals work doesn't level.
- What a pet summons is also yours, and that adds to the upkeep. It's why
  Zangband's own documentation warns about a pet that can make more pets.
- They can turn on you. Catch one in a ball spell, or put on something that
  aggravates, and it stops being yours. The game asks before you aim at one.
- Changing its shape doesn't change its side, though. Polymorph a pet and what
  it becomes is still yours. The new shape is drawn from between its own depth
  and the depth you're standing on, so doing it deep is how a weak pet becomes a
  strong one, and where the dragons come from. That's Zangband's own rule, and
  the upkeep pays for it.
- Four of them follow you downstairs, and each has a one in twenty chance of
  leaving at every level change. That's a median of thirteen or fourteen
  descents, and with a full stable about one descent in five costs you one.
  Zangband deleted every pet at every staircase. This is the softer rule, and
  both numbers are in ``constants.txt``.

They come from charms and summons across Life, Nature, Death and Chaos, a Wand
of Tame Monster, a wall of mould, and the whole Trump realm. Uniques, quest
monsters and anything whose mind can't be confused refuse outright.

Pets and allies
---------------

- A monster can be on your side. Angband has no such idea: every monster is an
  enemy, by assumption and not by any rule. Here allegiance is a field on the
  monster, with three states: hostile, friendly, and yours.
- Nine orders, given as a standing policy for all your pets at once, and they
  follow you downstairs. Zangband left them behind, which punishes you for using
  the feature.
- They cost you. There's an allowance you keep for nothing, then a charge on the
  sum of your pets' levels above it, so a stable of weak animals is cheap and
  one deep ally isn't. A pet leaves you about one time in twenty at a level
  change.
- You can get one from a realm spell that summons or charms, a Wand of Tame
  Monster, a Mindcrafter's *domination* (a mind attack until level 30, and
  wholesale from 30), a Chaos Lord handing you a servant, a magical figurine
  thrown to let out the monster it holds, or anything your own pets summon.
  Trump is the realm of pets, and fifteen of its spells call something that
  serves you. That's all of Zangband's routes; see :doc:`pets`.
- Walking into one changes places with it, and hurting one turns it against
  you, instead of the game asking you to confirm.

Nightmare mode
--------------

An irreversible choice at birth, as unfair as Zangband meant it to be. Its own
description is *"this isn't even remotely fair!"* It's off by default; press
``=`` at the birth menus to find it.

- Monsters have twice the hit points and five more points of speed, and none
  of them is asleep, or waiting for you, when it arrives.
- Sustains fail one time in thirteen, drains stick, and no stairs can be made,
  so neither scumming nor patience is a way out.
- The floor lies. What lives on a level isn't what should, a few walls on every
  level look like floor, a bell tolls before midnight and something answers it,
  and a recall now and again lands you deeper than you asked.
- Four additions of its own, kept apart from the port because Zangband
  described them and never built them: breeders that fill a level two and a
  half times over, reflection that fails twice as often, nastier mutations, and
  shopkeepers who are poor and never leave.

See :doc:`nightmare`, which says what it does and what it doesn't.

Closed stores and small levels
------------------------------

Two more of Zangband's ironman options, chosen at birth like nightmare mode
and kept for the whole game. With stores permanently closed, every shop is
locked, your home too, and you live on what you find. With unusually small
levels, every dungeon level is somewhere between one screen and a little
short of a full level, with fewer monsters and just as much treasure.
Either one, or nightmare, or forced descent, takes away a Golem's immunity to
stunning, as it did in Zangband. See :ref:`the birth options <birth-options>`.

The original's interface
------------------------

The game comes as two Mac applications. ``Zangband.app`` is Angband 4.2's own
front end, where the game draws everything in windows of text or tiles.
``ZangbandTK.app`` brings back ZangbandTk, the Zangband of the late 1990s,
rebuilt on Tcl/Tk 9 and not ported:

- Windows of its own. The map and its panes are in the main one, and the
  character, the items on a paper doll, the knowledge browser, the spell books,
  the shops and the buildings each have theirs. Pointing at an item recalls it,
  pointing at the map says what's there, a click on the map steps that way and
  a double-click walks to the square, and the Choice pane answers the game's
  prompts with a click.
- The original's way in and out: a Choose Game window, a Birth window with a
  portrait for every race and class, the tombstone, the Hall of Fame, a
  character record, and tips at the start.
- The original's menu bar, built from the game's own commands, so every entry
  shows its key in the keyset you play with.
- One Options window for everything: the tile set, the fonts, the size of every
  window's text, and palettes including high contrast and colour-blind, with a
  colour-blind version of the Neon tiles to go with it.
- Sound, and your own music. There are Angband 4.2's sounds, with 38 more for
  events the original sounded and 4.2 didn't, and music from a folder of your
  own, shuffled. Both are off until you turn them on.
- A Keymaps window that draws your own keyboard, and the function keys and
  numeric keypad as game keys.
- The manual inside the game, in a Help window, with F1 opening it at the part
  about whichever window you're in.

Both read the same folder of characters, so you can carry a character from one
to the other. The original ZangbandTk ran on Windows and X11 and never on the
Mac, so on the Mac this is the *new* port, not the old one returning. It runs on
Windows and Linux too, from its own downloads, with sound and music through SDL
and a Keymaps window that draws the keyboard you have. See :doc:`download` and
the Tcl/Tk section of :doc:`customize`.

Tunable without a rebuild
-------------------------

The balance dials live in ``lib/gamedata/constants.txt`` and take effect when
you restart:

.. code-block:: text

   lethality:hit-points:73      # percent of base monster hit points
   lethality:armor-class:50     # percent of base monster armour class
   world:blessing-turns:500     # how long a blessed beast's gift keeps others away
   world:blessing-bound:100     # how far it bounds after being touched, in grids
   melee:vorpal-chance:6        # a vorpal weapon cuts deep on one blow in this many
   melee:vorpal-multiplier:2
   melee:chaotic-chance:7       # a chaotic weapon discharges on one blow in this many

Set both lethality values to ``100`` and the game behaves exactly like vanilla
Angband 4.2. That's a supported setup, and a useful comparison.


Where this differs from Zangband
================================

ZangbandTK isn't a port. Zangband stopped in 2005 at 2.7.5-pre1 and Angband
kept going, so every piece of Zangband that comes here has to be expressed in a
game that has moved twenty years on. Most of it converts intact. Where it
doesn't, the reason is recorded: every judgement below has a numbered entry in
the project's decision log, with the source line it came off and what the
change cost. Five reasons cover almost all of it.

Angband 4.2 cannot say it
-------------------------

The commonest reason, and the least interesting: the mechanism exists in
Zangband and 4.2 has no vocabulary for it.

- Chaos lost three damage types. Zangband's ``hurt_types`` has four
  projections 4.2 has no counterpart for, and Chaos's own spells cast three of
  them. Disintegration damaged *and* removed walls, which 4.2 splits into two
  effects; the other two were mapped onto the nearest thing that exists. Five
  spell radii that grew with the caster's level are frozen, because 4.2's
  radius is a constant in the data. Chaos still arrived whole, with all
  thirty-two spells.
- Blessing a weapon is an effect of its own. 4.2 has a ``BLESSED`` object flag,
  which a Priest fights better with, but no spell that conferred it, so Life's
  *Bless Weapon* waited until there was one. Swapping in a random brand would
  have made it a different spell. It drives out any curse that isn't permanent,
  as Zangband's did, and there's no heavy curse here for it to fail on one time
  in three.
- Nothing dispels demons by name. 4.2 dispels undead, evil, or everything.
  *Exorcism* therefore hits undead by name and demons by being evil, which
  catches more than Zangband's version did.
- A ring of glyphs is one glyph. *Warding True* laid glyphs around the caster;
  4.2's ``GLYPH`` places one, beneath you.
- Nothing jams a door. In 4.2 a door is open, closed, locked or broken and no
  effect reaches the lock, so Arcane's *Wizard Lock* makes a door along the line
  you aim instead of jamming one that's already there. It does the same job a
  different way, and it works in an empty corridor, where the original needed a
  door to work on.
- Telling you everything about an object means every rune on it. 4.2 replaced
  whole-object identification with runes learned one at a time, so Sorcery's
  *Identify True*, Nature's *Stone Tell*, Trump's *Trump Lore* and Life's
  *Holy Vision* teach you every rune on one object at once, where *Identify*
  teaches one. A rune is yours, not the object's, so each is then known on
  everything that has it. That's stronger than Zangband's, which knew only that
  one object.
- Polymorph Self keeps the mutations and drops the race change. Zangband
  rewrote your race permanently. 4.2 has shapechanges, which are better, and a
  character whose race is mangled mid-game breaks things a savefile has to
  believe.

Where a gap was worth closing, it was closed. ``FETCH`` didn't exist in 4.2 and
now does, because telekinesis, Sorcery and Trump all needed it.

The original never finished it
------------------------------

Zangband has features that were written and then not wired up. You wouldn't
have noticed playing it, because nothing showed.

- Virtues. Topi Ylinen wrote them in 1998. By 2.7.5-pre1 there were 168 places
  that wrote a virtue and none that read one, and the screen that would have
  displayed them was commented out with the note *"Display virtues option is
  always left out"*. For seven years, eight counters moved with everything a
  character did, and nothing read them. The selection is kept because that part
  did real work. What reads them is ours: the Lords of Chaos weigh them, and so
  does the dream at the inn.
- The mutation regeneration penalty. The spoiler describes it in detail, but it
  isn't in the source: ``count_mutations()`` has two callers and both are
  prerequisite checks. The documentation describes a version that shipped and
  then lost the feature, so this game doesn't have it either.

Pointing it back at Amber
-------------------------

Zangband began as a game built on Roger Zelazny's *Chronicles of Amber* and
didn't stay one. It picked up Moorcock, then Lovecraft, then Tolkien, then
Warhammer. Steering it back is a project goal, and it applies to content
already imported as much as to what comes next.

- A Chaos-Warrior serves a Lord of the Courts. Zangband's sixteen patrons are
  Moorcock's Elric gods (Slortar, Xiombarg, Arioch) plus Khorne, Slaanesh,
  Nurgle and Tzeentch, who are Warhammer. None of them has any connection to
  Amber. Nine named Lords of the Courts of Chaos replace them.
- The Unicorn of Amber is in the game, and she's Amber's emblem: she makes you
  whole once and is then gone about her business.
- Races are curated, not imported wholesale. Four of Zangband's were turned
  down on the evidence.
- One realm isn't Zangband's. Zangband's Arcane is explicitly the weak
  generalist with no high-level spells, and 4.2's arcane realm is a Mage's main
  line. Folding them together makes Arcane stronger here than it was there, and
  the alternative was two realms sharing one name.

Because it plays better
-----------------------

Some changes are neither conversion nor fidelity. They make the game better.

- Pets follow you downstairs. Zangband left them behind. That was carried over
  at first and then reversed, because a pet you've spent the game charming and
  feeding, abandoned by a staircase, is a punishment for using the feature.
- A power paid for in blood isn't also penalised for it. Both games charge 5%
  failure per point of mana you're short. Both also let a character with no
  mana pay the cost in hit points instead, and those are the same characters,
  so the surcharge fell only on people already bleeding for it. A Draconian's
  breath went from 95% to fail to 7%.
- A blessed beast bounds out of earshot. The white deer heals you once and
  leaves. It used to bound ten grids, which a creature moving at speed 130 and
  hearing at 40 undoes at once, so it came back and stood there being refused.
  Reported from play twice.
- A character who has cheated shows it, in red, on the status line. The game
  always knew; it just never mentioned it.
- Mutations are on the character sheet. They reached the character dump and
  nowhere on screen, which meant the only way to see the passive ones was to
  write a file and read it.

Nobody wrote down what they meant
---------------------------------

In the last group, the original does something, the reason isn't recorded
anywhere, and a judgement had to be made.

- Seven realms, mapped by content. 4.2 has four realms and Zangband names
  seven. Four plus seven is eleven, and two of the names collide. The four that
  overlap are mapped by what they *hold*, not by what they're called.
- Spell experience is per-book. Zangband awards ``5 × book² × level`` and 4.2
  awards ``sexp × level``, so the mechanism transfers exactly. It does leave a
  spread of 0.33× to 5.62× against 4.2's own curve, because Zangband ties the
  reward to which book a spell is in and 4.2 ties it to the spell. It's kept as
  Zangband had it, and the outliers are recorded for playtest, not corrected.
- Classes Zangband never had borrow the figures of the class they match. The
  Druid, Necromancer and Blackguard are Angband's own, so Zangband's spell table
  has no levels or mana for them at all.
- Monster sleepiness was worked out, not assumed. The two games use different
  scales and neither documents the relationship, so it was derived by
  observing 434 monsters the two games share instead of by picking a factor.

Not yet
=======

Only the Tk front end is left, and its part of the manual is already written.

.. list-table::
   :header-rows: 1
   :widths: 26 12 22 40

   * - Feature
     - Milestone
     - Where it stands
     - Chapter
   * - The Tk front end
     - T9
     - Being finished. Ships from the next release, on Windows and Linux as
       well as Apple Silicon Macs.
     - :doc:`customize`

The world is finished. The towns, services, roads, dungeons and quests are all
in, which was Phase 2's world work. The character is finished too. Twenty-eight
races are in, with racial powers for twenty-two of them, and so is every class
Zangband has: the Monk, the Mindcrafter, the Chaos-Warrior, the Warrior-Mage
and the High-Mage, which closes that requirement. Virtues are in too, eight
drawn from eighteen at birth, moved by how you play, and read by the Lords of
Chaos and by the dream at the inn. So are the mutations, all
ninety-six of them, which completes M8.

The realms are in as well: all seven, chosen at birth, each with Zangband's own
thirty-two spells in four books. Trump came last, with pets, because fifteen of
its spells turn on a creature being *yours*. M9 is closed. Pets and
nightmare mode followed, the last on 4 October 2026, so the game itself is
complete.
